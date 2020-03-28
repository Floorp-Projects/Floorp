# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

about-logins-page-title = Logins & Passwords

# "Google Play" and "App Store" are both branding and should not be translated

login-app-promo-title = Take your passwords everywhere
login-app-promo-subtitle = Get the free { -lockwise-brand-name } app
login-app-promo-android =
  .alt = Get it on Google Play
login-app-promo-apple =
  .alt = Download on the App Store

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
menu-menuitem-preferences =
  { PLATFORM() ->
      [windows] Options
     *[other] Preferences
  }
about-logins-menu-menuitem-help = Help
menu-menuitem-android-app = { -lockwise-brand-short-name } for Android
menu-menuitem-iphone-app = { -lockwise-brand-short-name } for iPhone and iPad

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

login-intro-heading = Looking for your saved logins? Set up { -sync-brand-short-name }.
about-logins-login-intro-heading-logged-in = No synced logins found.
login-intro-description = If you saved your logins to { -brand-product-name } on a different device, here’s how to get them here:
login-intro-instruction-fxa = Create or sign in to your { -fxaccount-brand-name } on the device where your logins are saved
login-intro-instruction-fxa-settings = Make sure you’ve selected the Logins checkbox in { -sync-brand-short-name } Settings
about-logins-intro-instruction-help = Visit <a data-l10n-name="help-link">{ -lockwise-brand-short-name } Support</a> for more help
about-logins-intro-import = If your logins are saved in another browser, you can <a data-l10n-name="import-link">import them into { -lockwise-brand-short-name }</a>

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

# This message can be seen by attempting to edit a login in about:logins
about-logins-edit-login-os-auth-dialog-message = Verify your identity to edit the saved login.
# This message can be seen by attempting to edit a login in about:logins
# On MacOS, only provide the reason that account verification is needed. Do not put a complete sentence here.
about-logins-edit-login-os-auth-dialog-message-macosx = edit the saved login

# This message can be seen by attempting to reveal a password in about:logins
about-logins-reveal-password-os-auth-dialog-message = Verify your identity to reveal the saved password.
# This message can be seen by attempting to reveal a password in about:logins
# On MacOS, only provide the reason that account verification is needed. Do not put a complete sentence here.
about-logins-reveal-password-os-auth-dialog-message-macosx = reveal the saved password

# This message can be seen by attempting to copy a password in about:logins
about-logins-copy-password-os-auth-dialog-message = Verify your identity to copy the saved password.
# This message can be seen by attempting to copy a password in about:logins
# On MacOS, only provide the reason that account verification is needed. Do not put a complete sentence here.
about-logins-copy-password-os-auth-dialog-message-macosx = copy the saved password

## Master Password notification

master-password-notification-message = Please enter your master password to view saved logins & passwords
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

confirm-discard-changes-dialog-title = Discard unsaved changes?
confirm-discard-changes-dialog-message = All unsaved changes will be lost.
confirm-discard-changes-dialog-confirm-button = Discard

## Breach Alert notification

about-logins-breach-alert-title = Website Breach
breach-alert-text = Passwords were leaked or stolen from this website since you last updated your login details. Change your password to protect your account.
breach-alert-link = Learn more about this breach.
breach-alert-dismiss =
    .title = Close this alert
about-logins-breach-alert-date = This breach occurred on { DATETIME($date, day: "numeric", month: "long", year: "numeric") }

## Vulnerable Password notification

about-logins-vulnerable-alert-title = Vulnerable Password
about-logins-vulnerable-alert-text = This password was leaked or stolen in another company’s data breach. Reusing credentials puts all of your accounts at risk. To improve your online security, change this password.
# Variables:
#   $hostname (String) - The hostname of the website associated with the login, e.g. "example.com"
about-logins-vulnerable-alert-link = Go to { $hostname }

## Error Messages

# This is an error message that appears when a user attempts to save
# a new login that is identical to an existing saved login.
# Variables:
#   $loginTitle (String) - The title of the website associated with the login.
about-logins-error-message-duplicate-login-with-link = An entry for { $loginTitle } with that username already exists. <a data-l10n-name="duplicate-link">Go to existing entry?</a>

# This is a generic error message.
about-logins-error-message-default = An error occurred while trying to save this password.
