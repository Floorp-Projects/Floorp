/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {LocalizationHelper, ELLIPSIS} = require("devtools/shared/l10n");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const JSOL = require("devtools/client/shared/vendor/jsol");
const {KeyCodes} = require("devtools/client/shared/keycodes");
const { getUnicodeHostname } = require("devtools/client/shared/unicode-url");

// GUID to be used as a separator in compound keys. This must match the same
// constant in devtools/server/actors/storage.js,
// devtools/client/storage/test/head.js and
// devtools/server/tests/browser/head.js
const SEPARATOR_GUID = "{9d414cc5-8319-0a04-0586-c0a6ae01670a}";

loader.lazyRequireGetter(this, "TreeWidget",
                         "devtools/client/shared/widgets/TreeWidget", true);
loader.lazyRequireGetter(this, "TableWidget",
                         "devtools/client/shared/widgets/TableWidget", true);
loader.lazyImporter(this, "VariablesView",
  "resource://devtools/client/shared/widgets/VariablesView.jsm");

/**
 * Localization convenience methods.
 */
const STORAGE_STRINGS = "devtools/client/locales/storage.properties";
const L10N = new LocalizationHelper(STORAGE_STRINGS);

const GENERIC_VARIABLES_VIEW_SETTINGS = {
  lazyEmpty: true,
   // ms
  lazyEmptyDelay: 10,
  searchEnabled: true,
  searchPlaceholder: L10N.getStr("storage.search.placeholder"),
  preventDescriptorModifiers: true
};

const REASON = {
  NEW_ROW: "new-row",
  NEXT_50_ITEMS: "next-50-items",
  POPULATE: "populate",
  UPDATE: "update"
};

const COOKIE_KEY_MAP = {
  path: "Path",
  host: "Domain",
  expires: "Expires",
  isSecure: "Secure",
  isHttpOnly: "HttpOnly",
  isDomain: "HostOnly",
  creationTime: "CreationTime",
  lastAccessed: "LastAccessed"
};

const SAFE_HOSTS_PREFIXES_REGEX = /^(about:|https?:|file:|moz-extension:)/;

// Maximum length of item name to show in context menu label - will be
// trimmed with ellipsis if it's longer.
const ITEM_NAME_MAX_LENGTH = 32;

/**
 * StorageUI is controls and builds the UI of the Storage Inspector.
 *
 * @param {Front} front
 *        Front for the storage actor
 * @param {Target} target
 *        Interface for the page we're debugging
 * @param {Window} panelWin
 *        Window of the toolbox panel to populate UI in.
 */
