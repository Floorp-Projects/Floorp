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

## A modal confirmation dialog to allow an extension on quarantined domains.

# Variables:
#   $addonName (String): localized name of the extension.
webext-quarantine-confirmation-title =
    Run { $addonName } on restricted sites?

webext-quarantine-confirmation-line-1 =
    To protect your data, this extension is not allowed on this site.
webext-quarantine-confirmation-line-2 =
    Allow this extension if you trust it to read and change your data on sites restricted by { -vendor-short-name }.

webext-quarantine-confirmation-allow =
    .label = Allow
    .accesskey = A

webext-quarantine-confirmation-deny =
    .label = Don’t Allow
    .accesskey = D
