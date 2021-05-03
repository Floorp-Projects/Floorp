/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { LocalizationHelper, ELLIPSIS } = require("devtools/shared/l10n");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const { parseItemValue } = require("devtools/shared/storage/utils");
const { KeyCodes } = require("devtools/client/shared/keycodes");
const { getUnicodeHostname } = require("devtools/client/shared/unicode-url");
const getStorageTypeURL = require("devtools/client/storage/utils/mdn-utils");

// GUID to be used as a separator in compound keys. This must match the same
// constant in devtools/server/actors/storage.js,
// devtools/client/storage/test/head.js and
// devtools/server/tests/browser/head.js
const SEPARATOR_GUID = "{9d414cc5-8319-0a04-0586-c0a6ae01670a}";

loader.lazyRequireGetter(
  this,
  "TreeWidget",
  "devtools/client/shared/widgets/TreeWidget",
  true
);
loader.lazyRequireGetter(
  this,
  "TableWidget",
  "devtools/client/shared/widgets/TableWidget",
  true
);
loader.lazyImporter(
  this,
  "VariablesView",
  "resource://devtools/client/storage/VariablesView.jsm"
);

/**
 * Localization convenience methods.
 */
const STORAGE_STRINGS = "devtools/client/locales/storage.properties";
const L10N = new LocalizationHelper(STORAGE_STRINGS, true);

const GENERIC_VARIABLES_VIEW_SETTINGS = {
  lazyEmpty: true,
  // ms
  lazyEmptyDelay: 10,
  searchEnabled: true,
  contextMenuId: "variable-view-popup",
  searchPlaceholder: L10N.getStr("storage.search.placeholder"),
  preventDescriptorModifiers: true,
};

const REASON = {
  NEW_ROW: "new-row",
  NEXT_50_ITEMS: "next-50-items",
  POPULATE: "populate",
  UPDATE: "update",
};

// Maximum length of item name to show in context menu label - will be
// trimmed with ellipsis if it's longer.
const ITEM_NAME_MAX_LENGTH = 32;

// We only localize certain table headers. The headers that we do not localize
// along with their English translation are stored in this Map for easy
// reference.
const NON_L10N_STRINGS = new Map([
  ["Cache.url", "URL"],
  ["cookies.host", "Domain"],
  ["cookies.hostOnly", "HostOnly"],
  ["cookies.isHttpOnly", "HttpOnly"],
  ["cookies.isSecure", "Secure"],
  ["cookies.path", "Path"],
  ["cookies.sameSite", "SameSite"],
  ["cookies.uniqueKey", "Unique key"],
  ["extensionStorage.name", "Key"],
  ["extensionStorage.value", "Value"],
  ["indexedDB.autoIncrement", "Auto Increment"],
  ["indexedDB.db", "Database Name"],
  ["indexedDB.indexes", "Indexes"],
  ["indexedDB.keyPath", "Key Path"],
  ["indexedDB.name", "Key"],
  ["indexedDB.objectStore", "Object Store Name"],
  ["indexedDB.objectStores", "Object Stores"],
  ["indexedDB.origin", "Origin"],
  ["indexedDB.storage", "Storage"],
  ["indexedDB.uniqueKey", "Unique key"],
  ["indexedDB.value", "Value"],
  ["indexedDB.version", "Version"],
  ["localStorage.name", "Key"],
  ["localStorage.value", "Value"],
  ["sessionStorage.name", "Key"],
  ["sessionStorage.value", "Value"],
]);

// If a l10n ID has been changed since it was created we store it in this map
// along with it's new value for easy reference.
const NON_ORIGINAL_L10N_IDS = new Map([
  ["cookies.expires", "cookies.expires2"],
  ["cookies.lastAccessed", "cookies.lastAccessed2"],
  ["cookies.creationTime", "cookies.creationTime2"],
  ["indexedDB.keyPath", "indexedDB.keyPath2"],
]);

/**
 * StorageUI is controls and builds the UI of the Storage Inspector.
 *
 * @param {Window} panelWin
 *        Window of the toolbox panel to populate UI in.
 * @param {Object} commands
 *        The commands object with all interfaces defined from devtools/shared/commands/
 */
