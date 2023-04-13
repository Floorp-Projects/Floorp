# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

migration-wizard-selection-header = Import Browser Data
migration-wizard-selection-list = Select the data you’d like to import.

# Shown in the new migration wizard's dropdown selector for choosing the browser
# to import from. This variant is shown when the selected browser doesn't support
# user profiles, and so we only show the browser name.
#
# Variables:
#  $sourceBrowser (String): the name of the browser to import from.
migration-wizard-selection-option-without-profile = { $sourceBrowser }

# Shown in the new migration wizard's dropdown selector for choosing the browser
# and user profile to import from. This variant is shown when the selected browser
# supports user profiles.
#
# Variables:
#  $sourceBrowser (String): the name of the browser to import from.
#  $profileName (String): the name of the user profile to import from.
migration-wizard-selection-option-with-profile = { $sourceBrowser } — { $profileName }

# Each migrator is expected to include a display name string, and that display
# name string should have a key with "migration-wizard-migrator-display-name-"
# as a prefix followed by the unique identification key for the migrator.

migration-wizard-migrator-display-name-brave = Brave
migration-wizard-migrator-display-name-canary = Chrome Canary
migration-wizard-migrator-display-name-chrome = Chrome
migration-wizard-migrator-display-name-chrome-beta = Chrome Beta
migration-wizard-migrator-display-name-chrome-dev = Chrome Dev
migration-wizard-migrator-display-name-chromium = Chromium
migration-wizard-migrator-display-name-chromium-360se = 360 Secure Browser
migration-wizard-migrator-display-name-chromium-edge = Microsoft Edge
migration-wizard-migrator-display-name-chromium-edge-beta = Microsoft Edge Beta
migration-wizard-migrator-display-name-edge-legacy = Microsoft Edge Legacy
migration-wizard-migrator-display-name-firefox = Firefox
migration-wizard-migrator-display-name-ie = Microsoft Internet Explorer
migration-wizard-migrator-display-name-opera = Opera
migration-wizard-migrator-display-name-opera-gx = Opera GX
migration-wizard-migrator-display-name-safari = Safari
migration-wizard-migrator-display-name-vivaldi = Vivaldi

## These strings will be displayed based on how many resources are selected to import

migration-all-available-data-label = Import all available data
migration-no-selected-data-label = No data selected for import
migration-selected-data-label = Import selected data

##

migration-select-all-option-label = Select all
migration-bookmarks-option-label = Bookmarks

# Favorites is used for Bookmarks when importing from Internet Explorer or
# Edge, as this is the terminology for bookmarks on those browsers.
migration-favorites-option-label = Favorites

migration-logins-and-passwords-option-label = Saved logins and passwords
migration-history-option-label = Browsing history
migration-form-autofill-option-label = Form autofill data
migration-import-button-label = Import
migration-cancel-button-label = Cancel
migration-done-button-label = Done

migration-wizard-import-browser-no-browsers = { -brand-short-name } couldn’t find any programs that contain bookmark, history or password data.
migration-wizard-import-browser-no-resources = There was an error. { -brand-short-name } can’t find any data to import from that browser profile.

## These strings will be used to create a dynamic list of items that can be
## imported. The list will be created using Intl.ListFormat(), so it will
## follow each locale's rules, and the first item will be capitalized by code.
## When applicable, the resources should be in their plural form.
## For example, a possible list could be "Bookmarks, passwords and autofill data".

migration-list-bookmark-label = bookmarks

# “favorites” refers to bookmarks in Edge and Internet Explorer. Use the same terminology
# if the browser is available in your language.
migration-list-favorites-label = favorites
migration-list-password-label = passwords
migration-list-history-label = history
migration-list-autofill-label = autofill data

##

migration-wizard-progress-header = Importing Data
migration-wizard-progress-done-header = Data Imported Successfully
migration-wizard-progress-icon-in-progress =
  .aria-label = Importing…
migration-wizard-progress-icon-completed =
  .aria-label = Completed

migration-safari-password-import-header = Import Passwords from Safari
migration-safari-password-import-steps-header = To import Safari passwords:
migration-safari-password-import-step1 = In Safari, open “Safari” menu and go to Preferences > Passwords
migration-safari-password-import-step2 = Select the <img data-l10n-name="safari-icon-3dots"/> button and choose “Export All Passwords”
migration-safari-password-import-step3 = Save the passwords file
migration-safari-password-import-step4 = Use “Select File” below to choose the passwords file you saved
migration-safari-password-import-skip-button = Skip
migration-safari-password-import-select-button = Select File


# Shown in the migration wizard after importing bookmarks from another
# browser has completed.
#
# Variables:
#  $quantity (Number): the number of successfully imported bookmarks
migration-wizard-progress-success-bookmarks =
    { $quantity ->
        [one] { $quantity } bookmark
       *[other] { $quantity } bookmarks
    }

# Shown in the migration wizard after importing bookmarks from either
# Internet Explorer or Edge.
#
# Use the same terminology if the browser is available in your language.
#
# Variables:
#  $quantity (Number): the number of successfully imported bookmarks
migration-wizard-progress-success-favorites =
    { $quantity ->
        [one] { $quantity } favorite
       *[other] { $quantity } favorites
    }

# Shown in the migration wizard after importing passwords from another
# browser has completed.
#
# Variables:
#  $quantity (Number): the number of successfully imported passwords
migration-wizard-progress-success-passwords =
    { $quantity ->
        [one] { $quantity } password
       *[other] { $quantity } passwords
    }

# Shown in the migration wizard after importing history from another
# browser has completed.
#
# Variables:
#  $maxAgeInDays (Number): the maximum number of days of history that might be imported.
migration-wizard-progress-success-history =
    { $maxAgeInDays ->
        [one] From the last day
       *[other] From the last { $maxAgeInDays } days
    }

migration-wizard-progress-success-formdata = Form history

migration-wizard-safari-permissions-sub-header = To import Safari bookmarks and browsing history:
migration-wizard-safari-instructions-continue = Select “Continue”
migration-wizard-safari-instructions-folder = Select Safari folder in the list and choose “Open”
migration-wizard-safari-select-button = Select File
