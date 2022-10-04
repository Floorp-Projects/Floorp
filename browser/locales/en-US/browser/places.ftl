# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

places-open =
  .label = Open
  .accesskey = O
places-open-in-tab =
  .label = Open in New Tab
  .accesskey = w
places-open-in-container-tab =
  .label = Open in New Container Tab
  .accesskey = i
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
places-history =
  .aria-label = History
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
  .accesskey = E
places-edit-generic =
  .label = Edit…
  .accesskey = E
places-edit-folder2 =
  .label = Edit Folder…
  .accesskey = E
places-delete-folder =
  .label =
      { $count ->
          [1] Delete Folder
         *[other] Delete Folders
      }
  .accesskey = D

# Managed bookmarks are created by an administrator and cannot be changed by the user.
managed-bookmarks =
  .label = Managed bookmarks
# This label is used when a managed bookmarks folder doesn't have a name.
managed-bookmarks-subfolder =
  .label = Subfolder

# This label is used for the "Other Bookmarks" folder that appears in the bookmarks toolbar.
other-bookmarks-folder =
  .label = Other Bookmarks

places-show-in-folder =
  .label = Show in Folder
  .accesskey = F

# Variables:
# $count (number) - The number of elements being selected for removal.
places-delete-bookmark =
  .label =
      { $count ->
          [1] Delete Bookmark
         *[other] Delete Bookmarks
      }
  .accesskey = D

places-manage-bookmarks =
  .label = Manage Bookmarks
  .accesskey = M

places-forget-about-this-site-confirmation-title =
  Forgetting about this site

# Variables:
# $hostOrBaseDomain (string) - The base domain (or host in case there is no base domain) for which data is being removed
places-forget-about-this-site-confirmation-message =
  This action will remove all data related to { $hostOrBaseDomain } including history, passwords, cookies, cache and content preferences. Are you sure you want to proceed?

places-forget-about-this-site-forget = Forget

places-library2 =
  .title = Library
  .style = min-width:700px; min-height:500px;

places-organize-button =
  .label = Organize
  .tooltiptext = Organize your bookmarks
  .accesskey = O

places-organize-button-mac =
  .label = Organize
  .tooltiptext = Organize your bookmarks

places-file-close =
  .label = Close
  .accesskey = C

places-cmd-close =
  .key = w

places-view-button =
  .label = Views
  .tooltiptext = Change your view
  .accesskey = V

places-view-button-mac =
  .label = Views
  .tooltiptext = Change your view

places-view-menu-columns =
  .label = Show Columns
  .accesskey = C

places-view-menu-sort =
  .label = Sort
  .accesskey = S

places-view-sort-unsorted =
  .label = Unsorted
  .accesskey = U

places-view-sort-ascending =
  .label = A > Z Sort Order
  .accesskey = A

places-view-sort-descending =
  .label = Z > A Sort Order
  .accesskey = Z

places-maintenance-button =
  .label = Import and Backup
  .tooltiptext = Import and backup your bookmarks
  .accesskey = I

places-maintenance-button-mac =
  .label = Import and Backup
  .tooltiptext = Import and backup your bookmarks

places-cmd-backup =
  .label = Backup…
  .accesskey = B

places-cmd-restore =
  .label = Restore
  .accesskey = R

places-cmd-restore-from-file =
  .label = Choose File…
  .accesskey = C

places-import-bookmarks-from-html =
  .label = Import Bookmarks from HTML…
  .accesskey = I

places-export-bookmarks-to-html =
  .label = Export Bookmarks to HTML…
  .accesskey = E

places-import-other-browser =
  .label = Import Data from Another Browser…
  .accesskey = A

places-view-sort-col-name =
  .label = Name

places-view-sort-col-tags =
  .label = Tags

places-view-sort-col-url =
  .label = Location

places-view-sort-col-most-recent-visit =
  .label = Most Recent Visit

places-view-sort-col-visit-count =
  .label = Visit Count

places-view-sort-col-date-added =
  .label = Added

places-view-sort-col-last-modified =
  .label = Last Modified

places-cmd-find-key =
  .key = f

places-back-button =
  .tooltiptext = Go back

places-forward-button =
  .tooltiptext = Go forward

places-details-pane-select-an-item-description = Select an item to view and edit its properties