class StorageUI {
  constructor(front, target, panelWin, toolbox) {
    EventEmitter.decorate(this);

    this._target = target;
    this._window = panelWin;
    this._panelDoc = panelWin.document;
    this._toolbox = toolbox;
    this.front = front;
    this.storageTypes = null;
    this.sidebarToggledOpen = null;
    this.shouldLoadMoreItems = true;

    const treeNode = this._panelDoc.getElementById("storage-tree");
    this.tree = new TreeWidget(treeNode, {
      defaultType: "dir",
      contextMenuId: "storage-tree-popup"
    });
    this.onHostSelect = this.onHostSelect.bind(this);
    this.tree.on("select", this.onHostSelect);

    const tableNode = this._panelDoc.getElementById("storage-table");
    this.table = new TableWidget(tableNode, {
      emptyText: L10N.getStr("table.emptyText"),
      highlightUpdated: true,
      cellContextMenuId: "storage-table-popup"
    });

    this.updateObjectSidebar = this.updateObjectSidebar.bind(this);
    this.table.on(TableWidget.EVENTS.ROW_SELECTED, this.updateObjectSidebar);

    this.handleScrollEnd = this.handleScrollEnd.bind(this);
    this.table.on(TableWidget.EVENTS.SCROLL_END, this.handleScrollEnd);

    this.editItem = this.editItem.bind(this);
    this.table.on(TableWidget.EVENTS.CELL_EDIT, this.editItem);

    this.sidebar = this._panelDoc.getElementById("storage-sidebar");
    this.sidebar.setAttribute("width", "300");
    this.view = new VariablesView(this.sidebar.firstChild,
                                  GENERIC_VARIABLES_VIEW_SETTINGS);

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

    this.front.listStores().then(storageTypes => {
      // When we are in the browser console we list indexedDBs internal to
      // Firefox e.g. defined inside a .jsm. Because there is no way before this
      // point to know whether or not we are inside the browser toolbox we have
      // already fetched the hostnames of these databases.
      //
      // If we are not inside the browser toolbox we need to delete these
      // hostnames.
      if (!this._target.chrome && storageTypes.indexedDB) {
        const hosts = storageTypes.indexedDB.hosts;
        const newHosts = {};

        for (const [host, dbs] of Object.entries(hosts)) {
          if (SAFE_HOSTS_PREFIXES_REGEX.test(host)) {
            newHosts[host] = dbs;
          }
        }

        storageTypes.indexedDB.hosts = newHosts;
      }

      this.populateStorageTree(storageTypes);
    }).catch(e => {
      if (!this._toolbox || this._toolbox._destroyer) {
        // The toolbox is in the process of being destroyed... in this case throwing here
        // is expected and normal so let's ignore the error.
        return;
      }

      // The toolbox is open so the error is unexpected and real so let's log it.
      console.error(e);
    });

    this.onEdit = this.onEdit.bind(this);
    this.front.on("stores-update", this.onEdit);
    this.onCleared = this.onCleared.bind(this);
    this.front.on("stores-cleared", this.onCleared);

    this.handleKeypress = this.handleKeypress.bind(this);
    this._panelDoc.addEventListener("keypress", this.handleKeypress);

    this.onTreePopupShowing = this.onTreePopupShowing.bind(this);
    this._treePopup = this._panelDoc.getElementById("storage-tree-popup");
    this._treePopup.addEventListener("popupshowing", this.onTreePopupShowing);

    this.onTablePopupShowing = this.onTablePopupShowing.bind(this);
    this._tablePopup = this._panelDoc.getElementById("storage-table-popup");
    this._tablePopup.addEventListener("popupshowing", this.onTablePopupShowing);

    this.onRefreshTable = this.onRefreshTable.bind(this);
    this.onAddItem = this.onAddItem.bind(this);
    this.onRemoveItem = this.onRemoveItem.bind(this);
    this.onRemoveAllFrom = this.onRemoveAllFrom.bind(this);
    this.onRemoveAll = this.onRemoveAll.bind(this);
    this.onRemoveAllSessionCookies = this.onRemoveAllSessionCookies.bind(this);
    this.onRemoveTreeItem = this.onRemoveTreeItem.bind(this);

    this._refreshButton = this._panelDoc.getElementById("refresh-button");
    this._refreshButton.addEventListener("command", this.onRefreshTable);

    this._addButton = this._panelDoc.getElementById("add-button");
    this._addButton.addEventListener("command", this.onAddItem);

    this._tablePopupAddItem = this._panelDoc.getElementById(
      "storage-table-popup-add");
    this._tablePopupAddItem.addEventListener("command", this.onAddItem);

    this._tablePopupDelete = this._panelDoc.getElementById(
      "storage-table-popup-delete");
    this._tablePopupDelete.addEventListener("command", this.onRemoveItem);

    this._tablePopupDeleteAllFrom = this._panelDoc.getElementById(
      "storage-table-popup-delete-all-from");
    this._tablePopupDeleteAllFrom.addEventListener("command",
      this.onRemoveAllFrom);

    this._tablePopupDeleteAll = this._panelDoc.getElementById(
      "storage-table-popup-delete-all");
    this._tablePopupDeleteAll.addEventListener("command", this.onRemoveAll);

    this._tablePopupDeleteAllSessionCookies = this._panelDoc.getElementById(
      "storage-table-popup-delete-all-session-cookies");
    this._tablePopupDeleteAllSessionCookies.addEventListener("command",
      this.onRemoveAllSessionCookies);

    this._treePopupDeleteAll = this._panelDoc.getElementById(
      "storage-tree-popup-delete-all");
    this._treePopupDeleteAll.addEventListener("command", this.onRemoveAll);

    this._treePopupDeleteAllSessionCookies = this._panelDoc.getElementById(
      "storage-tree-popup-delete-all-session-cookies");
    this._treePopupDeleteAllSessionCookies.addEventListener("command",
      this.onRemoveAllSessionCookies);

    this._treePopupDelete = this._panelDoc.getElementById("storage-tree-popup-delete");
    this._treePopupDelete.addEventListener("command", this.onRemoveTreeItem);
  }

