# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# NOTE: New strings should use the about-logins- prefix.

about-logins-page-title = Logins & Passwords

login-filter =
  .placeholder = Search Logins

create-login-button = Create New Login

fxaccounts-sign-in-text = Get your passwords on your other devices
fxaccounts-sign-in-button = Sign in to { -sync-brand-short-name }
fxaccounts-avatar-button =
  .title = Manage account

## The ⋯ menu that is in the top corner of the page

menu =
  .title = Open menu
# This menuitem is only visible on Windows and macOS
about-logins-menu-menuitem-import-from-another-browser = Import from Another Browser…
about-logins-menu-menuitem-import-from-a-file = Import from a File…
about-logins-menu-menuitem-export-logins = Export Logins…
menu-menuitem-preferences =
  { PLATFORM() ->
      [windows] Options
     *[other] Preferences
  }
about-logins-menu-menuitem-help = Help

## Login List

login-list =
  .aria-label = Logins matching search query
login-list-count =
  { $count ->
      [one] { $count } login
     *[other] { $count } logins
  }
login-list-sort-label-text = Sort by:
login-list-name-option = Name (A-Z)
login-list-name-reverse-option = Name (Z-A)
about-logins-login-list-alerts-option = Alerts
login-list-last-changed-option = Last Modified
login-list-last-used-option = Last Used
login-list-intro-title = No logins found
login-list-intro-description = When you save a password in { -brand-product-name }, it will show up here.
about-logins-login-list-empty-search-title = No logins found
about-logins-login-list-empty-search-description = There are no results matching your search.
login-list-item-title-new-login = New Login
login-list-item-subtitle-new-login = Enter your login credentials
login-list-item-subtitle-missing-username = (no username)
about-logins-list-item-breach-icon =
  .title = Breached website
about-logins-list-item-vulnerable-password-icon =
  .title = Vulnerable password

## Introduction screen

about-logins-login-intro-heading-logged-out = Looking for your saved logins? Set up { -sync-brand-short-name } or Import Them.
about-logins-login-intro-heading-logged-in = No synced logins found.
login-intro-description = If you saved your logins to { -brand-product-name } on a different device, here’s how to get them here:
login-intro-instruction-fxa = Create or sign in to your { -fxaccount-brand-name } on the device where your logins are saved
login-intro-instruction-fxa-settings = Make sure you’ve selected the Logins checkbox in { -sync-brand-short-name } Settings
about-logins-intro-instruction-help = Visit <a data-l10n-name="help-link">{ -lockwise-brand-short-name } Support</a> for more help
about-logins-intro-import = If your logins are saved in another browser, you can <a data-l10n-name="import-link">import them into { -lockwise-brand-short-name }</a>
about-logins-intro-import2 = If your logins are saved outside of { -brand-product-name }, you can <a data-l10n-name="import-browser-link">import them from another browser</a> or <a data-l10n-name="import-file-link">from a file</a>

## Login

login-item-new-login-title = Create New Login
login-item-edit-button = Edit
about-logins-login-item-remove-button = Remove
login-item-origin-label = Website address
login-item-origin =
  .placeholder = https://www.example.com
login-item-username-label = Username
about-logins-login-item-username =
  .placeholder = (no username)
login-item-copy-username-button-text = Copy
login-item-copied-username-button-text = Copied!
login-item-password-label = Password
login-item-password-reveal-checkbox =
  .aria-label = Show password
login-item-copy-password-button-text = Copy
login-item-copied-password-button-text = Copied!
login-item-save-changes-button = Save Changes
login-item-save-new-button = Save
login-item-cancel-button = Cancel
login-item-time-changed = Last modified: { DATETIME($timeChanged, day: "numeric", month: "long", year: "numeric") }
login-item-time-created = Created: { DATETIME($timeCreated, day: "numeric", month: "long", year: "numeric") }
login-item-time-used = Last used: { DATETIME($timeUsed, day: "numeric", month: "long", year: "numeric") }

## OS Authentication dialog

about-logins-os-auth-dialog-caption = { -brand-full-name }

## The macOS strings are preceded by the operating system with "Firefox is trying to "
## and includes subtitle of "Enter password for the user "xxx" to allow this." These
## notes are only valid for English. Please test in your respected locale.

# This message can be seen when attempting to edit a login in about:logins on Windows.
about-logins-edit-login-os-auth-dialog-message-win = To edit your login, enter your Windows login credentials. This helps protect the security of your accounts.
# This message can be seen when attempting to edit a login in about:logins
# On MacOS, only provide the reason that account verification is needed. Do not put a complete sentence here.
about-logins-edit-login-os-auth-dialog-message-macosx = edit the saved login

# This message can be seen when attempting to reveal a password in about:logins on Windows.
about-logins-reveal-password-os-auth-dialog-message-win = To view your password, enter your Windows login credentials. This helps protect the security of your accounts.
# This message can be seen when attempting to reveal a password in about:logins
# On MacOS, only provide the reason that account verification is needed. Do not put a complete sentence here.
about-logins-reveal-password-os-auth-dialog-message-macosx = reveal the saved password

