# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### UI strings for the simplified onboarding / multistage about:welcome
### Various strings use a non-breaking space to avoid a single dangling /
### widowed word, so test on various window sizes if you also want this.

## Welcome page strings

onboarding-welcome-header = Welcome to { -brand-short-name }
onboarding-start-browsing-button-label = Start Browsing
onboarding-not-now-button-label = Not now

## Custom Return To AMO onboarding strings

return-to-amo-subtitle = Great, you’ve got { -brand-short-name }
# <img data-l10n-name="icon"/> will be replaced with the icon belonging to the extension
#
# Variables:
#   $addon-name (String) - Name of the add-on
return-to-amo-addon-title = Now let’s get you <img data-l10n-name="icon"/> <b>{ $addon-name }</b>.
return-to-amo-add-extension-label = Add the Extension

## Multistage 3-screen onboarding flow strings (about:welcome pages)

# The <span data-l10n-name="zap"></span> in this string allows a "zap" underline style to be
# automatically added to the text inside it. { -brand-short-name } should stay inside the span.
onboarding-multistage-welcome-header = Welcome to <span data-l10n-name="zap">{ -brand-short-name }</span>
onboarding-multistage-welcome-subtitle = The fast, safe, and private browser that’s backed by a non-profit.
onboarding-multistage-welcome-primary-button-label = Start Setup
onboarding-multistage-welcome-secondary-button-label = Sign in
onboarding-multistage-welcome-secondary-button-text = Have an account?

# The <span data-l10n-name="zap"></span> in this string allows a "zap" underline style to be
# automatically added to the text inside it. "default" should stay inside the span.
onboarding-multistage-set-default-header = Make { -brand-short-name } your <span data-l10n-name="zap">default</span>
onboarding-multistage-set-default-subtitle = Speed, safety, and privacy every time you browse.
onboarding-multistage-set-default-primary-button-label = Make Default
onboarding-multistage-set-default-secondary-button-label = Not now

# The <span data-l10n-name="zap"></span> in this string allows a "zap" underline style to be
# automatically added to the text inside it. { -brand-short-name } should stay inside the span.
onboarding-multistage-pin-default-header = Start by making <span data-l10n-name="zap">{ -brand-short-name }</span> a click away
onboarding-multistage-pin-default-subtitle = Fast, safe, and private browsing every time you use the web.
# The "settings" here refers to "Windows 10 Settings App" and not the browser's
onboarding-multistage-pin-default-waiting-subtitle = Choose { -brand-short-name } under Web browser when your settings open
# The "settings" here refers to "Windows 10 Settings App" and not the browser's
onboarding-multistage-pin-default-help-text = This will pin { -brand-short-name } to taskbar and open settings
onboarding-multistage-pin-default-primary-button-label = Make { -brand-short-name } My Primary Browser

# The <span data-l10n-name="zap"></span> in this string allows a "zap" underline style to be
# automatically added to the text inside it. "more" should stay inside the span.
onboarding-multistage-import-header = Import your passwords, <br/>bookmarks, and <span data-l10n-name="zap">more</span>
onboarding-multistage-import-subtitle = Coming from another browser? It’s easy to bring everything to { -brand-short-name }.
onboarding-multistage-import-primary-button-label = Start Import
onboarding-multistage-import-secondary-button-label = Not now

# Info displayed in the footer of import settings screen during onboarding flow.
# This supports welcome screen showing top sites imported from the user's default browser.
onboarding-import-sites-disclaimer = The sites listed here were found on this device. { -brand-short-name } does not save or sync data from another browser unless you choose to import it.

# Aria-label to make the "steps" of multistage onboarding visible to screen readers.
# Variables:
#   $current (Int) - Number of the current page
#   $total (Int) - Total number of pages
onboarding-welcome-steps-indicator =
  .aria-label = Getting started: screen { $current } of { $total }

# The <span data-l10n-name="zap"></span> in this string allows a "zap" underline style to be
# automatically added to the text inside it. "look" should stay inside the span.
onboarding-multistage-theme-header = Choose a <span data-l10n-name="zap">look</span>
onboarding-multistage-theme-subtitle = Personalize { -brand-short-name } with a theme.
onboarding-multistage-theme-primary-button-label2 = Done
onboarding-multistage-theme-secondary-button-label = Not now

# Automatic theme uses operating system color settings
onboarding-multistage-theme-label-automatic = Automatic

onboarding-multistage-theme-label-light = Light
onboarding-multistage-theme-label-dark = Dark
# "Firefox Alpenglow" here is the name of the theme, and should be kept in English.
onboarding-multistage-theme-label-alpenglow = Firefox Alpenglow

## Please make sure to split the content of the title attribute into lines whose
## width corresponds to about 40 Latin characters, to ensure that the tooltip
## doesn't become too long. Line breaks will be preserved when displaying the
## tooltip.

# Tooltip displayed on hover of automatic theme
onboarding-multistage-theme-tooltip-automatic-2 =
  .title =
    Inherit the appearance of your operating
    system for buttons, menus, and windows.

# Input description for automatic theme
onboarding-multistage-theme-description-automatic-2 =
  .aria-description =
    Inherit the appearance of your operating
    system for buttons, menus, and windows.

# Tooltip displayed on hover of light theme
onboarding-multistage-theme-tooltip-light-2 =
  .title =
    Use a light appearance for buttons,
    menus, and windows.

# Input description for light theme
onboarding-multistage-theme-description-light =
  .aria-description =
    Use a light appearance for buttons,
    menus, and windows.

# Tooltip displayed on hover of dark theme
onboarding-multistage-theme-tooltip-dark-2 =
  .title =
    Use a dark appearance for buttons,
    menus, and windows.

# Input description for dark theme
onboarding-multistage-theme-description-dark =
  .aria-description =
    Use a dark appearance for buttons,
    menus, and windows.

# Tooltip displayed on hover of Alpenglow theme
onboarding-multistage-theme-tooltip-alpenglow-2 =
  .title =
    Use a colorful appearance for buttons,
    menus, and windows.

# Input description for Alpenglow theme
onboarding-multistage-theme-description-alpenglow =
  .aria-description =
    Use a colorful appearance for buttons,
    menus, and windows.
