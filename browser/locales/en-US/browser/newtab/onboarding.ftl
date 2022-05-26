# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### UI strings for the MR1 onboarding / multistage about:welcome
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
return-to-amo-add-theme-label = Add the Theme

## Multistage onboarding strings (about:welcome pages)

# Aria-label to make the "steps" of multistage onboarding visible to screen readers.
# Variables:
#   $current (Int) - Number of the current page
#   $total (Int) - Total number of pages
onboarding-welcome-steps-indicator =
  .aria-label = Getting started: screen { $current } of { $total }

# "Hero Text" displayed on left side of welcome screen.
# The "Fire" in "Fire starts here" plays on the "Fire" in "Firefox".
# It also signals the passion users bring to Firefox, how they use
# Firefox to pursue those passions, as well as the boldness in their
# choice to use Firefox over a larger competitor browser.
# An alternative title for localization is: "It starts here".
# This text can be formatted to span multiple lines as needed.
mr1-welcome-screen-hero-text =
  Fire starts
  here

# Caption for background image in about:welcome. "Soraya Osorio" is the name
# of the person and shouldn't be translated.
# In case your language needs to adapt the nouns to a gender, Soraya is a female name (she/her).
# You can see the picture in about:welcome in Nightly 90.
mr1-onboarding-welcome-image-caption = Soraya Osorio — Furniture designer, Firefox fan

# This button will open system settings to turn on prefers-reduced-motion
mr1-onboarding-reduce-motion-button-label = Turn off animations

## Title and primary button strings differ between platforms as they
## match the OS' application context menu item action where Windows uses "pin"
## and "taskbar" while macOS "keep" and "Dock" (proper noun).

# Title used on welcome page when Firefox is not pinned
mr1-onboarding-pin-header = { PLATFORM() ->
    [macos] Keep { -brand-short-name } in your Dock for easy access
   *[other] Pin { -brand-short-name } to your taskbar for easy access
}
# Primary button string used on welcome page when Firefox is not pinned.
mr1-onboarding-pin-primary-button-label = { PLATFORM() ->
    [macos] Keep in Dock
   *[other] Pin to taskbar
}

## Multistage MR1 onboarding strings (about:welcome pages)

# This string will be used on welcome page primary button label
# when Firefox is both pinned and default
mr1-onboarding-get-started-primary-button-label = Get started

mr1-onboarding-welcome-header = Welcome to { -brand-short-name }
mr1-onboarding-set-default-pin-primary-button-label =
  Make { -brand-short-name } my primary browser
    .title = Sets { -brand-short-name } as default browser and pins to taskbar

# This string will be used on welcome page primary button label
# when Firefox is not default but already pinned
mr1-onboarding-set-default-only-primary-button-label =
  Make { -brand-short-name } my default browser
mr1-onboarding-set-default-secondary-button-label = Not now
mr1-onboarding-sign-in-button-label = Sign in

## Title, subtitle and primary button string used on set default onboarding screen
## when Firefox is not default browser

mr1-onboarding-default-header =
    Make { -brand-short-name } your default
mr1-onboarding-default-subtitle = Put speed, safety, and privacy on autopilot.
mr1-onboarding-default-primary-button-label = Make default browser

## Multistage MR1 onboarding strings (about:welcome pages)

mr1-onboarding-import-header = Bring it all with you
mr1-onboarding-import-subtitle = Import your passwords, <br/>bookmarks, and more.

# The primary import button label will depend on whether we can detect which browser was used to download Firefox.
# Variables:
#   $previous (Str) - Previous browser name, such as Edge, Chrome
mr1-onboarding-import-primary-button-label-attribution = Import from { $previous }

# This string will be used in cases where we can't detect the previous browser name.
mr1-onboarding-import-primary-button-label-no-attribution = Import from previous browser
mr1-onboarding-import-secondary-button-label = Not now

mr2-onboarding-colorway-header = Life in color
mr2-onboarding-colorway-subtitle = Vibrant new colorways. Available for a limited time.
mr2-onboarding-colorway-primary-button-label = Save colorway
mr2-onboarding-colorway-secondary-button-label = Not now
mr2-onboarding-colorway-label-soft = Soft
mr2-onboarding-colorway-label-balanced = Balanced
# "Bold" is used in the sense of bravery or courage, not in the sense of
# emphasized text.
mr2-onboarding-colorway-label-bold = Bold

# Automatic theme uses operating system color settings
mr2-onboarding-theme-label-auto = Auto

# This string will be used for Default theme
mr2-onboarding-theme-label-default = Default

mr1-onboarding-theme-header = Make it your own
mr1-onboarding-theme-subtitle = Personalize { -brand-short-name } with a theme.
mr1-onboarding-theme-primary-button-label = Save theme
mr1-onboarding-theme-secondary-button-label = Not now

# System theme uses operating system color settings
mr1-onboarding-theme-label-system = System theme

mr1-onboarding-theme-label-light = Light
mr1-onboarding-theme-label-dark = Dark
# "Alpenglow" here is the name of the theme, and should be kept in English.
mr1-onboarding-theme-label-alpenglow = Alpenglow