class StorageUI {
  constructor(panelWin, toolbox, commands) {
    EventEmitter.decorate(this);
    this._window = panelWin;
    this._panelDoc = panelWin.document;
    this._toolbox = toolbox;
    this._commands = commands;
    this.sidebarToggledOpen = null;
    this.shouldLoadMoreItems = true;

    const treeNode = this._panelDoc.getElementById("storage-tree");
    this.tree = new TreeWidget(treeNode, {
      defaultType: "dir",
      contextMenuId: "storage-tree-popup",
    });
    this.onHostSelect = this.onHostSelect.bind(this);
    this.tree.on("select", this.onHostSelect);

    const tableNode = this._panelDoc.getElementById("storage-table");
    this.table = new TableWidget(tableNode, {
      emptyText: "storage-table-empty-text",
      highlightUpdated: true,
      cellContextMenuId: "storage-table-popup",
      l10n: this._panelDoc.l10n,
    });

    this.updateObjectSidebar = this.updateObjectSidebar.bind(this);
    this.table.on(TableWidget.EVENTS.ROW_SELECTED, this.updateObjectSidebar);

    this.handleScrollEnd = this.handleScrollEnd.bind(this);
    this.table.on(TableWidget.EVENTS.SCROLL_END, this.handleScrollEnd);

    this.editItem = this.editItem.bind(this);
    this.table.on(TableWidget.EVENTS.CELL_EDIT, this.editItem);

    this.sidebar = this._panelDoc.getElementById("storage-sidebar");
    this.sidebar.setAttribute("width", "300");
    this.view = new VariablesView(
      this.sidebar.firstChild,
      GENERIC_VARIABLES_VIEW_SETTINGS
    );

    this.filterItems = this.filterItems.bind(this);
    this.onPaneToggleButtonClicked = this.onPaneToggleButtonClicked.bind(this);
    this.setupToolbar();

    const shortcuts = new KeyShortcuts({
      window: this._panelDoc.defaultView,
    });
    const key = L10N.getStr("storage.filter.key");
    shortcuts.on(key, event => {
      event.preventDefault();
      this.searchBox.focus();
    });

    this.handleKeypress = this.handleKeypress.bind(this);
    this._panelDoc.addEventListener("keypress", this.handleKeypress);

    this.onTreePopupShowing = this.onTreePopupShowing.bind(this);
    this._treePopup = this._panelDoc.getElementById("storage-tree-popup");
    this._treePopup.addEventListener("popupshowing", this.onTreePopupShowing);

    this.onTablePopupShowing = this.onTablePopupShowing.bind(this);
    this._tablePopup = this._panelDoc.getElementById("storage-table-popup");
    this._tablePopup.addEventListener("popupshowing", this.onTablePopupShowing);

    this.onVariableViewPopupShowing = this.onVariableViewPopupShowing.bind(
      this
    );
    this._variableViewPopup = this._panelDoc.getElementById(
      "variable-view-popup"
    );
    this._variableViewPopup.addEventListener(
      "popupshowing",
      this.onVariableViewPopupShowing
    );

    this.onRefreshTable = this.onRefreshTable.bind(this);
    this.onAddItem = this.onAddItem.bind(this);
    this.onCopyItem = this.onCopyItem.bind(this);
    this.onRemoveItem = this.onRemoveItem.bind(this);
    this.onRemoveAllFrom = this.onRemoveAllFrom.bind(this);
    this.onRemoveAll = this.onRemoveAll.bind(this);
    this.onRemoveAllSessionCookies = this.onRemoveAllSessionCookies.bind(this);
    this.onRemoveTreeItem = this.onRemoveTreeItem.bind(this);

    this._refreshButton = this._panelDoc.getElementById("refresh-button");
    this._refreshButton.addEventListener("click", this.onRefreshTable);
    this._refreshButton.setAttribute(
      "title",
      L10N.getFormatStr("storage.popupMenu.refreshItemLabel")
    );

    this._addButton = this._panelDoc.getElementById("add-button");
    this._addButton.addEventListener("click", this.onAddItem);

    this._variableViewPopupCopy = this._panelDoc.getElementById(
      "variable-view-popup-copy"
    );
    this._variableViewPopupCopy.addEventListener("command", this.onCopyItem);

    this._tablePopupAddItem = this._panelDoc.getElementById(
      "storage-table-popup-add"
    );
    this._tablePopupAddItem.addEventListener("command", this.onAddItem);

    this._tablePopupDelete = this._panelDoc.getElementById(
      "storage-table-popup-delete"
    );
    this._tablePopupDelete.addEventListener("command", this.onRemoveItem);

    this._tablePopupDeleteAllFrom = this._panelDoc.getElementById(
      "storage-table-popup-delete-all-from"
    );
    this._tablePopupDeleteAllFrom.addEventListener(
      "command",
      this.onRemoveAllFrom
    );

    this._tablePopupDeleteAll = this._panelDoc.getElementById(
      "storage-table-popup-delete-all"
    );
    this._tablePopupDeleteAll.addEventListener("command", this.onRemoveAll);

    this._tablePopupDeleteAllSessionCookies = this._panelDoc.getElementById(
      "storage-table-popup-delete-all-session-cookies"
    );
    this._tablePopupDeleteAllSessionCookies.addEventListener(
      "command",
      this.onRemoveAllSessionCookies
    );

    this._treePopupDeleteAll = this._panelDoc.getElementById(
      "storage-tree-popup-delete-all"
    );
    this._treePopupDeleteAll.addEventListener("command", this.onRemoveAll);

    this._treePopupDeleteAllSessionCookies = this._panelDoc.getElementById(
      "storage-tree-popup-delete-all-session-cookies"
    );
    this._treePopupDeleteAllSessionCookies.addEventListener(
      "command",
      this.onRemoveAllSessionCookies
    );

    this._treePopupDelete = this._panelDoc.getElementById(
      "storage-tree-popup-delete"
    );
    this._treePopupDelete.addEventListener("command", this.onRemoveTreeItem);
  }

  get currentTarget() {
    return this._commands.targetCommand.targetFront;
  }

  async init() {
    // This is a distionarry of arrays, keyed by storage key
    // - Keys are storage keys, available on each storage resource, via ${resource.resourceKey}
    //   and are typically "Cache", "cookies", "indexedDB", "localStorage", ...
    // - Values are arrays of storage fronts. This isn't the deprecated global storage front (target.getFront(storage), only used by legacy listener),
    //   but rather the storage specific front, i.e. a storage resource. Storage resources are fronts.
    this.storageResources = {};

    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
    await this._commands.targetCommand.watchTargets(
      [this._commands.targetCommand.TYPES.FRAME],
      this._onTargetAvailable,
      this._onTargetDestroyed
    );

    this._onResourceListAvailable = this._onResourceListAvailable.bind(this);

    const { resourceCommand } = this._toolbox;
    await this._toolbox.resourceCommand.watchResources(
      [
        // The first item in this list will be the first selected storage item
        // Tests assume Cookie -- moving cookie will break tests
        resourceCommand.TYPES.COOKIE,
        resourceCommand.TYPES.CACHE_STORAGE,
        resourceCommand.TYPES.EXTENSION_STORAGE,
        resourceCommand.TYPES.INDEXED_DB,
        resourceCommand.TYPES.LOCAL_STORAGE,
        resourceCommand.TYPES.SESSION_STORAGE,
      ],
      {
        onAvailable: this._onResourceListAvailable,
      }
    );
  }

  async _onTargetAvailable({ targetFront }) {
    // Only support top level target and navigation to new processes.
    // i.e. ignore additional targets created for remote <iframes>
    if (!targetFront.isTopLevel) {
      return;
    }

    this.front = await targetFront.getFront("storage");
  }

  async _onResourceListAvailable(resources) {
    for (const resource of resources) {
      const { resourceKey } = resource;

      // NOTE: We might be getting more than 1 resource per storage type when
      //       we have remote frames, so we need an array to store these.
      if (!this.storageResources[resourceKey]) {
        this.storageResources[resourceKey] = [];
      }
      this.storageResources[resourceKey].push(resource);
      resource.on(
        "single-store-update",
        this._onStoreUpdate.bind(this, resource)
      );
      resource.on(
        "single-store-cleared",
        this._onStoreCleared.bind(this, resource)
      );
    }

    try {
      await this.populateStorageTree();
    } catch (e) {
      if (!this._toolbox || this._toolbox._destroyer) {
        // The toolbox is in the process of being destroyed... in this case throwing here
        // is expected and normal so let's ignore the error.
        return;
      }

      // The toolbox is open so the error is unexpected and real so let's log it.
      console.error(e);
    }
  }

