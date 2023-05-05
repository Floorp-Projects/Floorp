# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

mztabrow-open-menu-button =
  .title = Open menu

# Variables:
#   $date (string) - Date to be formatted based on locale
mztabrow-date = { DATETIME($date, dateStyle: "short") }

# Variables:
#   $time (string) - Time to be formatted based on locale
mztabrow-time = { DATETIME($time, timeStyle: "short") }

# Variables:
#   $targetURI (string) - URL of tab that will be opened in the new tab
mztabrow-tabs-list-tab =
  .title = Open { $targetURI } in a new tab

# Variables:
#   $tabTitle (string) - Title of tab being dismissed
mztabrow-dismiss-tab-button =
  .title = Dismiss { $tabTitle }

# Used instead of the localized relative time when a timestamp is within a minute or so of now
mztabrow-just-now-timestamp = Just now

# Strings below are used for context menu options within panel-list.
# For developers, this duplicates command because the label attribute is required.

mztabrow-delete = Delete
    .accesskey = D
mztabrow-forget-about-this-site = Forget About This Site…
    .accesskey = F
mztabrow-open-in-window = Open in New Window
    .accesskey = N
mztabrow-open-in-private-window = Open in New Private Window
    .accesskey = P
# “Bookmark” is a verb, as in "Bookmark this page" (add to bookmarks).
mztabrow-add-bookmark = Bookmark…
    .accesskey = B
mztabrow-save-to-pocket = Save to { -pocket-brand-name }
    .accesskey = o
mztabrow-copy-link = Copy Link
    .accesskey = L