  set animationsEnabled(value) {
    this._panelDoc.documentElement.classList.toggle("no-animate", !value);
  }

  destroy() {
    this.table.off(TableWidget.EVENTS.ROW_SELECTED, this.updateObjectSidebar);
    this.table.off(TableWidget.EVENTS.SCROLL_END, this.handleScrollEnd);
    this.table.off(TableWidget.EVENTS.CELL_EDIT, this.editItem);
    this.table.destroy();

    this.front.off("stores-update", this.onEdit);
    this.front.off("stores-cleared", this.onCleared);
    this._panelDoc.removeEventListener("keypress", this.handleKeypress);
    this.searchBox.removeEventListener("input", this.filterItems);
    this.searchBox = null;

    this.sidebarToggleBtn.removeEventListener("click", this.onPaneToggleButtonClicked);
    this.sidebarToggleBtn = null;

    this._treePopup.removeEventListener("popupshowing", this.onTreePopupShowing);
    this._refreshButton.removeEventListener("command", this.onRefreshTable);
    this._addButton.removeEventListener("command", this.onAddItem);
    this._tablePopupAddItem.removeEventListener("command", this.onAddItem);
    this._treePopupDeleteAll.removeEventListener("command", this.onRemoveAll);
    this._treePopupDeleteAllSessionCookies.removeEventListener("command",
      this.onRemoveAllSessionCookies);
    this._treePopupDelete.removeEventListener("command", this.onRemoveTreeItem);

    this._tablePopup.removeEventListener("popupshowing", this.onTablePopupShowing);
    this._tablePopupDelete.removeEventListener("command", this.onRemoveItem);
    this._tablePopupDeleteAllFrom.removeEventListener("command", this.onRemoveAllFrom);
    this._tablePopupDeleteAll.removeEventListener("command", this.onRemoveAll);
    this._tablePopupDeleteAllSessionCookies.removeEventListener("command",
      this.onRemoveAllSessionCookies);
  }

  setupToolbar() {
    this.searchBox = this._panelDoc.getElementById("storage-searchbox");
    this.searchBox.addEventListener("command", this.filterItems);

    // Setup the sidebar toggle button.
    this.sidebarToggleBtn = this._panelDoc.querySelector(".sidebar-toggle");
    this.updateSidebarToggleButton();

    this.sidebarToggleBtn.addEventListener("click", this.onPaneToggleButtonClicked);
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

    this.sidebarToggleBtn.setAttribute("tooltiptext", title);
  }

  /**
   * Hide the object viewer sidebar
   */
  hideSidebar() {
    this.sidebar.hidden = true;
    this.updateSidebarToggleButton();
  }

