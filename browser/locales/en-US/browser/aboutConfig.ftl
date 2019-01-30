# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# An old map warning, see https://en.wikipedia.org/wiki/Here_be_dragons
about-config-warning-title = Here be dragons!
about-config-warning-text = Changing these advanced settings can be harmful to the stability, security, and performance of this application. You should only continue if you are sure of what you are doing.
about-config-warning-checkbox = Annoy me again, please!
about-config-warning-button = I accept the risk

about-config-title = about:config

about-config-search =
    .placeholder = Search or press ESC to show all

about-config-pref-add = Add
about-config-pref-toggle = Toggle
about-config-pref-edit = Edit
about-config-pref-save = Save
about-config-pref-reset = Reset
about-config-pref-delete = Delete

## Preferences with a non-default value are differentiated visually, and at the
## same time the state is made accessible to screen readers using an aria-label
## that won't be visible or copied to the clipboard.
##
## Variables:
##   $value (String): The full value of the preference.
about-config-pref-accessible-value-default =
    .aria-label = { $value } (default)
about-config-pref-accessible-value-custom =
    .aria-label = { $value } (custom)
