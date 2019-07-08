# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

about-logins-page-title = Logins & Passwords

login-filter =
  .placeholder = Search Logins

create-login-button = New Login

## The ⋯ menu that is in the top corner of the page
menu =
  .title = Open menu
menu-menuitem-faq = Frequently Asked Questions
menu-menuitem-feedback = Leave Feedback
menu-menuitem-import = Import Passwords…
menu-menuitem-preferences =
  { PLATFORM() ->
      [windows] Options
     *[other] Preferences
  }

## Login List
login-list =
  .aria-label = Logins matching search query
login-list-count =
  { $count ->
      [one] { $count } login
     *[other] { $count } logins
  }
login-list-last-changed-option = Last Changed
login-list-last-used-option = Last Used
login-list-name-option = Name
login-list-sort-label-text = Sort by:
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
login-item-copied-username-button-text = ✔ Copied!
login-item-copy-username-button-text = Copy
login-item-password-label = Password
login-item-password-reveal-checkbox-show =
  .title = Show password
login-item-password-reveal-checkbox-hide =
  .title = Hide password
login-item-copied-password-button-text = ✔ Copied!
login-item-copy-password-button-text = Copy
login-item-save-changes-button = Save Changes
login-item-cancel-button = Cancel
login-item-time-changed = Last modified: { DATETIME($timeChanged, day: "numeric", month: "long", year: "numeric") }
login-item-time-created = Created: { DATETIME($timeCreated, day: "numeric", month: "long", year: "numeric") }
login-item-time-used = Last used: { DATETIME($timeUsed, day: "numeric", month: "long", year: "numeric") }

## Master Password notification
master-password-notification-message = Please enter your master password to view saved logins & passwords
master-password-reload-button =
  .label = Log in
  .accesskey = L

confirm-delete-dialog-title = Confirm Deletion
confirm-delete-dialog-message = Are you sure you want to delete this login?
confirm-delete-dialog-dismiss-button =
  .title = Cancel
confirm-delete-dialog-cancel-button = Cancel
confirm-delete-dialog-confirm-button = Delete login
