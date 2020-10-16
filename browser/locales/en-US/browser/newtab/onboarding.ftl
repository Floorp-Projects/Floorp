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
onboarding-cards-dismiss =
    .title = Dismiss
    .aria-label = Dismiss

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
onboarding-multistage-theme-primary-button-label = Save Theme
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

## These strings belong to the individual onboarding messages.

## Each message has a title and a description of what the browser feature is.
## Each message also has an associated button for the user to try the feature.
## The string for the button is found above, in the UI strings section

onboarding-tracking-protection-title2 = Protection From Tracking
onboarding-tracking-protection-text2 = { -brand-short-name } helps stop websites from tracking you online, making it harder for ads to follow you around the web.
onboarding-tracking-protection-button2 = How it Works

onboarding-data-sync-title = Take Your Settings with You
# "Sync" is short for synchronize.
onboarding-data-sync-text2 = Sync your bookmarks, passwords, and more everywhere you use { -brand-product-name }.
onboarding-data-sync-button2 = Sign in to { -sync-brand-short-name }

onboarding-firefox-monitor-title = Stay Alert to Data Breaches
onboarding-firefox-monitor-text2 = { -monitor-brand-name } monitors if your email has appeared in a known data breach and alerts you if it appears in a new breach.
onboarding-firefox-monitor-button = Sign Up for Alerts

onboarding-browse-privately-title = Browse Privately
onboarding-browse-privately-text = Private Browsing clears your search and browsing history to keep it secret from anyone who uses your computer.
onboarding-browse-privately-button = Open a Private Window

onboarding-firefox-send-title = Keep Your Shared Files Private
onboarding-firefox-send-text2 = Upload your files to { -send-brand-name } to share them with end-to-end encryption and a link that automatically expires.
onboarding-firefox-send-button = Try { -send-brand-name }

onboarding-mobile-phone-title = Get { -brand-product-name } on Your Phone
onboarding-mobile-phone-text = Download { -brand-product-name } for iOS or Android and sync your data across devices.
# "Mobile" is short for mobile/cellular phone, "Browser" is short for web
# browser.
onboarding-mobile-phone-button = Download Mobile Browser

onboarding-send-tabs-title = Instantly Send Yourself Tabs
# "Send Tabs" refers to "Send Tab to Device" feature that appears when opening a
# tab's context menu.
onboarding-send-tabs-text2 = Easily share pages between your devices without having to copy links or leave the browser.
onboarding-send-tabs-button = Start Using Send Tabs

onboarding-pocket-anywhere-title = Read and Listen Anywhere
onboarding-pocket-anywhere-text2 = Save your favorite content offline with the { -pocket-brand-name } App and read, listen, and watch whenever it’s convenient for you.
onboarding-pocket-anywhere-button = Try { -pocket-brand-name }

onboarding-lockwise-strong-passwords-title = Create and Store Strong Passwords
onboarding-lockwise-strong-passwords-text = { -lockwise-brand-name } creates strong passwords on the spot and saves all of them in one place.
onboarding-lockwise-strong-passwords-button = Manage Your Logins

onboarding-facebook-container-title = Set Boundaries with Facebook
onboarding-facebook-container-text2 = { -facebook-container-brand-name } keeps your profile separate from everything else, making it harder for Facebook to target you with ads.
onboarding-facebook-container-button = Add the Extension

onboarding-import-browser-settings-title = Import Your Bookmarks, Passwords, and More
onboarding-import-browser-settings-text = Dive right in—easily bring your Chrome sites and settings with you.
onboarding-import-browser-settings-button = Import Chrome Data

onboarding-personal-data-promise-title = Private by Design
onboarding-personal-data-promise-text = { -brand-product-name } treats your data with respect by taking less of it, protecting it, and being clear about how we use it.
onboarding-personal-data-promise-button = Read our Promise
