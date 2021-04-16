# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

places-open =
  .label = Open
  .accesskey = O
places-open-in-tab =
  .label = Open in New Tab
  .accesskey = w
places-open-all-bookmarks =
  .label = Open All Bookmarks
  .accesskey = O
places-open-all-in-tabs =
  .label = Open All in Tabs
  .accesskey = O
places-open-in-window =
  .label = Open in New Window
  .accesskey = N
places-open-in-private-window =
  .label = Open in New Private Window
  .accesskey = P

places-add-bookmark =
  .label = Add Bookmark…
  .accesskey = B
places-add-folder-contextmenu =
  .label = Add Folder…
  .accesskey = F
places-add-folder =
  .label = Add Folder…
  .accesskey = o
places-add-separator =
  .label = Add Separator
  .accesskey = S

places-view =
  .label = View
  .accesskey = w
places-by-date =
  .label = By Date
  .accesskey = D
places-by-site =
  .label = By Site
  .accesskey = S
places-by-most-visited =
  .label = By Most Visited
  .accesskey = V
places-by-last-visited =
  .label = By Last Visited
  .accesskey = L
places-by-day-and-site =
  .label = By Date and Site
  .accesskey = t

places-history-search =
  .placeholder = Search history
places-bookmarks-search =
  .placeholder = Search bookmarks

places-delete-domain-data =
  .label = Forget About This Site
  .accesskey = F
places-sortby-name =
  .label = Sort By Name
  .accesskey = r
# places-edit-bookmark and places-edit-generic will show one or the other and can have the same access key.
places-edit-bookmark =
  .label = Edit Bookmark…
  .accesskey = i
places-edit-generic =
  .label = Edit…
  .accesskey = i
places-edit-folder =
  .label = Rename Folder…
  .accesskey = e
places-remove-folder =
  .label =
      { $count ->
          [1] Remove Folder
         *[other] Remove Folders
      }
  .accesskey = m

# Managed bookmarks are created by an administrator and cannot be changed by the user.
managed-bookmarks =
  .label = Managed bookmarks
# This label is used when a managed bookmarks folder doesn't have a name.
managed-bookmarks-subfolder =
  .label = Subfolder

# This label is used for the "Other Bookmarks" folder that appears in the bookmarks toolbar.
other-bookmarks-folder =
  .label = Other Bookmarks

# Variables:
# $count (number) - The number of elements being selected for removal.
places-remove-bookmark =
  .label =
      { $count ->
          [1] Remove Bookmark
         *[other] Remove Bookmarks
      }
  .accesskey = e

places-manage-bookmarks =
  .label = Manage Bookmarks
  .accesskey = M