  _onTargetDestroyed({ targetFront }) {
    // Remove all storages related to this target
    for (const type in this.storageResources) {
      this.storageResources[type] = this.storageResources[type].filter(
        storage => {
          // Note that the storage front may already be destroyed,
          // and have a null targetFront attribute. So also remove all already destroyed fronts.
          return !storage.isDestroyed() && storage.targetFront != targetFront;
        }
      );
    }

    // Only support top level target and navigation to new processes.
    // i.e. ignore additional targets created for remote <iframes>
    if (!targetFront.isTopLevel) {
      return;
    }

    this.storageResources = {};
    this.table.clear();
    this.hideSidebar();
    this.tree.clear();
  }

  set animationsEnabled(value) {
    this._panelDoc.documentElement.classList.toggle("no-animate", !value);
  }

  destroy() {
    const { resourceCommand } = this._toolbox;
    resourceCommand.unwatchResources(
      [
        resourceCommand.TYPES.COOKIE,
        resourceCommand.TYPES.CACHE_STORAGE,
        resourceCommand.TYPES.EXTENSION_STORAGE,
        resourceCommand.TYPES.INDEXED_DB,
        resourceCommand.TYPES.LOCAL_STORAGE,
        resourceCommand.TYPES.SESSION_STORAGE,
      ],
      {
        onAvailable: this._onResourceListAvailable,
      }
    );

    this.table.off(TableWidget.EVENTS.ROW_SELECTED, this.updateObjectSidebar);
    this.table.off(TableWidget.EVENTS.SCROLL_END, this.handleScrollEnd);
    this.table.off(TableWidget.EVENTS.CELL_EDIT, this.editItem);
    this.table.destroy();

    this._panelDoc.removeEventListener("keypress", this.handleKeypress);
    this.searchBox.removeEventListener("input", this.filterItems);
    this.searchBox = null;

    this.sidebarToggleBtn.removeEventListener(
      "click",
      this.onPaneToggleButtonClicked
    );
    this.sidebarToggleBtn = null;

    this._treePopup.removeEventListener(
      "popupshowing",
      this.onTreePopupShowing
    );
    this._refreshButton.removeEventListener("click", this.onRefreshTable);
    this._addButton.removeEventListener("click", this.onAddItem);
    this._tablePopupAddItem.removeEventListener("command", this.onAddItem);
    this._treePopupDeleteAll.removeEventListener("command", this.onRemoveAll);
    this._treePopupDeleteAllSessionCookies.removeEventListener(
      "command",
      this.onRemoveAllSessionCookies
    );
    this._treePopupDelete.removeEventListener("command", this.onRemoveTreeItem);

    this._tablePopup.removeEventListener(
      "popupshowing",
      this.onTablePopupShowing
    );
    this._tablePopupDelete.removeEventListener("command", this.onRemoveItem);
    this._tablePopupDeleteAllFrom.removeEventListener(
      "command",
      this.onRemoveAllFrom
    );
    this._tablePopupDeleteAll.removeEventListener("command", this.onRemoveAll);
    this._tablePopupDeleteAllSessionCookies.removeEventListener(
      "command",
      this.onRemoveAllSessionCookies
    );
  }

  setupToolbar() {
    this.searchBox = this._panelDoc.getElementById("storage-searchbox");
    this.searchBox.addEventListener("input", this.filterItems);

    // Setup the sidebar toggle button.
    this.sidebarToggleBtn = this._panelDoc.querySelector(".sidebar-toggle");
    this.updateSidebarToggleButton();

    this.sidebarToggleBtn.addEventListener(
      "click",
      this.onPaneToggleButtonClicked
    );
  }

  onPaneToggleButtonClicked() {
    if (this.sidebar.hidden && this.table.selectedRow) {
      this.sidebar.hidden = false;
      this.sidebarToggledOpen = true;
      this.updateSidebarToggleButton();
    } else {
      this.sidebarToggledOpen = false;
      this.hideSidebar();
    }
  }

  updateSidebarToggleButton() {
    let title;
    this.sidebarToggleBtn.hidden = !this.table.hasSelectedRow;

    if (this.sidebar.hidden) {
      this.sidebarToggleBtn.classList.add("pane-collapsed");
      title = L10N.getStr("storage.expandPane");
    } else {
      this.sidebarToggleBtn.classList.remove("pane-collapsed");
      title = L10N.getStr("storage.collapsePane");
    }

    this.sidebarToggleBtn.setAttribute("title", title);
  }

  /**
   * Hide the object viewer sidebar
   */
  hideSidebar() {
    this.sidebar.hidden = true;
    this.updateSidebarToggleButton();
  }

  getCurrentFront() {
    const { datatype, host } = this.table;
    return this._getStorage(datatype, host);
  }

  _getStorage(type, host) {
    const storageType = this.storageResources[type];
    return storageType.find(x => host in x.hosts);
  }

  /**
   *  Make column fields editable
   *
   *  @param {Array} editableFields
   *         An array of keys of columns to be made editable
   */
  makeFieldsEditable(editableFields) {
    if (editableFields && editableFields.length > 0) {
      this.table.makeFieldsEditable(editableFields);
    } else if (this.table._editableFieldsEngine) {
      this.table._editableFieldsEngine.destroy();
    }
  }

  editItem(data) {
    const selectedItem = this.tree.selectedItem;
    if (!selectedItem) {
      return;
    }
    const front = this.getCurrentFront();

    front.editItem(data);
  }

  /**
   * Removes the given item from the storage table. Reselects the next item in
   * the table and repopulates the sidebar with that item's data if the item
   * being removed was selected.
   */
  async removeItemFromTable(name) {
    if (this.table.isSelected(name) && this.table.items.size > 1) {
      if (this.table.selectedIndex == 0) {
        this.table.selectNextRow();
      } else {
        this.table.selectPreviousRow();
      }
    }

    this.table.remove(name);
    await this.updateObjectSidebar();
  }

