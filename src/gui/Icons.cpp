/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Icons.h"

#include <QBitmap>
#include <QIconEngine>
#include <QPaintDevice>
#include <QPainter>
#include <QStyle>

#include "config-keepassx.h"
#include "core/Config.h"
#include "gui/MainWindow.h"
#include "gui/osutils/OSUtils.h"

class AdaptiveIconEngine : public QIconEngine
{
public:
    explicit AdaptiveIconEngine(QIcon baseIcon);
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;
    QIconEngine* clone() const override;

private:
    QIcon m_baseIcon;
};

Icons* Icons::m_instance(nullptr);

Icons::Icons()
{
}

QIcon Icons::applicationIcon()
{
    return icon("keepassxc", false);
}

QString Icons::trayIconAppearance() const
{
    auto iconAppearance = config()->get(Config::GUI_TrayIconAppearance).toString();
    if (iconAppearance.isNull()) {
#ifdef Q_OS_MACOS
        iconAppearance = osUtils->isDarkMode() ? "monochrome-light" : "monochrome-dark";
#else
        iconAppearance = "monochrome-light";
#endif
    }
    return iconAppearance;
}

QIcon Icons::trayIcon(QString style)
{
    if (style == "unlocked") {
        style.clear();
    }
    if (!style.isEmpty()) {
        style = "-" + style;
    }

    auto iconApperance = trayIconAppearance();
    if (!iconApperance.startsWith("monochrome")) {
        return icon(QString("keepassxc%1").arg(style), false);
    }

    QIcon i;
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    if (osUtils->isStatusBarDark()) {
        i = icon(QString("keepassxc-monochrome-light%1").arg(style), false);
    } else {
        i = icon(QString("keepassxc-monochrome-dark%1").arg(style), false);
    }
#else
    i = icon(QString("keepassxc-%1%2").arg(iconApperance, style), false);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    // Set as mask to allow the operating system to recolour the tray icon. This may look weird
    // if we failed to detect the status bar background colour correctly, but it is certainly
    // better than a barely visible icon and even if we did guess correctly, it allows for better
    // integration should the system's preferred colours not be 100% black or white.
    i.setIsMask(true);
#endif
    return i;
}

QIcon Icons::trayIconLocked()
{
    return trayIcon("locked");
}

QIcon Icons::trayIconUnlocked()
{
    return trayIcon("unlocked");
}

AdaptiveIconEngine::AdaptiveIconEngine(QIcon baseIcon)
    : QIconEngine()
    , m_baseIcon(std::move(baseIcon))
{
}

void AdaptiveIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    double dpr = !kpxcApp->testAttribute(Qt::AA_UseHighDpiPixmaps) ? 1.0 : painter->device()->devicePixelRatioF();
#else
    double dpr = !kpxcApp->testAttribute(Qt::AA_UseHighDpiPixmaps) ? 1.0 : painter->device()->devicePixelRatio();
#endif
    QSize pixmapSize = rect.size() * dpr;

    painter->save();
    painter->drawPixmap(rect, m_baseIcon.pixmap(pixmapSize, mode, state));

    if (getMainWindow()) {
        QPalette palette = getMainWindow()->palette();
        painter->setCompositionMode(QPainter::CompositionMode_SourceAtop);

        if (mode == QIcon::Active) {
            painter->fillRect(rect, palette.color(QPalette::Active, QPalette::ButtonText));
        } else if (mode == QIcon::Selected) {
            painter->fillRect(rect, palette.color(QPalette::Active, QPalette::HighlightedText));
        } else if (mode == QIcon::Disabled) {
            painter->fillRect(rect, palette.color(QPalette::Disabled, QPalette::WindowText));
        } else {
            painter->fillRect(rect, palette.color(QPalette::Normal, QPalette::WindowText));
        }
    }
    painter->restore();
}

QPixmap AdaptiveIconEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    QPainter painter(&img);
    paint(&painter, QRect(0, 0, size.width(), size.height()), mode, state);
    return QPixmap::fromImage(img, Qt::NoFormatConversion);
}

QIconEngine* AdaptiveIconEngine::clone() const
{
    return new AdaptiveIconEngine(m_baseIcon);
}

QIcon Icons::icon(const QString& name, bool recolor, const QColor& overrideColor)
{
    QString cacheName =
        QString("%1:%2:%3").arg(recolor ? "1" : "0", overrideColor.isValid() ? overrideColor.name() : "#", name);
    QIcon icon = m_iconCache.value(cacheName);

    if (!icon.isNull() && !overrideColor.isValid()) {
        return icon;
    }

    // Resetting the application theme name before calling QIcon::fromTheme() is required for hacky
    // QPA platform themes such as qt5ct, which randomly mess with the configured icon theme.
    // If we do not reset the theme name here, it will become empty at some point, causing
    // Qt to look for icons at the user-level and global default locations.
    //
    // See issue #4963: https://github.com/keepassxreboot/keepassxc/issues/4963
    // and qt5ct issue #80: https://sourceforge.net/p/qt5ct/tickets/80/
    QIcon::setThemeName("application");
    icon = QIcon::fromTheme(name);
    if (recolor) {
        icon = QIcon(new AdaptiveIconEngine(icon));
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        icon.setIsMask(true);
#endif
    }

    m_iconCache.insert(cacheName, icon);
    return icon;
}

QIcon Icons::onOffIcon(const QString& name, bool recolor)
{
    QString cacheName = "onoff/" + name;

    QIcon icon = m_iconCache.value(cacheName);

    if (!icon.isNull()) {
        return icon;
    }

    const QSize size(48, 48);
    QIcon on = Icons::icon(name + "-on", recolor);
    icon.addPixmap(on.pixmap(size, QIcon::Mode::Normal), QIcon::Mode::Normal, QIcon::On);
    icon.addPixmap(on.pixmap(size, QIcon::Mode::Selected), QIcon::Mode::Selected, QIcon::On);
    icon.addPixmap(on.pixmap(size, QIcon::Mode::Disabled), QIcon::Mode::Disabled, QIcon::On);

    QIcon off = Icons::icon(name + "-off", recolor);
    icon.addPixmap(off.pixmap(size, QIcon::Mode::Normal), QIcon::Mode::Normal, QIcon::Off);
    icon.addPixmap(off.pixmap(size, QIcon::Mode::Selected), QIcon::Mode::Selected, QIcon::Off);
    icon.addPixmap(off.pixmap(size, QIcon::Mode::Disabled), QIcon::Mode::Disabled, QIcon::Off);

    m_iconCache.insert(cacheName, icon);

    return icon;
}

Icons* Icons::instance()
{
    if (!m_instance) {
        m_instance = new Icons();

        Q_INIT_RESOURCE(icons);
        QIcon::setThemeSearchPaths(QStringList{":/icons"} << QIcon::themeSearchPaths());
        QIcon::setThemeName("application");
    }

    return m_instance;
}