# This message can be seen when attempting to copy a password in about:logins on Windows.
about-logins-copy-password-os-auth-dialog-message-win = To copy your password, enter your Windows login credentials. This helps protect the security of your accounts.
# This message can be seen when attempting to copy a password in about:logins
# On MacOS, only provide the reason that account verification is needed. Do not put a complete sentence here.
about-logins-copy-password-os-auth-dialog-message-macosx = copy the saved password

# This message can be seen when attempting to export a password in about:logins on Windows.
about-logins-export-password-os-auth-dialog-message-win = To export your logins, enter your Windows login credentials. This helps protect the security of your accounts.
# This message can be seen when attempting to export a password in about:logins
# On MacOS, only provide the reason that account verification is needed. Do not put a complete sentence here.
about-logins-export-password-os-auth-dialog-message-macosx = export saved logins and passwords

## Primary Password notification

about-logins-primary-password-notification-message = Please enter your Primary Password to view saved logins & passwords
master-password-reload-button =
  .label = Log in
  .accesskey = L

## Password Sync notification

enable-password-sync-notification-message =
  { PLATFORM() ->
      [windows] Want your logins everywhere you use { -brand-product-name }? Go to your { -sync-brand-short-name } Options and select the Logins checkbox.
     *[other] Want your logins everywhere you use { -brand-product-name }? Go to your { -sync-brand-short-name } Preferences and select the Logins checkbox.
  }
enable-password-sync-preferences-button =
  .label =
    { PLATFORM() ->
        [windows] Visit { -sync-brand-short-name } Options
       *[other] Visit { -sync-brand-short-name } Preferences
    }
  .accesskey = V
about-logins-enable-password-sync-dont-ask-again-button =
  .label = Don’t ask me again
  .accesskey = D

## Dialogs

confirmation-dialog-cancel-button = Cancel
confirmation-dialog-dismiss-button =
  .title = Cancel

about-logins-confirm-remove-dialog-title = Remove this login?
confirm-delete-dialog-message = This action cannot be undone.
about-logins-confirm-remove-dialog-confirm-button = Remove

about-logins-confirm-export-dialog-title = Export logins and passwords
about-logins-confirm-export-dialog-message = Your passwords will be saved as readable text (e.g., BadP@ssw0rd) so anyone who can open the exported file can view them.
about-logins-confirm-export-dialog-confirm-button = Export…

confirm-discard-changes-dialog-title = Discard unsaved changes?
confirm-discard-changes-dialog-message = All unsaved changes will be lost.
confirm-discard-changes-dialog-confirm-button = Discard

## Breach Alert notification

about-logins-breach-alert-title = Website Breach
breach-alert-text = Passwords were leaked or stolen from this website since you last updated your login details. Change your password to protect your account.
about-logins-breach-alert-date = This breach occurred on { DATETIME($date, day: "numeric", month: "long", year: "numeric") }
# Variables:
#   $hostname (String) - The hostname of the website associated with the login, e.g. "example.com"
about-logins-breach-alert-link = Go to { $hostname }
about-logins-breach-alert-learn-more-link = Learn more

## Vulnerable Password notification

about-logins-vulnerable-alert-title = Vulnerable Password
about-logins-vulnerable-alert-text2 = This password has been used on another account that was likely in a data breach. Reusing credentials puts all your accounts at risk. Change this password.
# Variables:
#   $hostname (String) - The hostname of the website associated with the login, e.g. "example.com"
about-logins-vulnerable-alert-link = Go to { $hostname }
about-logins-vulnerable-alert-learn-more-link = Learn more

## Error Messages

# This is an error message that appears when a user attempts to save
# a new login that is identical to an existing saved login.
# Variables:
#   $loginTitle (String) - The title of the website associated with the login.
about-logins-error-message-duplicate-login-with-link = An entry for { $loginTitle } with that username already exists. <a data-l10n-name="duplicate-link">Go to existing entry?</a>

# This is a generic error message.
about-logins-error-message-default = An error occurred while trying to save this password.


## Login Export Dialog

# Title of the file picker dialog
about-logins-export-file-picker-title = Export Logins File
# The default file name shown in the file picker when exporting saved logins.
# This must end in .csv
about-logins-export-file-picker-default-filename = logins.csv
about-logins-export-file-picker-export-button = Export
# A description for the .csv file format that may be shown as the file type
# filter by the operating system.
about-logins-export-file-picker-csv-filter-title =
  { PLATFORM() ->
      [macos] CSV Document
     *[other] CSV File
  }

## Login Import Dialog

# Title of the file picker dialog
about-logins-import-file-picker-title = Import Logins File
about-logins-import-file-picker-import-button = Import
# A description for the .csv file format that may be shown as the file type
# filter by the operating system.
about-logins-import-file-picker-csv-filter-title =
  { PLATFORM() ->
      [macos] CSV Document
     *[other] CSV File
  }