  /**
   * Event handler for "stores-cleared" event coming from the storage actor.
   *
   * @param {object}
   *        An object containing which hosts/paths are cleared from a
   *        storage
   */
  _onStoreCleared(resource, { clearedHostsOrPaths }) {
    const { resourceKey } = resource;
    function* enumPaths() {
      if (Array.isArray(clearedHostsOrPaths)) {
        // Handle the legacy response with array of hosts
        for (const host of clearedHostsOrPaths) {
          yield [host];
        }
      } else {
        // Handle the new format that supports clearing sub-stores in a host
        for (const host in clearedHostsOrPaths) {
          const paths = clearedHostsOrPaths[host];

          if (!paths.length) {
            yield [host];
          } else {
            for (let path of paths) {
              try {
                path = JSON.parse(path);
                yield [host, ...path];
              } catch (ex) {
                // ignore
              }
            }
          }
        }
      }
    }

    for (const path of enumPaths()) {
      // Find if the path is selected (there is max one) and clear it
      if (this.tree.isSelected([resourceKey, ...path])) {
        this.table.clear();
        this.hideSidebar();

        // Reset itemOffset to 0 so that items added after local storate is
        // cleared will be shown
        this.itemOffset = 0;

        this.emit("store-objects-cleared");
        break;
      }
    }
  }

  /**
   * Event handler for "stores-update" event coming from the storage actor.
   *
   * @param {object} argument0
   *        An object containing the details of the added, changed and deleted
   *        storage objects.
   *        Each of these 3 objects are of the following format:
   *        {
   *          <store_type1>: {
   *            <host1>: [<store_names1>, <store_name2>...],
   *            <host2>: [<store_names34>...], ...
   *          },
   *          <store_type2>: {
   *            <host1>: [<store_names1>, <store_name2>...],
   *            <host2>: [<store_names34>...], ...
   *          }, ...
   *        }
   *        Where store_type1 and store_type2 is one of cookies, indexedDB,
   *        sessionStorage and localStorage; host1, host2 are the host in which
   *        this change happened; and [<store_namesX] is an array of the names
   *        of the changed store objects. This array is empty for deleted object
   *        if the host was completely removed.
   */
  async _onStoreUpdate(resource, update) {
    const { changed, added, deleted } = update;
    if (added) {
      await this.handleAddedItems(added);
    }

    if (changed) {
      await this.handleChangedItems(changed);
    }

    // We are dealing with batches of changes here. Deleted **MUST** come last in case it
    // is in the same batch as added and changed events e.g.
    //   - An item is changed then deleted in the same batch: deleted then changed will
    //     display an item that has been deleted.
    //   - An item is added then deleted in the same batch: deleted then added will
    //     display an item that has been deleted.
    if (deleted) {
      await this.handleDeletedItems(deleted);
    }

    if (added || deleted || changed) {
      this.emit("store-objects-edit");
    }
  }

  /**
   * Handle added items received by onEdit
   *
   * @param {object} See onEdit docs
   */
  async handleAddedItems(added) {
    for (const type in added) {
      for (const host in added[type]) {
        const label = this.getReadableLabelFromHostname(host);
        this.tree.add([type, { id: host, label: label, type: "url" }]);
        for (let name of added[type][host]) {
          try {
            name = JSON.parse(name);
            if (name.length == 3) {
              name.splice(2, 1);
            }
            this.tree.add([type, host, ...name]);
            if (!this.tree.selectedItem) {
              this.tree.selectedItem = [type, host, name[0], name[1]];
              await this.fetchStorageObjects(
                type,
                host,
                [JSON.stringify(name)],
                REASON.NEW_ROW
              );
            }
          } catch (ex) {
            // Do nothing
          }
        }

        if (this.tree.isSelected([type, host])) {
          await this.fetchStorageObjects(
            type,
            host,
            added[type][host],
            REASON.NEW_ROW
          );
        }
      }
    }
  }

  /**
   * Handle deleted items received by onEdit
   *
   * @param {object} See onEdit docs
   */
  async handleDeletedItems(deleted) {
    for (const type in deleted) {
      for (const host in deleted[type]) {
        if (!deleted[type][host].length) {
          // This means that the whole host is deleted, thus the item should
          // be removed from the storage tree
          if (this.tree.isSelected([type, host])) {
            this.table.clear();
            this.hideSidebar();
            this.tree.selectPreviousItem();
          }

          this.tree.remove([type, host]);
        } else {
          for (const name of deleted[type][host]) {
            try {
              if (["indexedDB", "Cache"].includes(type)) {
                // For indexedDB and Cache, the key is being parsed because
                // these storages are represented as a tree and the key
                // used to notify their changes is not a simple string.
                const names = JSON.parse(name);
                // Is a whole cache, database or objectstore deleted?
                // Then remove it from the tree.
                if (names.length < 3) {
                  if (this.tree.isSelected([type, host, ...names])) {
                    this.table.clear();
                    this.hideSidebar();
                    this.tree.selectPreviousItem();
                  }
                  this.tree.remove([type, host, ...names]);
                }

                // Remove the item from table if currently displayed.
                if (names.length > 0) {
                  const tableItemName = names.pop();
                  if (this.tree.isSelected([type, host, ...names])) {
                    await this.removeItemFromTable(tableItemName);
                  }
                }
              } else if (this.tree.isSelected([type, host])) {
                // For all the other storage types with a simple string key,
                // remove the item from the table by name without any parsing.
                await this.removeItemFromTable(name);
              }
            } catch (ex) {
              if (this.tree.isSelected([type, host])) {
                await this.removeItemFromTable(name);
              }
            }
          }
        }
      }
    }
  }

  /**
   * Handle changed items received by onEdit
   *
   * @param {object} See onEdit docs
   */
  async handleChangedItems(changed) {
    const selectedItem = this.tree.selectedItem;
    if (!selectedItem) {
      return;
    }

    const [type, host, db, objectStore] = selectedItem;
    if (
      !changed[type] ||
      !changed[type][host] ||
      changed[type][host].length == 0
    ) {
      return;
    }
    try {
      const toUpdate = [];
      for (const name of changed[type][host]) {
        if (["indexedDB", "Cache"].includes(type)) {
          // For indexedDB and Cache, the key is being parsed because
          // these storage are represented as a tree and the key
          // used to notify their changes is not a simple string.
          const names = JSON.parse(name);
          if (names[0] == db && names[1] == objectStore && names[2]) {
            toUpdate.push(name);
          }
        } else {
          // For all the other storage types with a simple string key,
          // update the item from the table by name without any parsing.
          toUpdate.push(name);
        }
      }
      await this.fetchStorageObjects(type, host, toUpdate, REASON.UPDATE);
    } catch (ex) {
      await this.fetchStorageObjects(
        type,
        host,
        changed[type][host],
        REASON.UPDATE
      );
    }
  }