  getCurrentFront() {
    const type = this.table.datatype;

    return this.storageTypes[type];
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
   * @param {object} response
   *        An object containing which storage types were cleared
   */
  onCleared(response) {
    function* enumPaths() {
      for (const type in response) {
        if (Array.isArray(response[type])) {
          // Handle the legacy response with array of hosts
          for (const host of response[type]) {
            yield [type, host];
          }
        } else {
          // Handle the new format that supports clearing sub-stores in a host
          for (const host in response[type]) {
            const paths = response[type][host];

            if (!paths.length) {
              yield [type, host];
            } else {
              for (let path of paths) {
                try {
                  path = JSON.parse(path);
                  yield [type, host, ...path];
                } catch (ex) {
                  // ignore
                }
              }
            }
          }
        }
      }
    }

    for (const path of enumPaths()) {
      // Find if the path is selected (there is max one) and clear it
      if (this.tree.isSelected(path)) {
        this.table.clear();
        this.hideSidebar();
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
  async onEdit({ changed, added, deleted }) {
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
        this.tree.add([type, {id: host, label: label, type: "url"}]);
        for (let name of added[type][host]) {
          try {
            name = JSON.parse(name);
            if (name.length == 3) {
              name.splice(2, 1);
            }
            this.tree.add([type, host, ...name]);
            if (!this.tree.selectedItem) {
              this.tree.selectedItem = [type, host, name[0], name[1]];
              await this.fetchStorageObjects(type, host, [JSON.stringify(name)],
                                              REASON.NEW_ROW);
            }
          } catch (ex) {
            // Do nothing
          }
        }

        if (this.tree.isSelected([type, host])) {
          await this.fetchStorageObjects(type, host, added[type][host],
                                          REASON.NEW_ROW);
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
              // trying to parse names in case of indexedDB or cache
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
    if (!changed[type] || !changed[type][host] ||
        changed[type][host].length == 0) {
      return;
    }
    try {
      const toUpdate = [];
      for (const name of changed[type][host]) {
        const names = JSON.parse(name);
        if (names[0] == db && names[1] == objectStore && names[2]) {
          toUpdate.push(name);
        }
      }
      await this.fetchStorageObjects(type, host, toUpdate, REASON.UPDATE);
    } catch (ex) {
      await this.fetchStorageObjects(type, host, changed[type][host], REASON.UPDATE);
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
    const fetchOpts = reason === REASON.NEXT_50_ITEMS ? {offset: this.itemOffset}
                                                    : {};
    const storageType = this.storageTypes[type];

    this.sidebarToggledOpen = null;

    if (reason !== REASON.NEXT_50_ITEMS &&
        reason !== REASON.UPDATE &&
        reason !== REASON.NEW_ROW &&
        reason !== REASON.POPULATE) {
      throw new Error("Invalid reason specified");
    }

    try {
      if (reason === REASON.POPULATE) {
        let subType = null;
        // The indexedDB type could have sub-type data to fetch.
        // If having names specified, then it means
        // we are fetching details of specific database or of object store.
        if (type === "indexedDB" && names) {
          const [ dbName, objectStoreName ] = JSON.parse(names[0]);
          if (dbName) {
            subType = "database";
          }
          if (objectStoreName) {
            subType = "object store";
          }
        }

        this.actorSupportsAddItem = await this._target.actorHasMethod(type, "addItem");
        this.actorSupportsRemoveItem =
          await this._target.actorHasMethod(type, "removeItem");
        this.actorSupportsRemoveAll =
          await this._target.actorHasMethod(type, "removeAll");
        this.actorSupportsRemoveAllSessionCookies =
          await this._target.actorHasMethod(type, "removeAllSessionCookies");

        await this.resetColumns(type, host, subType);
      }

      const {data} = await storageType.getStoreObjects(host, names, fetchOpts);
      if (data.length) {
        await this.populateTable(data, reason);
      }
      this.updateToolbar();
      this.emit("store-objects-updated");
    } catch (ex) {
      console.error(ex);
    }
  }

  /**
   * Updates the toolbar hiding and showing buttons as appropriate.
   */
  updateToolbar() {
    const item = this.tree.selectedItem;
    const howManyNodesIn = item ? item.length : 0;

    // The first node is just a title e.g. "Cookies" so we need to be at least
    // 2 nodes in to show the add button.
    const canAdd = this.actorSupportsAddItem && howManyNodesIn > 1;

    if (canAdd) {
      this._addButton.hidden = false;
      this._addButton.setAttribute("tooltiptext",
        L10N.getFormatStr("storage.popupMenu.addItemLabel"));
    } else {
      this._addButton.hidden = true;
      this._addButton.removeAttribute("tooltiptext");
    }
  }

  /**
   * Populates the storage tree which displays the list of storages present for
   * the page.
   *
   * @param {object} storageTypes
   *        List of storages and their corresponding hosts returned by the
   *        StorageFront.listStores call.
   */
  populateStorageTree(storageTypes) {
    this.storageTypes = {};
    for (const type in storageTypes) {
      // Ignore `from` field, which is just a protocol.js implementation
      // artifact.
      if (type === "from") {
        continue;
      }
      let typeLabel = type;
      try {
        typeLabel = L10N.getStr("tree.labels." + type);
      } catch (e) {
        console.error("Unable to localize tree label type:" + type);
      }
      this.tree.add([{id: type, label: typeLabel, type: "store"}]);
      if (!storageTypes[type].hosts) {
        continue;
      }
      this.storageTypes[type] = storageTypes[type];
      for (const host in storageTypes[type].hosts) {
        const label = this.getReadableLabelFromHostname(host);
        this.tree.add([type, {id: host, label: label, type: "url"}]);
        for (const name of storageTypes[type].hosts[host]) {
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
    }
  }

  /**
   * Populates the selected entry from the table in the sidebar for a more
   * detailed view.
   */
  async updateObjectSidebar() {
    const item = this.table.selectedRow;
    let value;

    // Get the string value (async action) and the update the UI synchronously.
    if (item && item.name && item.valueActor) {
      value = await item.valueActor.string();
    }

    // Bail if the selectedRow is no longer selected, the item doesn't exist or the state
    // changed in another way during the above yield.
    if (this.table.items.size === 0 ||
        !item ||
        !this.table.selectedRow ||
        item.uniqueKey !== this.table.selectedRow.uniqueKey) {
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
      const itemVar = mainScope.addItem(item.name + "", {}, {relaxed: true});

      // The main area where the value will be displayed
      itemVar.setGrip(value);

      // May be the item value is a json or a key value pair itself
      this.parseItemValue(item.name, value);

      // By default the item name and value are shown. If this is the only
      // information available, then nothing else is to be displayed.
      const itemProps = Object.keys(item);
      if (itemProps.length > 3) {
        // Display any other information other than the item name and value
        // which may be available.
        const rawObject = Object.create(null);
        const otherProps = itemProps.filter(
          e => !["name", "value", "valueActor"].includes(e));
        for (const prop of otherProps) {
          const column = this.table.columns.get(prop);
          if (column && column.private) {
            continue;
          }

          const cookieProp = COOKIE_KEY_MAP[prop] || prop;
          // The pseduo property of HostOnly refers to converse of isDomain property
          rawObject[cookieProp] = (prop === "isDomain") ? !item[prop] : item[prop];
        }
        itemVar.populate(rawObject, {sorted: true});
        itemVar.twisty = true;
        itemVar.expanded = true;
      }
    } else {
      // Case when displaying IndexedDB db/object store properties.
      for (const key in item) {
        const column = this.table.columns.get(key);
        if (column && column.private) {
          continue;
        }

        mainScope.addItem(key, {}, true).setGrip(item[key]);
        this.parseItemValue(key, item[key]);
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
   * Tries to parse a string value into either a json or a key-value separated
   * object and populates the sidebar with the parsed value. The value can also
   * be a key separated array.
   *
   * @param {string} name
   *        The key corresponding to the `value` string in the object
   * @param {string} originalValue
   *        The string to be parsed into an object
   */
  parseItemValue(name, originalValue) {
    // Find if value is URLEncoded ie
    let decodedValue = "";
    try {
      decodedValue = decodeURIComponent(originalValue);
    } catch (e) {
      // Unable to decode, nothing to do
    }
    const value = (decodedValue && decodedValue !== originalValue)
      ? decodedValue : originalValue;

    let json = null;
    try {
      json = JSOL.parse(value);
    } catch (ex) {
      json = null;
    }

    if (!json && value) {
      json = this._extractKeyValPairs(value);
    }

    // return if json is null, or same as value, or just a string.
    if (!json || json == value || typeof json == "string") {
      return;
    }

    // One special case is a url which gets separated as key value pair on :
    if ((json.length == 2 || Object.keys(json).length == 1) &&
        ((json[0] || Object.keys(json)[0]) + "").match(/^(http|file|ftp)/)) {
      return;
    }

    const jsonObject = Object.create(null);
    const view = this.view;
    jsonObject[name] = json;
    const valueScope = view.getScopeAtIndex(1) ||
                      view.addScope(L10N.getStr("storage.parsedValue.label"));
    valueScope.expanded = true;
    const jsonVar = valueScope.addItem("", Object.create(null), {relaxed: true});
    jsonVar.expanded = true;
    jsonVar.twisty = true;
    jsonVar.populate(jsonObject, {expanded: true});
  }

  /**
   * Tries to parse a string into an object on the basis of key-value pairs,
   * separated by various separators. If failed, tries to parse for single
   * separator separated values to form an array.
   *
   * @param {string} value
   *        The string to be parsed into an object or array
   */
  _extractKeyValPairs(value) {
    const makeObject = (keySep, pairSep) => {
      const object = {};
      for (const pair of value.split(pairSep)) {
        const [key, val] = pair.split(keySep);
        object[key] = val;
      }
      return object;
    };

    // Possible separators.
    const separators = ["=", ":", "~", "#", "&", "\\*", ",", "\\."];
    // Testing for object
    for (let i = 0; i < separators.length; i++) {
      const kv = separators[i];
      for (let j = 0; j < separators.length; j++) {
        if (i == j) {
          continue;
        }
        const p = separators[j];
        const word = `[^${kv}${p}]*`;
        const keyValue = `${word}${kv}${word}`;
        const keyValueList = `${keyValue}(${p}${keyValue})*`;
        const regex = new RegExp(`^${keyValueList}$`);
        if (value.match && value.match(regex) && value.includes(kv) &&
            (value.includes(p) || value.split(kv).length == 2)) {
          return makeObject(kv, p);
        }
      }
    }
    // Testing for array
    for (const p of separators) {
      const word = `[^${p}]*`;
      const wordList = `(${word}${p})+${word}`;
      const regex = new RegExp(`^${wordList}$`);
      if (value.match && value.match(regex)) {
        return value.split(p.replace(/\\*/g, ""));
      }
    }
    return null;
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
      return;
    }
    if (item.length > 2) {
      names = [JSON.stringify(item.slice(2))];
    }
    await this.fetchStorageObjects(type, host, names, REASON.POPULATE);
    this.itemOffset = 0;
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

      columns[f.name] = f.name;
      let columnName;
      try {
        // Path key names for l10n in the case of a string change.
        const name = f.name === "keyPath" ? "keyPath2" : f.name;

        columnName = L10N.getStr("table.headers." + type + "." + name);
      } catch (e) {
        columnName = COOKIE_KEY_MAP[f.name];
      }

      if (!columnName) {
        console.error("Unable to localize table header type:" + type + " key:" + f.name);
      } else {
        columns[f.name] = columnName;
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
    if (event.keyCode == KeyCodes.DOM_VK_ESCAPE && !this.sidebar.hidden) {
      // Stop Propagation to prevent opening up of split console
      this.hideSidebar();
      this.sidebarToggledOpen = false;
      event.stopPropagation();
      event.preventDefault();
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
    const type = selectedItem[0];

    // IndexedDB only supports removing items from object stores (level 4 of the tree)
    if ((!this.actorSupportsAddItem && !this.actorSupportsRemoveItem &&
          type !== "cookies") ||
        (type === "indexedDB" && selectedItem.length !== 4)) {
      event.preventDefault();
      return;
    }

    const rowId = this.table.contextMenuRowId;
    const data = this.table.items.get(rowId);

    if (this.actorSupportsRemoveItem) {
      const name = data[this.table.uniqueId];
      const separatorRegex = new RegExp(SEPARATOR_GUID, "g");
      const label = addEllipsis((name + "").replace(separatorRegex, "-"));

      this._tablePopupDelete.hidden = false;
      this._tablePopupDelete.setAttribute("label",
        L10N.getFormatStr("storage.popupMenu.deleteLabel", label));
    } else {
      this._tablePopupDelete.hidden = true;
    }

    if (this.actorSupportsAddItem) {
      this._tablePopupAddItem.hidden = false;
      this._tablePopupAddItem.setAttribute("label",
        L10N.getFormatStr("storage.popupMenu.addItemLabel"));
    } else {
      this._tablePopupAddItem.hidden = true;
    }

    let showDeleteAllSessionCookies = false;
    if (this.actorSupportsRemoveAllSessionCookies) {
      if (type === "cookies" && selectedItem.length === 2) {
        showDeleteAllSessionCookies = true;
      }
    }

    this._tablePopupDeleteAllSessionCookies.hidden = !showDeleteAllSessionCookies;

    if (type === "cookies") {
      const host = addEllipsis(data.host);

      this._tablePopupDeleteAllFrom.hidden = false;
      this._tablePopupDeleteAllFrom.setAttribute("label",
        L10N.getFormatStr("storage.popupMenu.deleteAllFromLabel", host));
    } else {
      this._tablePopupDeleteAllFrom.hidden = true;
    }
  }

  onTreePopupShowing(event) {
    let showMenu = false;
    const selectedItem = this.tree.selectedItem;

    if (selectedItem) {
      const type = selectedItem[0];

      // The delete all (aka clear) action is displayed for IndexedDB object stores
      // (level 4 of tree), for Cache objects (level 3) and for the whole host (level 2)
      // for other storage types (cookies, localStorage, ...).
      let showDeleteAll = false;
      if (this.actorSupportsRemoveAll) {
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
      if (this.actorSupportsRemoveAllSessionCookies) {
        if (type === "cookies" && selectedItem.length === 2) {
          showDeleteAllSessionCookies = true;
        }
      }

      this._treePopupDeleteAllSessionCookies.hidden = !showDeleteAllSessionCookies;

      // The delete action is displayed for:
      // - IndexedDB databases (level 3 of the tree)
      // - Cache objects (level 3 of the tree)
      const showDelete = (type == "indexedDB" || type == "Cache") &&
                        selectedItem.length == 3;
      this._treePopupDelete.hidden = !showDelete;
      if (showDelete) {
        const itemName = addEllipsis(selectedItem[selectedItem.length - 1]);
        this._treePopupDelete.setAttribute("label",
          L10N.getFormatStr("storage.popupMenu.deleteLabel", itemName));
      }

      showMenu = showDeleteAll || showDelete;
    }

    if (!showMenu) {
      event.preventDefault();
    }
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
   * Handles removing an item from the storage
   */
  onRemoveItem() {
    const [, host, ...path] = this.tree.selectedItem;
    const front = this.getCurrentFront();
    const rowId = this.table.contextMenuRowId;
    const data = this.table.items.get(rowId);
    let name = data[this.table.uniqueId];
    if (path.length > 0) {
      name = JSON.stringify([...path, name]);
    }
    front.removeItem(host, name);
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

    front.removeDatabase(host, dbName).then(result => {
      if (result.blocked) {
        const notificationBox = this._toolbox.getNotificationBox();
        notificationBox.appendNotification(
          L10N.getFormatStr("storage.idb.deleteBlocked", dbName),
          "storage-idb-delete-blocked",
          null,
          notificationBox.PRIORITY_WARNING_LOW);
      }
    }).catch(error => {
      const notificationBox = this._toolbox.getNotificationBox();
      notificationBox.appendNotification(
        L10N.getFormatStr("storage.idb.deleteError", dbName),
        "storage-idb-delete-error",
        null,
        notificationBox.PRIORITY_CRITICAL_LOW);
    });
  }

  removeCache(host, cacheName) {
    const front = this.getCurrentFront();

    front.removeItem(host, JSON.stringify([ cacheName ]));
  }
}

exports.StorageUI = StorageUI;

// Helper Functions

function createGUID() {
  return "{cccccccc-cccc-4ccc-yccc-cccccccccccc}".replace(/[cy]/g, c => {
    const r = Math.random() * 16 | 0;
    const v = c == "c" ? r : (r & 0x3 | 0x8);
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
