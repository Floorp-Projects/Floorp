# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### Strings for the upgrade dialog that can be displayed on major version change.

## New changes screen

# This title can be explicitly wrapped to control which words are on which line.
upgrade-dialog-new-title =
    Say hello to a new { -brand-short-name }
upgrade-dialog-new-subtitle = Designed to get you where you want to go, faster
upgrade-dialog-new-item-menu-title = Streamlined toolbar and menus
upgrade-dialog-new-item-menu-description = Prioritize the important things so you find what you need.
upgrade-dialog-new-item-tabs-title = Modern tabs
upgrade-dialog-new-item-tabs-description = Neatly contain information, supporting focus and flexible movement.
upgrade-dialog-new-item-icons-title = Fresh icons and clearer messages
upgrade-dialog-new-item-icons-description = Help you find your way with a lighter touch.
upgrade-dialog-new-primary-default-button = Make { -brand-short-name } my default browser
upgrade-dialog-new-primary-theme-button = Choose a theme
upgrade-dialog-new-secondary-button = Not now
# This string is only shown on Windows 7, where we intentionally suppress the
# theme selection screen.
upgrade-dialog-new-primary-win7-button = Okay, got it!

## Pin Firefox screen
##
## These title, subtitle and button strings differ between platforms as they
## match the OS' application context menu item action where Windows uses "pin"
## and "taskbar" while macOS "keep" and "Dock" (proper noun).

# This title can be explicitly wrapped to control which words are on which line.
upgrade-dialog-pin-title = { PLATFORM() ->
    [macos] Keep { -brand-short-name } in your Dock
   *[other] Pin { -brand-short-name } to your taskbar
}
# The English macOS string avoids repeating "Keep" a third time, so if your
# translations don't repeat anyway, the same string can be used cross-platform.
upgrade-dialog-pin-subtitle = { PLATFORM() ->
    [macos] Get easy access to the freshest { -brand-short-name } yet.
   *[other] Keep the freshest { -brand-short-name } yet within reach.
}
upgrade-dialog-pin-primary-button = { PLATFORM() ->
    [macos] Keep in Dock
   *[other] Pin to taskbar
}
upgrade-dialog-pin-secondary-button = Not now

## Default browser screen

# This title can be explicitly wrapped to control which words are on which line.
upgrade-dialog-default-title-2 =
    Make { -brand-short-name } your default
upgrade-dialog-default-subtitle-2 = Put speed, safety, and privacy on autopilot.
upgrade-dialog-default-primary-button-2 = Make default browser
upgrade-dialog-default-secondary-button = Not now

## Theme selection screen

# This title can be explicitly wrapped to control which words are on which line.
upgrade-dialog-theme-title-2 =
    Get a clean start with a crisp theme
upgrade-dialog-theme-system = System theme
    .title = Follow the operating system theme for buttons, menus, and windows
upgrade-dialog-theme-light = Light
    .title = Use a light theme for buttons, menus, and windows
upgrade-dialog-theme-dark = Dark
    .title = Use a dark theme for buttons, menus, and windows
upgrade-dialog-theme-alpenglow = Alpenglow
    .title = Use a dynamic, colorful theme for buttons, menus, and windows
upgrade-dialog-theme-keep = Keep previous
    .title = Use the theme you had installed before updating { -brand-short-name }
upgrade-dialog-theme-primary-button = Save theme
upgrade-dialog-theme-secondary-button = Not now