  /**
   * Fetches the storage objects from the storage actor and populates the
   * storage table with the returned data.
   *
   * @param {string} type
   *        The type of storage. Ex. "cookies"
   * @param {string} host
   *        Hostname
   * @param {array} names
   *        Names of particular store objects. Empty if all are requested
   * @param {Constant} reason
   *        See REASON constant at top of file.
   */
  async fetchStorageObjects(type, host, names, reason) {
    const fetchOpts =
      reason === REASON.NEXT_50_ITEMS ? { offset: this.itemOffset } : {};
    const storage = this._getStorage(type, host);
    this.sidebarToggledOpen = null;

    if (
      reason !== REASON.NEXT_50_ITEMS &&
      reason !== REASON.UPDATE &&
      reason !== REASON.NEW_ROW &&
      reason !== REASON.POPULATE
    ) {
      throw new Error("Invalid reason specified");
    }

    try {
      if (
        reason === REASON.POPULATE ||
        (reason === REASON.NEW_ROW && this.table.items.size === 0)
      ) {
        let subType = null;
        // The indexedDB type could have sub-type data to fetch.
        // If having names specified, then it means
        // we are fetching details of specific database or of object store.
        if (type === "indexedDB" && names) {
          const [dbName, objectStoreName] = JSON.parse(names[0]);
          if (dbName) {
            subType = "database";
          }
          if (objectStoreName) {
            subType = "object store";
          }
        }

        await this.resetColumns(type, host, subType);
      }

      const { data } = await storage.getStoreObjects(host, names, fetchOpts);
      if (data.length) {
        await this.populateTable(data, reason);
      } else if (reason === REASON.POPULATE) {
        await this.clearHeaders();
      }
      this.updateToolbar();
      this.emit("store-objects-updated");
    } catch (ex) {
      console.error(ex);
    }
  }

  supportsAddItem(type, host) {
    const storage = this._getStorage(type, host);
    return storage?.traits.supportsAddItem || false;
  }

  supportsRemoveItem(type, host) {
    const storage = this._getStorage(type, host);
    return storage?.traits.supportsRemoveItem || false;
  }

  supportsRemoveAll(type, host) {
    const storage = this._getStorage(type, host);
    return storage?.traits.supportsRemoveAll || false;
  }

  supportsRemoveAllSessionCookies(type, host) {
    const storage = this._getStorage(type, host);
    return storage?.traits.supportsRemoveAllSessionCookies || false;
  }

  /**
   * Updates the toolbar hiding and showing buttons as appropriate.
   */
  updateToolbar() {
    const item = this.tree.selectedItem;
    if (!item) {
      return;
    }

    const [type, host] = item;

    // Add is only supported if the selected item has a host.
    const canAdd = this.supportsAddItem(type, host) && host;

    if (canAdd) {
      this._addButton.hidden = false;
      this._addButton.setAttribute(
        "title",
        L10N.getFormatStr("storage.popupMenu.addItemLabel")
      );
    } else {
      this._addButton.hidden = true;
      this._addButton.removeAttribute("title");
    }
  }

  /**
   * Populates the storage tree which displays the list of storages present for
   * the page.
   */
  async populateStorageTree() {
    const populateTreeFromResource = (type, resource) => {
      for (const host in resource.hosts) {
        const label = this.getReadableLabelFromHostname(host);
        this.tree.add([type, { id: host, label: label, type: "url" }]);
        for (const name of resource.hosts[host]) {
          try {
            const names = JSON.parse(name);
            this.tree.add([type, host, ...names]);
            if (!this.tree.selectedItem) {
              this.tree.selectedItem = [type, host, names[0], names[1]];
            }
          } catch (ex) {
            // Do Nothing
          }
        }
        if (!this.tree.selectedItem) {
          this.tree.selectedItem = [type, host];
        }
      }
    };

    // When can we expect the "store-objects-updated" event?
    //   -> TreeWidget setter `selectedItem` emits a "select" event
    //   -> on tree "select" event, this module calls `onHostSelect`
    //   -> finally `onHostSelect` calls `fetchStorageObjects`, which will emit
    //      "store-objects-updated" at the end of the method.
    // So if the selection changed, we can wait for "store-objects-updated",
    // which is emitted at the end of `fetchStorageObjects`.
    const onStoresObjectsUpdated = this.once("store-objects-updated");

    // Save the initially selected item to check if tree.selected was updated,
    // see comment above.
    const initialSelectedItem = this.tree.selectedItem;

    for (const [type, resources] of Object.entries(this.storageResources)) {
      let typeLabel = type;
      try {
        typeLabel = L10N.getStr("tree.labels." + type);
      } catch (e) {
        console.error("Unable to localize tree label type:" + type);
      }

      this.tree.add([{ id: type, label: typeLabel, type: "store" }]);

      // storageResources values are arrays, with storage resources.
      // we may have many storage resources per type if we get remote iframes.
      for (const resource of resources) {
        populateTreeFromResource(type, resource);
      }
    }

    if (initialSelectedItem !== this.tree.selectedItem) {
      await onStoresObjectsUpdated;
    }
  }

