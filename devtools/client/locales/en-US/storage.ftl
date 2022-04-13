# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are used inside the Storage Inspector.

# Key shortcut used to focus the filter box on top of the data view
storage-filter-key = CmdOrCtrl+F

# Hint shown when the selected storage host does not contain any data
storage-table-empty-text = No data present for selected host

# Hint shown when the cookies storage type is selected. Clicking the link will open
# https://firefox-source-docs.mozilla.org/devtools-user/storage_inspector/cookies/
storage-table-type-cookies-hint = View and edit cookies by selecting a host. <a data-l10n-name="learn-more-link">Learn more</a>

# Hint shown when the local storage type is selected. Clicking the link will open
# https://firefox-source-docs.mozilla.org/devtools-user/storage_inspector/local_storage_session_storage/
storage-table-type-localstorage-hint = View and edit the local storage by selecting a host. <a data-l10n-name="learn-more-link">Learn more</a>

# Hint shown when the session storage type is selected. Clicking the link will open
# https://firefox-source-docs.mozilla.org/devtools-user/storage_inspector/local_storage_session_storage/
storage-table-type-sessionstorage-hint = View and edit the session storage by selecting a host. <a data-l10n-name="learn-more-link">Learn more</a>

# Hint shown when the IndexedDB storage type is selected. Clicking the link will open
# https://firefox-source-docs.mozilla.org/devtools-user/storage_inspector/indexeddb/
storage-table-type-indexeddb-hint = View and delete IndexedDB entries by selecting a database. <a data-l10n-name="learn-more-link">Learn more</a>

# Hint shown when the cache storage type is selected. Clicking the link will open
# https://firefox-source-docs.mozilla.org/devtools-user/storage_inspector/cache_storage/
storage-table-type-cache-hint = View and delete the cache storage entries by selecting a storage. <a data-l10n-name="learn-more-link">Learn more</a>

# Hint shown when the extension storage type is selected. Clicking the link will open
# https://firefox-source-docs.mozilla.org/devtools-user/storage_inspector/extension_storage/
storage-table-type-extensionstorage-hint = View and edit the extension storage by selecting a host. <a data-l10n-name="learn-more-link">Learn more</a>

# Placeholder for the searchbox that allows you to filter the table items
storage-search-box =
  .placeholder = Filter Items

# Placeholder text in the sidebar search box
storage-variable-view-search-box =
  .placeholder = Filter values

# Add Item button title
storage-add-button =
  .title = Add Item

# Refresh button title
storage-refresh-button =
  .title = Refresh Items

# Context menu action to delete all storage items
storage-context-menu-delete-all =
  .label = Delete All

# Context menu action to delete all session cookies
storage-context-menu-delete-all-session-cookies =
  .label = Delete All Session Cookies

# Context menu action to copy a storage item
storage-context-menu-copy =
  .label = Copy

# Context menu action to delete storage item
# Variables:
#   $itemName (String) - Name of the storage item that will be deleted
storage-context-menu-delete =
  .label = Delete “{ $itemName }”

# Context menu action to add an item
storage-context-menu-add-item =
  .label = Add Item

# Context menu action to delete all storage items from a given host
# Variables:
#   $host (String) - Host for which we want to delete the items
storage-context-menu-delete-all-from =
  .label = Delete All From “{ $host }”

## Header names of the columns in the Storage Table for each type of storage available
## through the Storage Tree to the side.

storage-table-headers-cookies-name = Name
storage-table-headers-cookies-value = Value
storage-table-headers-cookies-expires = Expires / Max-Age
storage-table-headers-cookies-size = Size
storage-table-headers-cookies-last-accessed = Last Accessed
storage-table-headers-cookies-creation-time = Created
storage-table-headers-cache-status = Status
storage-table-headers-extension-storage-area = Storage Area

## Labels for Storage type groups present in the Storage Tree, like cookies, local storage etc.

storage-tree-labels-cookies = Cookies
storage-tree-labels-local-storage = Local Storage
storage-tree-labels-session-storage = Session Storage
storage-tree-labels-indexed-db = Indexed DB
storage-tree-labels-cache = Cache Storage
storage-tree-labels-extension-storage = Extension Storage

##

# Tooltip for the button that collapses the right panel in the
# storage UI when the panel is closed.
storage-expand-pane =
  .title = Expand Pane

# Tooltip for the button that collapses the right panel in the
# storage UI when the panel is open.
storage-collapse-pane =
  .title = Collapse Pane

# String displayed in the expires column when the cookie is a Session Cookie
storage-expires-session = Session

# Heading displayed over the item value in the sidebar
storage-data = Data

# Heading displayed over the item parsed value in the sidebar
storage-parsed-value = Parsed Value

# Warning notification when IndexedDB database could not be deleted immediately.
# Variables:
#   $dbName (String) - Name of the database
storage-idb-delete-blocked = Database “{ $dbName }” will be deleted after all connections are closed.

# Error notification when IndexedDB database could not be deleted.
# Variables:
#   $dbName (String) - Name of the database
storage-idb-delete-error = Database “{ $dbName }” could not be deleted.
