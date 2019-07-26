# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

about-logins-page-title = Logins & Passwords

login-filter =
  .placeholder = Search Logins

create-login-button = Create New Login

## The ⋯ menu that is in the top corner of the page
menu =
  .title = Open menu
# This menuitem is only visible on Windows
menu-menuitem-import = Import Passwords…
menu-menuitem-preferences =
  { PLATFORM() ->
      [windows] Options
     *[other] Preferences
  }
menu-menuitem-feedback = Send Feedback
menu-menuitem-faq = Frequently Asked Questions
menu-menuitem-download-android = Lockwise for Android
menu-menuitem-download-iphone = Lockwise for iPhone and iPad

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
login-list-last-changed-option = Last Modified
login-list-last-used-option = Last Used
login-list-item-title-new-login = New Login
login-list-item-subtitle-new-login = Enter your login credentials
login-list-item-subtitle-missing-username = (no username)

## Login
login-item-new-login-title = Create New Login
login-item-edit-button = Edit
login-item-delete-button = Delete
login-item-origin-label = Website Address
login-item-origin =
  .placeholder = https://www.example.com
login-item-open-site-button = Launch
login-item-username-label = Username
login-item-username =
  .placeholder = name@example.com
login-item-copy-username-button-text = Copy
login-item-copied-username-button-text = Copied!
login-item-password-label = Password
login-item-password-reveal-checkbox-show =
  .title = Show password
login-item-password-reveal-checkbox-hide =
  .title = Hide password
login-item-copy-password-button-text = Copy
login-item-copied-password-button-text = Copied!
login-item-save-changes-button = Save Changes
login-item-save-new-button = Save
login-item-cancel-button = Cancel
login-item-time-changed = Last modified: { DATETIME($timeChanged, day: "numeric", month: "long", year: "numeric") }
login-item-time-created = Created: { DATETIME($timeCreated, day: "numeric", month: "long", year: "numeric") }
login-item-time-used = Last used: { DATETIME($timeUsed, day: "numeric", month: "long", year: "numeric") }

## Master Password notification
master-password-notification-message = Please enter your master password to view saved logins & passwords
master-password-reload-button =
  .label = Log in
  .accesskey = L

confirm-delete-dialog-title = Delete this login?
confirm-delete-dialog-message = This action cannot be undone.
confirm-delete-dialog-dismiss-button =
  .title = Cancel
confirm-delete-dialog-cancel-button = Cancel
confirm-delete-dialog-confirm-button = Delete