  /**
   * Populates the selected entry from the table in the sidebar for a more
   * detailed view.
   */
  /* eslint-disable-next-line */
  async updateObjectSidebar() {
    const item = this.table.selectedRow;
    let value;

    // Get the string value (async action) and the update the UI synchronously.
    if ((item?.name || item?.name === "") && item?.valueActor) {
      value = await item.valueActor.string();
    }

    // Bail if the selectedRow is no longer selected, the item doesn't exist or the state
    // changed in another way during the above yield.
    if (
      this.table.items.size === 0 ||
      !item ||
      !this.table.selectedRow ||
      item.uniqueKey !== this.table.selectedRow.uniqueKey
    ) {
      this.hideSidebar();
      return;
    }

    // Start updating the UI. Everything is sync beyond this point.
    if (this.sidebarToggledOpen === null || this.sidebarToggledOpen === true) {
      this.sidebar.hidden = false;
    }

    this.updateSidebarToggleButton();
    this.view.empty();
    const mainScope = this.view.addScope(L10N.getStr("storage.data.label"));
    mainScope.expanded = true;

    if (value) {
      const itemVar = mainScope.addItem(item.name + "", {}, { relaxed: true });

      // The main area where the value will be displayed
      itemVar.setGrip(value);

      // May be the item value is a json or a key value pair itself
      const obj = parseItemValue(value);
      if (typeof obj === "object") {
        this.populateSidebar(item.name, obj);
      }

      // By default the item name and value are shown. If this is the only
      // information available, then nothing else is to be displayed.
      const itemProps = Object.keys(item);
      if (itemProps.length > 3) {
        // Display any other information other than the item name and value
        // which may be available.
        const rawObject = Object.create(null);
        const otherProps = itemProps.filter(
          e => !["name", "value", "valueActor"].includes(e)
        );
        for (const prop of otherProps) {
          const column = this.table.columns.get(prop);
          if (column?.private) {
            continue;
          }

          const fieldName = getColumnName(this.table.datatype, prop);
          rawObject[fieldName] = item[prop];
        }
        itemVar.populate(rawObject, { sorted: true });
        itemVar.twisty = true;
        itemVar.expanded = true;
      }
    } else {
      // Case when displaying IndexedDB db/object store properties.
      for (const key in item) {
        const column = this.table.columns.get(key);
        if (column?.private) {
          continue;
        }

        mainScope.addItem(key, {}, true).setGrip(item[key]);
        const obj = parseItemValue(item[key]);
        if (typeof obj === "object") {
          this.populateSidebar(item.name, obj);
        }
      }
    }

    this.emit("sidebar-updated");
  }

  /**
   * Gets a readable label from the hostname. If the hostname is a Punycode
   * domain(I.e. an ASCII domain name representing a Unicode domain name), then
   * this function decodes it to the readable Unicode domain name, and label
   * the Unicode domain name toggether with the original domian name, and then
   * return the label; if the hostname isn't a Punycode domain(I.e. it isn't
   * encoded and is readable on its own), then this function simply returns the
   * original hostname.
   *
   * @param {string} host
   *        The string representing a host, e.g, example.com, example.com:8000
   */
  getReadableLabelFromHostname(host) {
    try {
      const { hostname } = new URL(host);
      const unicodeHostname = getUnicodeHostname(hostname);
      if (hostname !== unicodeHostname) {
        // If the hostname is a Punycode domain representing a Unicode domain,
        // we decode it to the Unicode domain name, and then label the Unicode
        // domain name together with the original domain name.
        return host.replace(hostname, unicodeHostname) + " [ " + host + " ]";
      }
    } catch (_) {
      // Skip decoding for a host which doesn't include a domain name, simply
      // consider them to be readable.
    }
    return host;
  }

  /**
   * Populates the sidebar with a parsed object.
   *
   * @param {object} obj - Either a json or a key-value separated object or a
   * key separated array
   */
  populateSidebar(name, obj) {
    const jsonObject = Object.create(null);
    const view = this.view;
    jsonObject[name] = obj;
    const valueScope =
      view.getScopeAtIndex(1) ||
      view.addScope(L10N.getStr("storage.parsedValue.label"));
    valueScope.expanded = true;
    const jsonVar = valueScope.addItem("", Object.create(null), {
      relaxed: true,
    });
    jsonVar.expanded = true;
    jsonVar.twisty = true;
    jsonVar.populate(jsonObject, { expanded: true });
  }

  /**
   * Select handler for the storage tree. Fetches details of the selected item
   * from the storage details and populates the storage tree.
   *
   * @param {array} item
   *        An array of ids which represent the location of the selected item in
   *        the storage tree
   */
  async onHostSelect(item) {
    if (!item) {
      return;
    }

    this.table.clear();
    this.hideSidebar();
    this.searchBox.value = "";

    const [type, host] = item;
    this.table.host = host;
    this.table.datatype = type;

    this.updateToolbar();

    let names = null;
    if (!host) {
      let storageTypeHintL10nId = "";
      switch (type) {
        case "Cache":
          storageTypeHintL10nId = "storage-table-type-cache-hint";
          break;
        case "cookies":
          storageTypeHintL10nId = "storage-table-type-cookies-hint";
          break;
        case "extensionStorage":
          storageTypeHintL10nId = "storage-table-type-extensionstorage-hint";
          break;
        case "localStorage":
          storageTypeHintL10nId = "storage-table-type-localstorage-hint";
          break;
        case "indexedDB":
          storageTypeHintL10nId = "storage-table-type-indexeddb-hint";
          break;
        case "sessionStorage":
          storageTypeHintL10nId = "storage-table-type-sessionstorage-hint";
          break;
      }
      this.table.setPlaceholder(
        storageTypeHintL10nId,
        getStorageTypeURL(this.table.datatype) +
          "?utm_source=devtools&utm_medium=storage-inspector"
      );

      // If selected item has no host then reset table headers
      await this.clearHeaders();
      return;
    }
    if (item.length > 2) {
      names = [JSON.stringify(item.slice(2))];
    }
    await this.fetchStorageObjects(type, host, names, REASON.POPULATE);
    this.itemOffset = 0;
  }

  /**
   * Clear the column headers in the storage table
   */
  async clearHeaders() {
    this.table.setColumns({}, null, {}, {});
  }

  /**
   * Resets the column headers in the storage table with the pased object `data`
   *
   * @param {string} type
   *        The type of storage corresponding to the after-reset columns in the
   *        table.
   * @param {string} host
   *        The host name corresponding to the table after reset.
   *
   * @param {string} [subType]
   *        The sub type under the given type.
   */
  async resetColumns(type, host, subtype) {
    this.table.host = host;
    this.table.datatype = type;

    let uniqueKey = null;
    const columns = {};
    const editableFields = [];
    const hiddenFields = [];
    const privateFields = [];
    const fields = await this.getCurrentFront().getFields(subtype);

    fields.forEach(f => {
      if (!uniqueKey) {
        this.table.uniqueId = uniqueKey = f.name;
      }

      if (f.editable) {
        editableFields.push(f.name);
      }

      if (f.hidden) {
        hiddenFields.push(f.name);
      }

      if (f.private) {
        privateFields.push(f.name);
      }

      const columnName = getColumnName(type, f.name);
      if (columnName) {
        columns[f.name] = columnName;
      } else if (!f.private) {
        // Private fields are only displayed when running tests so there is no
        // need to log an error if they are not localized.
        columns[f.name] = f.name;
        console.error(
          `No string defined in NON_L10N_STRINGS for '${type}.${f.name}'`
        );
      }
    });

    this.table.setColumns(columns, null, hiddenFields, privateFields);
    this.hideSidebar();

    this.makeFieldsEditable(editableFields);
  }

