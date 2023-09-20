# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Variables:
#   $date (string) - Date to be formatted based on locale
fxviewtabrow-date = { DATETIME($date, dateStyle: "short") }

# Variables:
#   $time (string) - Time to be formatted based on locale
fxviewtabrow-time = { DATETIME($time, timeStyle: "short") }

# Variables:
#   $targetURI (string) - URL of tab that will be opened in the new tab
fxviewtabrow-tabs-list-tab =
  .title = Open { $targetURI } in a new tab

# Variables:
#   $tabTitle (string) - Title of tab being dismissed
fxviewtabrow-dismiss-tab-button =
  .title = Dismiss { $tabTitle }

# Used instead of the localized relative time when a timestamp is within a minute or so of now
fxviewtabrow-just-now-timestamp = Just now

# Strings below are used for context menu options within panel-list.
# For developers, this duplicates command because the label attribute is required.

fxviewtabrow-delete = Delete
    .accesskey = D
fxviewtabrow-forget-about-this-site = Forget About This Site…
    .accesskey = F
fxviewtabrow-open-in-window = Open in New Window
    .accesskey = N
fxviewtabrow-open-in-private-window = Open in New Private Window
    .accesskey = P
# “Bookmark” is a verb, as in "Bookmark this page" (add to bookmarks).
fxviewtabrow-add-bookmark = Bookmark…
    .accesskey = B
fxviewtabrow-save-to-pocket = Save to { -pocket-brand-name }
    .accesskey = o
fxviewtabrow-copy-link = Copy Link
    .accesskey = L
fxviewtabrow-close-tab = Close Tab
    .accesskey = C
fxviewtabrow-move-tab = Move Tab
    .accesskey = v
fxviewtabrow-move-tab-start = Move to Start
    .accesskey = S
fxviewtabrow-move-tab-end = Move to End
    .accesskey = E
fxviewtabrow-move-tab-window = Move to New Window
    .accesskey = W
fxviewtabrow-send-tab = Send Tab to Device
    .accesskey = n

# Variables:
#   $tabTitle (string) - Title of the tab to which the context menu is associated
fxviewtabrow-options-menu-button =
  .title = Options for { $tabTitle }
