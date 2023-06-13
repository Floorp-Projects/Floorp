# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Variables:
#   $addonName (String): localized named of the extension that is asking to change the default search engine.
#   $currentEngine (String): name of the current search engine.
#   $newEngine (String): name of the new search engine.
webext-default-search-description = { $addonName } would like to change your default search engine from { $currentEngine } to { $newEngine }. Is that OK?
webext-default-search-yes =
    .label = Yes
    .accesskey = Y
webext-default-search-no =
    .label = No
    .accesskey = N

# Variables:
#   $addonName (String): localized named of the extension that was just installed.
addon-post-install-message = { $addonName } was added.