  /**
   * Populates or updates the rows in the storage table.
   *
   * @param {array[object]} data
   *        Array of objects to be populated in the storage table
   * @param {Constant} reason
   *        See REASON constant at top of file.
   */
  async populateTable(data, reason) {
    for (const item of data) {
      if (item.value) {
        item.valueActor = item.value;
        item.value = item.value.initial || "";
      }
      if (item.expires != null) {
        item.expires = item.expires
          ? new Date(item.expires).toUTCString()
          : L10N.getStr("label.expires.session");
      }
      if (item.creationTime != null) {
        item.creationTime = new Date(item.creationTime).toUTCString();
      }
      if (item.lastAccessed != null) {
        item.lastAccessed = new Date(item.lastAccessed).toUTCString();
      }

      switch (reason) {
        case REASON.POPULATE:
        case REASON.NEXT_50_ITEMS:
          // Update without flashing the row.
          this.table.push(item, true);
          break;
        case REASON.NEW_ROW:
          // Update and flash the row.
          this.table.push(item, false);
          break;
        case REASON.UPDATE:
          this.table.update(item);
          if (item == this.table.selectedRow && !this.sidebar.hidden) {
            await this.updateObjectSidebar();
          }
          break;
      }

      this.shouldLoadMoreItems = true;
    }
  }

  /**
   * Handles keypress event on the body table to close the sidebar when open
   *
   * @param {DOMEvent} event
   *        The event passed by the keypress event.
   */
  handleKeypress(event) {
    if (event.keyCode == KeyCodes.DOM_VK_ESCAPE) {
      if (!this.sidebar.hidden) {
        this.hideSidebar();
        this.sidebarToggledOpen = false;
        // Stop Propagation to prevent opening up of split console
        event.stopPropagation();
        event.preventDefault();
      }
    } else if (
      event.keyCode == KeyCodes.DOM_VK_BACK_SPACE ||
      event.keyCode == KeyCodes.DOM_VK_DELETE
    ) {
      if (this.table.selectedRow && event.target.localName != "input") {
        this.onRemoveItem(event);
        event.stopPropagation();
        event.preventDefault();
      }
    }
  }

  /**
   * Handles filtering the table
   */
  filterItems() {
    const value = this.searchBox.value;
    this.table.filterItems(value, ["valueActor"]);
    this._panelDoc.documentElement.classList.toggle("filtering", !!value);
  }

  /**
   * Handles endless scrolling for the table
   */
  async handleScrollEnd() {
    if (!this.shouldLoadMoreItems) {
      return;
    }
    this.shouldLoadMoreItems = false;
    this.itemOffset += 50;

    const item = this.tree.selectedItem;
    const [type, host] = item;
    let names = null;
    if (item.length > 2) {
      names = [JSON.stringify(item.slice(2))];
    }
    await this.fetchStorageObjects(type, host, names, REASON.NEXT_50_ITEMS);
  }

  /**
   * Fires before a cell context menu with the "Add" or "Delete" action is
   * shown. If the currently selected storage object doesn't support adding or
   * removing items, prevent showing the menu.
   */
  onTablePopupShowing(event) {
    const selectedItem = this.tree.selectedItem;
    const [type, host] = selectedItem;

    // IndexedDB only supports removing items from object stores (level 4 of the tree)
    if (
      (!this.supportsAddItem(type, host) &&
        !this.supportsRemoveItem(type, host)) ||
      (type === "indexedDB" && selectedItem.length !== 4)
    ) {
      event.preventDefault();
      return;
    }

    const rowId = this.table.contextMenuRowId;
    const data = this.table.items.get(rowId);

    if (this.supportsRemoveItem(type, host)) {
      const name = data[this.table.uniqueId];
      const separatorRegex = new RegExp(SEPARATOR_GUID, "g");
      const label = addEllipsis((name + "").replace(separatorRegex, "-"));

      this._tablePopupDelete.hidden = false;
      this._tablePopupDelete.setAttribute(
        "label",
        L10N.getFormatStr("storage.popupMenu.deleteLabel", label)
      );
    } else {
      this._tablePopupDelete.hidden = true;
    }

    if (this.supportsAddItem(type, host)) {
      this._tablePopupAddItem.hidden = false;
      this._tablePopupAddItem.setAttribute(
        "label",
        L10N.getFormatStr("storage.popupMenu.addItemLabel")
      );
    } else {
      this._tablePopupAddItem.hidden = true;
    }

    let showDeleteAllSessionCookies = false;
    if (this.supportsRemoveAllSessionCookies(type, host)) {
      if (selectedItem.length === 2) {
        showDeleteAllSessionCookies = true;
      }
    }

    this._tablePopupDeleteAllSessionCookies.hidden = !showDeleteAllSessionCookies;

    if (type === "cookies") {
      const hostString = addEllipsis(data.host);

      this._tablePopupDeleteAllFrom.hidden = false;
      this._tablePopupDeleteAllFrom.setAttribute(
        "label",
        L10N.getFormatStr("storage.popupMenu.deleteAllFromLabel", hostString)
      );
    } else {
      this._tablePopupDeleteAllFrom.hidden = true;
    }
  }

  onTreePopupShowing(event) {
    let showMenu = false;
    const selectedItem = this.tree.selectedItem;

    if (selectedItem) {
      const [type, host] = selectedItem;

      // The delete all (aka clear) action is displayed for IndexedDB object stores
      // (level 4 of tree), for Cache objects (level 3) and for the whole host (level 2)
      // for other storage types (cookies, localStorage, ...).
      let showDeleteAll = false;
      if (this.supportsRemoveAll(type, host)) {
        let level;
        if (type == "indexedDB") {
          level = 4;
        } else if (type == "Cache") {
          level = 3;
        } else {
          level = 2;
        }

        if (selectedItem.length == level) {
          showDeleteAll = true;
        }
      }

      this._treePopupDeleteAll.hidden = !showDeleteAll;

      // The delete all session cookies action is displayed for cookie object stores
      // (level 2 of tree)
      let showDeleteAllSessionCookies = false;
      if (this.supportsRemoveAllSessionCookies(type, host)) {
        if (type === "cookies" && selectedItem.length === 2) {
          showDeleteAllSessionCookies = true;
        }
      }

      this._treePopupDeleteAllSessionCookies.hidden = !showDeleteAllSessionCookies;

      // The delete action is displayed for:
      // - IndexedDB databases (level 3 of the tree)
      // - Cache objects (level 3 of the tree)
      const showDelete =
        (type == "indexedDB" || type == "Cache") && selectedItem.length == 3;
      this._treePopupDelete.hidden = !showDelete;
      if (showDelete) {
        const itemName = addEllipsis(selectedItem[selectedItem.length - 1]);
        this._treePopupDelete.setAttribute(
          "label",
          L10N.getFormatStr("storage.popupMenu.deleteLabel", itemName)
        );
      }

      showMenu = showDeleteAll || showDelete;
    }

    if (!showMenu) {
      event.preventDefault();
    }
  }