onboarding-theme-primary-button-label = Done

## Please make sure to split the content of the title attribute into lines whose
## width corresponds to about 40 Latin characters, to ensure that the tooltip
## doesn't become too long. Line breaks will be preserved when displaying the
## tooltip.

# Tooltip displayed on hover of system theme
mr1-onboarding-theme-tooltip-system =
  .title =
    Follow the operating system theme
    for buttons, menus, and windows.

# Input description for system theme
mr1-onboarding-theme-description-system =
  .aria-description =
    Follow the operating system theme
    for buttons, menus, and windows.

# Tooltip displayed on hover of light theme
mr1-onboarding-theme-tooltip-light =
  .title =
    Use a light theme for buttons,
    menus, and windows.

# Input description for light theme
mr1-onboarding-theme-description-light =
  .aria-description =
    Use a light theme for buttons,
    menus, and windows.

# Tooltip displayed on hover of dark theme
mr1-onboarding-theme-tooltip-dark =
  .title =
    Use a dark theme for buttons,
    menus, and windows.

# Input description for dark theme
mr1-onboarding-theme-description-dark =
  .aria-description =
    Use a dark theme for buttons,
    menus, and windows.

# Tooltip displayed on hover of Alpenglow theme
mr1-onboarding-theme-tooltip-alpenglow =
  .title =
    Use a dynamic, colorful theme for buttons,
    menus, and windows.

# Input description for Alpenglow theme
mr1-onboarding-theme-description-alpenglow =
  .aria-description =
    Use a dynamic, colorful theme for buttons,
    menus, and windows.

# Tooltip displayed on hover of non-default colorway theme
# variations e.g. soft, balanced, bold
mr2-onboarding-theme-tooltip =
  .title = Use this colorway.

# Selector description for non-default colorway theme
# variations e.g. soft, balanced, bold
mr2-onboarding-theme-description =
  .aria-description = Use this colorway.

# Tooltip displayed on hover of colorway
# Variables:
#   $colorwayName (String) - Name of colorway
mr2-onboarding-colorway-tooltip =
  .title = Explore { $colorwayName } colorways.

# Selector description for colorway
# Variables:
#   $colorwayName (String) - Name of colorway
mr2-onboarding-colorway-label = Explore { $colorwayName } colorways.

# Tooltip displayed on hover of default themes
mr2-onboarding-default-theme-tooltip =
  .title = Explore default themes.

# Selector description for default themes
mr2-onboarding-default-theme-label = Explore default themes.

## Strings for Thank You page

mr2-onboarding-thank-you-header = Thank you for choosing us
mr2-onboarding-thank-you-text = { -brand-short-name } is an independent browser backed by a non-profit. Together, we’re making the web safer, healthier, and more private.
mr2-onboarding-start-browsing-button-label = Start browsing

## Multistage live language reloading onboarding strings (about:welcome pages)
##
## The following language names are generated by the browser's Intl.DisplayNames API.
##
## Variables:
##   $negotiatedLanguage (String) - The name of the langpack's language, e.g. "Español (ES)"

onboarding-live-language-header = Choose your language

onboarding-live-language-button-label-downloading = Downloading the language pack for { $negotiatedLanguage }…
onboarding-live-language-waiting-button = Getting available languages…
onboarding-live-language-installing = Installing the language pack for { $negotiatedLanguage }…
onboarding-live-language-secondary-cancel-download = Cancel
onboarding-live-language-skip-button-label = Skip

## Firefox 100 Thank You screens

# "Hero Text" displayed on left side of welcome screen. This text can be
# formatted to span multiple lines as needed. The <span data-l10n-name="zap">
# </span> in this string allows a "zap" underline style to be automatically
# added to the text inside it. "Yous" should stay inside the zap span, but
# "Thank" can be put inside instead if there's no "you" in the translation.
# The English text would normally be "100 Thank-Yous" i.e., plural noun, but for
# aesthetics of splitting it across multiple lines, the hyphen is omitted.
fx100-thank-you-hero-text =
  100
  Thank
  <span data-l10n-name="zap">Yous</span>
fx100-thank-you-subtitle = It’s our 100th release! Thanks for helping us build a better, healthier internet.
fx100-thank-you-pin-primary-button-label = { PLATFORM() ->
    [macos] Keep { -brand-short-name } in Dock
   *[other] Pin { -brand-short-name } to taskbar
}

fx100-upgrade-thanks-header = 100 Thank-Yous
# Message shown with a start-browsing button. Emphasis <em> should be for "you"
# but "Thank" can be used instead if there's no "you" in the translation.
fx100-upgrade-thank-you-body = It’s our 100th release of { -brand-short-name }. Thank <em>you</em> for helping us build a better, healthier internet.
# Message shown with either a pin-to-taskbar or set-default button.
fx100-upgrade-thanks-keep-body = It’s our 100th release! Thanks for being a part of our community. Keep { -brand-short-name } one click away for the next 100.