  onVariableViewPopupShowing(event) {
    const item = this.view.getFocusedItem();
    this._variableViewPopupCopy.setAttribute("disabled", !item);
  }

  /**
   * Handles refreshing the selected storage
   */
  async onRefreshTable() {
    await this.onHostSelect(this.tree.selectedItem);
  }

  /**
   * Handles adding an item from the storage
   */
  onAddItem() {
    const selectedItem = this.tree.selectedItem;
    if (!selectedItem) {
      return;
    }

    const front = this.getCurrentFront();
    const [, host] = selectedItem;

    // Prepare to scroll into view.
    this.table.scrollIntoViewOnUpdate = true;
    this.table.editBookmark = createGUID();
    front.addItem(this.table.editBookmark, host);
  }

  /**
   * Handles copy an item from the storage
   */
  onCopyItem() {
    this.view._copyItem();
  }

  /**
   * Handles removing an item from the storage
   *
   * @param {DOMEvent} event
   *        The event passed by the command or keypress event.
   */
  onRemoveItem(event) {
    const [, host, ...path] = this.tree.selectedItem;
    const front = this.getCurrentFront();
    const uniqueId = this.table.uniqueId;
    const rowId =
      event.type == "command"
        ? this.table.contextMenuRowId
        : this.table.selectedRow[uniqueId];
    const data = this.table.items.get(rowId);

    let name = data[uniqueId];
    if (path.length > 0) {
      name = JSON.stringify([...path, name]);
    }
    front.removeItem(host, name);

    return false;
  }

  /**
   * Handles removing all items from the storage
   */
  onRemoveAll() {
    // Cannot use this.currentActor() if the handler is called from the
    // tree context menu: it returns correct value only after the table
    // data from server are successfully fetched (and that's async).
    const [, host, ...path] = this.tree.selectedItem;
    const front = this.getCurrentFront();
    const name = path.length > 0 ? JSON.stringify(path) : undefined;
    front.removeAll(host, name);
  }

  /**
   * Handles removing all session cookies from the storage
   */
  onRemoveAllSessionCookies() {
    // Cannot use this.currentActor() if the handler is called from the
    // tree context menu: it returns the correct value only after the
    // table data from server is successfully fetched (and that's async).
    const [, host, ...path] = this.tree.selectedItem;
    const front = this.getCurrentFront();
    const name = path.length > 0 ? JSON.stringify(path) : undefined;
    front.removeAllSessionCookies(host, name);
  }

  /**
   * Handles removing all cookies with exactly the same domain as the
   * cookie in the selected row.
   */
  onRemoveAllFrom() {
    const [, host] = this.tree.selectedItem;
    const front = this.getCurrentFront();
    const rowId = this.table.contextMenuRowId;
    const data = this.table.items.get(rowId);

    front.removeAll(host, data.host);
  }

  onRemoveTreeItem() {
    const [type, host, ...path] = this.tree.selectedItem;

    if (type == "indexedDB" && path.length == 1) {
      this.removeDatabase(host, path[0]);
    } else if (type == "Cache" && path.length == 1) {
      this.removeCache(host, path[0]);
    }
  }

  removeDatabase(host, dbName) {
    const front = this.getCurrentFront();

    front
      .removeDatabase(host, dbName)
      .then(result => {
        if (result.blocked) {
          const notificationBox = this._toolbox.getNotificationBox();
          notificationBox.appendNotification(
            L10N.getFormatStr("storage.idb.deleteBlocked", dbName),
            "storage-idb-delete-blocked",
            null,
            notificationBox.PRIORITY_WARNING_LOW
          );
        }
      })
      .catch(error => {
        const notificationBox = this._toolbox.getNotificationBox();
        notificationBox.appendNotification(
          L10N.getFormatStr("storage.idb.deleteError", dbName),
          "storage-idb-delete-error",
          null,
          notificationBox.PRIORITY_CRITICAL_LOW
        );
      });
  }

  removeCache(host, cacheName) {
    const front = this.getCurrentFront();

    front.removeItem(host, JSON.stringify([cacheName]));
  }
}

exports.StorageUI = StorageUI;

// Helper Functions

function createGUID() {
  return "{cccccccc-cccc-4ccc-yccc-cccccccccccc}".replace(/[cy]/g, c => {
    const r = (Math.random() * 16) | 0;
    const v = c == "c" ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}

function addEllipsis(name) {
  if (name.length > ITEM_NAME_MAX_LENGTH) {
    if (/^https?:/.test(name)) {
      // For URLs, add ellipsis in the middle
      const halfLen = ITEM_NAME_MAX_LENGTH / 2;
      return name.slice(0, halfLen) + ELLIPSIS + name.slice(-halfLen);
    }

    // For other strings, add ellipsis at the end
    return name.substr(0, ITEM_NAME_MAX_LENGTH) + ELLIPSIS;
  }

  return name;
}

/**
 * Get a string for a column name automatically choosing whether or not the
 * string should be localized.
 *
 * @param {String} type
 *        The storage type.
 * @param {String} name
 *        The field name that may need to be localized.
 */
function getColumnName(type, name) {
  // Check if a l10n ID has been changed since it was created and map it to
  // its new value if it has.
  let id = `${type}.${name}`;
  id = NON_ORIGINAL_L10N_IDS.get(id) || id;

  // If the ID exists in NON_L10N_STRINGS then we do not translate it
  // otherwise we get it from the L10N database. If it doesn't exist in the
  // database we will just use the field name.
  let columnName = NON_L10N_STRINGS.get(id);
  if (!columnName) {
    try {
      columnName = L10N.getStr(`table.headers.${id}`);
    } catch (e) {
      columnName = name;
    }
  }

  return columnName;
}
