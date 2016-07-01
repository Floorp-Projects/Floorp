/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Task} = require("devtools/shared/task");
const EventEmitter = require("devtools/shared/event-emitter");
const {LocalizationHelper} = require("devtools/client/shared/l10n");
const {KeyShortcuts} = require("devtools/client/shared/key-shortcuts");
const JSOL = require("devtools/client/shared/vendor/jsol");

loader.lazyRequireGetter(this, "TreeWidget",
                         "devtools/client/shared/widgets/TreeWidget", true);
loader.lazyRequireGetter(this, "TableWidget",
                         "devtools/client/shared/widgets/TableWidget", true);
loader.lazyRequireGetter(this, "ViewHelpers",
                         "devtools/client/shared/widgets/view-helpers");
loader.lazyImporter(this, "VariablesView",
  "resource://devtools/client/shared/widgets/VariablesView.jsm");

/**
 * Localization convenience methods.
 */
const STORAGE_STRINGS = "chrome://devtools/locale/storage.properties";
const L10N = new LocalizationHelper(STORAGE_STRINGS);

const GENERIC_VARIABLES_VIEW_SETTINGS = {
  lazyEmpty: true,
   // ms
  lazyEmptyDelay: 10,
  searchEnabled: true,
  searchPlaceholder: L10N.getStr("storage.search.placeholder"),
  preventDescriptorModifiers: true
};

// Columns which are hidden by default in the storage table
const HIDDEN_COLUMNS = [
  "creationTime",
  "isDomain",
  "isSecure"
];

const REASON = {
  NEW_ROW: "new-row",
  NEXT_50_ITEMS: "next-50-items",
  POPULATE: "populate",
  UPDATE: "update"
};

// Maximum length of item name to show in context menu label - will be
// trimmed with ellipsis if it's longer.
const ITEM_NAME_MAX_LENGTH = 32;

function addEllipsis(name) {
  if (name.length > ITEM_NAME_MAX_LENGTH) {
    return name.substr(0, ITEM_NAME_MAX_LENGTH) + L10N.ellipsis;
  }

  return name;
}

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
function StorageUI(front, target, panelWin, toolbox) {
  EventEmitter.decorate(this);

  this._target = target;
  this._window = panelWin;
  this._panelDoc = panelWin.document;
  this._toolbox = toolbox;
  this.front = front;

  let treeNode = this._panelDoc.getElementById("storage-tree");
  this.tree = new TreeWidget(treeNode, {
    defaultType: "dir",
    contextMenuId: "storage-tree-popup"
  });
  this.onHostSelect = this.onHostSelect.bind(this);
  this.tree.on("select", this.onHostSelect);

  let tableNode = this._panelDoc.getElementById("storage-table");
  this.table = new TableWidget(tableNode, {
    emptyText: L10N.getStr("table.emptyText"),
    highlightUpdated: true,
    cellContextMenuId: "storage-table-popup"
  });

  this.displayObjectSidebar = this.displayObjectSidebar.bind(this);
  this.table.on(TableWidget.EVENTS.ROW_SELECTED, this.displayObjectSidebar);

  this.handleScrollEnd = this.handleScrollEnd.bind(this);
  this.table.on(TableWidget.EVENTS.SCROLL_END, this.handleScrollEnd);

  this.editItem = this.editItem.bind(this);
  this.table.on(TableWidget.EVENTS.CELL_EDIT, this.editItem);

  this.sidebar = this._panelDoc.getElementById("storage-sidebar");
  this.sidebar.setAttribute("width", "300");
  this.view = new VariablesView(this.sidebar.firstChild,
                                GENERIC_VARIABLES_VIEW_SETTINGS);

  this.searchBox = this._panelDoc.getElementById("storage-searchbox");
  this.filterItems = this.filterItems.bind(this);
  this.searchBox.addEventListener("command", this.filterItems);

  let shortcuts = new KeyShortcuts({
    window: this._panelDoc.defaultView,
  });
  let key = L10N.getStr("storage.filter.key");
  shortcuts.on(key, (name, event) => {
    event.preventDefault();
    this.searchBox.focus();
  });

  this.front.listStores().then(storageTypes => {
    this.populateStorageTree(storageTypes);
  }).then(null, console.error);

  this.onUpdate = this.onUpdate.bind(this);
  this.front.on("stores-update", this.onUpdate);
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

  this.onRemoveItem = this.onRemoveItem.bind(this);
  this.onRemoveAllFrom = this.onRemoveAllFrom.bind(this);
  this.onRemoveAll = this.onRemoveAll.bind(this);
  this.onRemoveDatabase = this.onRemoveDatabase.bind(this);

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

  this._treePopupDeleteAll = this._panelDoc.getElementById(
    "storage-tree-popup-delete-all");
  this._treePopupDeleteAll.addEventListener("command", this.onRemoveAll);

  this._treePopupDeleteDatabase = this._panelDoc.getElementById(
    "storage-tree-popup-delete-database");
  this._treePopupDeleteDatabase.addEventListener("command",
    this.onRemoveDatabase);
}

exports.StorageUI = StorageUI;

StorageUI.prototype = {

  storageTypes: null,
  shouldLoadMoreItems: true,

  set animationsEnabled(value) {
    this._panelDoc.documentElement.classList.toggle("no-animate", !value);
  },

  destroy: function () {
    this.table.off(TableWidget.EVENTS.ROW_SELECTED, this.displayObjectSidebar);
    this.table.off(TableWidget.EVENTS.SCROLL_END, this.handleScrollEnd);
    this.table.off(TableWidget.EVENTS.CELL_EDIT, this.editItem);
    this.table.destroy();

    this.front.off("stores-update", this.onUpdate);
    this.front.off("stores-cleared", this.onCleared);
    this._panelDoc.removeEventListener("keypress", this.handleKeypress);
    this.searchBox.removeEventListener("input", this.filterItems);
    this.searchBox = null;

    this._treePopup.removeEventListener("popupshowing",
      this.onTreePopupShowing);
    this._treePopupDeleteAll.removeEventListener("command",
      this.onRemoveAll);
    this._treePopupDeleteDatabase.removeEventListener("command",
      this.onRemoveDatabase);

    this._tablePopup.removeEventListener("popupshowing",
      this.onTablePopupShowing);
    this._tablePopupDelete.removeEventListener("command",
      this.onRemoveItem);
    this._tablePopupDeleteAllFrom.removeEventListener("command",
      this.onRemoveAllFrom);
    this._tablePopupDeleteAll.removeEventListener("command",
      this.onRemoveAll);
  },

  /**
   * Empties and hides the object viewer sidebar
   */
  hideSidebar: function () {
    this.view.empty();
    this.sidebar.hidden = true;
    this.table.clearSelection();
  },

  getCurrentActor: function () {
    let type = this.table.datatype;

    return this.storageTypes[type];
  },

  /**
   *  Make column fields editable
   *
   *  @param {Array} editableFields
   *         An array of keys of columns to be made editable
   */
  makeFieldsEditable: function* (editableFields) {
    if (editableFields && editableFields.length > 0) {
      this.table.makeFieldsEditable(editableFields);
    } else if (this.table._editableFieldsEngine) {
      this.table._editableFieldsEngine.destroy();
    }
  },

  editItem: function (eventType, data) {
    let actor = this.getCurrentActor();

    actor.editItem(data);
  },

  /**
   * Removes the given item from the storage table. Reselects the next item in
   * the table and repopulates the sidebar with that item's data if the item
   * being removed was selected.
   */
  removeItemFromTable: function (name) {
    if (this.table.isSelected(name)) {
      if (this.table.selectedIndex == 0) {
        this.table.selectNextRow();
      } else {
        this.table.selectPreviousRow();
      }
      this.table.remove(name);
      this.displayObjectSidebar();
    } else {
      this.table.remove(name);
    }
  },

  /**
   * Event handler for "stores-cleared" event coming from the storage actor.
   *
   * @param {object} response
   *        An object containing which storage types were cleared
   */
  onCleared: function (response) {
    function* enumPaths() {
      for (let type in response) {
        if (Array.isArray(response[type])) {
          // Handle the legacy response with array of hosts
          for (let host of response[type]) {
            yield [type, host];
          }
        } else {
          // Handle the new format that supports clearing sub-stores in a host
          for (let host in response[type]) {
            let paths = response[type][host];

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

    for (let path of enumPaths()) {
      // Find if the path is selected (there is max one) and clear it
      if (this.tree.isSelected(path)) {
        this.table.clear();
        this.hideSidebar();
        this.emit("store-objects-cleared");
        break;
      }
    }
  },

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
  onUpdate: function ({ changed, added, deleted }) {
    if (deleted) {
      this.handleDeletedItems(deleted);
    }

    if (added) {
      this.handleAddedItems(added);
    }

    if (changed) {
      this.handleChangedItems(changed);
    }

    if (added || deleted || changed) {
      this.emit("store-objects-updated");
    }
  },

  /**
   * Handle added items received by onUpdate
   *
   * @param {object} See onUpdate docs
   */
  handleAddedItems: function (added) {
    for (let type in added) {
      for (let host in added[type]) {
        this.tree.add([type, {id: host, type: "url"}]);
        for (let name of added[type][host]) {
          try {
            name = JSON.parse(name);
            if (name.length == 3) {
              name.splice(2, 1);
            }
            this.tree.add([type, host, ...name]);
            if (!this.tree.selectedItem) {
              this.tree.selectedItem = [type, host, name[0], name[1]];
              this.fetchStorageObjects(type, host, [JSON.stringify(name)],
                                       REASON.NEW_ROW);
            }
          } catch (ex) {
            // Do nothing
          }
        }

        if (this.tree.isSelected([type, host])) {
          this.fetchStorageObjects(type, host, added[type][host],
                                   REASON.NEW_ROW);
        }
      }
    }
  },

  /**
   * Handle deleted items received by onUpdate
   *
   * @param {object} See onUpdate docs
   */
  handleDeletedItems: function (deleted) {
    for (let type in deleted) {
      for (let host in deleted[type]) {
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
          for (let name of deleted[type][host]) {
            try {
              // trying to parse names in case of indexedDB or cache
              let names = JSON.parse(name);
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
                let tableItemName = names.pop();
                if (this.tree.isSelected([type, host, ...names])) {
                  this.removeItemFromTable(tableItemName);
                }
              }
            } catch (ex) {
              if (this.tree.isSelected([type, host])) {
                this.removeItemFromTable(name);
              }
            }
          }
        }
      }
    }
  },

  /**
   * Handle changed items received by onUpdate
   *
   * @param {object} See onUpdate docs
   */
  handleChangedItems: function (changed) {
    let [type, host, db, objectStore] = this.tree.selectedItem;
    if (!changed[type] || !changed[type][host] ||
        changed[type][host].length == 0) {
      return;
    }
    try {
      let toUpdate = [];
      for (let name of changed[type][host]) {
        let names = JSON.parse(name);
        if (names[0] == db && names[1] == objectStore && names[2]) {
          toUpdate.push(name);
        }
      }
      this.fetchStorageObjects(type, host, toUpdate, REASON.UPDATE);
    } catch (ex) {
      this.fetchStorageObjects(type, host, changed[type][host], REASON.UPDATE);
    }
  },

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
  fetchStorageObjects: Task.async(function* (type, host, names, reason) {
    let fetchOpts = reason === REASON.NEXT_50_ITEMS ? {offset: this.itemOffset}
                                                    : {};
    let storageType = this.storageTypes[type];

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
        if (type == "indexedDB" && names) {
          let [ dbName, objectStoreName ] = JSON.parse(names[0]);
          if (dbName) {
            subType = "database";
          }
          if (objectStoreName) {
            subType = "object store";
          }
        }
        yield this.resetColumns(type, host, subType);
      }

      let {data} = yield storageType.getStoreObjects(host, names, fetchOpts);
      if (data.length) {
        this.populateTable(data, reason);
      }
      this.emit("store-objects-updated");
    } catch (ex) {
      console.error(ex);
    }
  }),

  /**
   * Populates the storage tree which displays the list of storages present for
   * the page.
   *
   * @param {object} storageTypes
   *        List of storages and their corresponding hosts returned by the
   *        StorageFront.listStores call.
   */
  populateStorageTree: function (storageTypes) {
    this.storageTypes = {};
    for (let type in storageTypes) {
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
      for (let host in storageTypes[type].hosts) {
        this.tree.add([type, {id: host, type: "url"}]);
        for (let name of storageTypes[type].hosts[host]) {
          try {
            let names = JSON.parse(name);
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
  },

  /**
   * Populates the selected entry from teh table in the sidebar for a more
   * detailed view.
   */
  displayObjectSidebar: Task.async(function* () {
    let item = this.table.selectedRow;
    if (!item) {
      // Make sure that sidebar is hidden and return
      this.sidebar.hidden = true;
      return;
    }

    // Get the string value (async action) and the update the UI synchronously.
    let value;
    if (item.name && item.valueActor) {
      value = yield item.valueActor.string();
    }

    // Start updating the UI. Everything is sync beyond this point.
    this.sidebar.hidden = false;
    this.view.empty();
    let mainScope = this.view.addScope(L10N.getStr("storage.data.label"));
    mainScope.expanded = true;

    if (value) {
      let itemVar = mainScope.addItem(item.name + "", {}, {relaxed: true});

      // The main area where the value will be displayed
      itemVar.setGrip(value);

      // May be the item value is a json or a key value pair itself
      this.parseItemValue(item.name, value);

      // By default the item name and value are shown. If this is the only
      // information available, then nothing else is to be displayed.
      let itemProps = Object.keys(item);
      if (itemProps.length > 3) {
        // Display any other information other than the item name and value
        // which may be available.
        let rawObject = Object.create(null);
        let otherProps = itemProps.filter(
          e => !["name", "value", "valueActor"].includes(e));
        for (let prop of otherProps) {
          rawObject[prop] = item[prop];
        }
        itemVar.populate(rawObject, {sorted: true});
        itemVar.twisty = true;
        itemVar.expanded = true;
      }
    } else {
      // Case when displaying IndexedDB db/object store properties.
      for (let key in item) {
        mainScope.addItem(key, {}, true).setGrip(item[key]);
        this.parseItemValue(key, item[key]);
      }
    }

    this.emit("sidebar-updated");
  }),

  /**
   * Tries to parse a string value into either a json or a key-value separated
   * object and populates the sidebar with the parsed value. The value can also
   * be a key separated array.
   *
   * @param {string} name
   *        The key corresponding to the `value` string in the object
   * @param {string} value
   *        The string to be parsed into an object
   */
  parseItemValue: function (name, value) {
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

    let jsonObject = Object.create(null);
    let view = this.view;
    jsonObject[name] = json;
    let valueScope = view.getScopeAtIndex(1) ||
                     view.addScope(L10N.getStr("storage.parsedValue.label"));
    valueScope.expanded = true;
    let jsonVar = valueScope.addItem("", Object.create(null), {relaxed: true});
    jsonVar.expanded = true;
    jsonVar.twisty = true;
    jsonVar.populate(jsonObject, {expanded: true});
  },

  /**
   * Tries to parse a string into an object on the basis of key-value pairs,
   * separated by various separators. If failed, tries to parse for single
   * separator separated values to form an array.
   *
   * @param {string} value
   *        The string to be parsed into an object or array
   */
  _extractKeyValPairs: function (value) {
    let makeObject = (keySep, pairSep) => {
      let object = {};
      for (let pair of value.split(pairSep)) {
        let [key, val] = pair.split(keySep);
        object[key] = val;
      }
      return object;
    };

    // Possible separators.
    const separators = ["=", ":", "~", "#", "&", "\\*", ",", "\\."];
    // Testing for object
    for (let i = 0; i < separators.length; i++) {
      let kv = separators[i];
      for (let j = 0; j < separators.length; j++) {
        if (i == j) {
          continue;
        }
        let p = separators[j];
        let word = `[^${kv}${p}]*`;
        let keyValue = `${word}${kv}${word}`;
        let keyValueList = `${keyValue}(${p}${keyValue})*`;
        let regex = new RegExp(`^${keyValueList}$`);
        if (value.match && value.match(regex) && value.includes(kv) &&
            (value.includes(p) || value.split(kv).length == 2)) {
          return makeObject(kv, p);
        }
      }
    }
    // Testing for array
    for (let p of separators) {
      let word = `[^${p}]*`;
      let wordList = `(${word}${p})+${word}`;
      let regex = new RegExp(`^${wordList}$`);
      if (value.match && value.match(regex)) {
        return value.split(p.replace(/\\*/g, ""));
      }
    }
    return null;
  },

  /**
   * Select handler for the storage tree. Fetches details of the selected item
   * from the storage details and populates the storage tree.
   *
   * @param {string} event
   *        The name of the event fired
   * @param {array} item
   *        An array of ids which represent the location of the selected item in
   *        the storage tree
   */
  onHostSelect: function (event, item) {
    this.table.clear();
    this.hideSidebar();
    this.searchBox.value = "";

    let [type, host] = item;
    let names = null;
    if (!host) {
      return;
    }
    if (item.length > 2) {
      names = [JSON.stringify(item.slice(2))];
    }
    this.fetchStorageObjects(type, host, names, REASON.POPULATE);
    this.itemOffset = 0;
  },

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
  resetColumns: function* (type, host, subtype) {
    this.table.host = host;
    this.table.datatype = type;

    let uniqueKey = null;
    let columns = {};
    let editableFields = [];
    let fields = yield this.getCurrentActor().getFields(subtype);

    fields.forEach(f => {
      if (!uniqueKey) {
        this.table.uniqueId = uniqueKey = f.name;
      }

      if (f.editable) {
        editableFields.push(f.name);
      }

      columns[f.name] = f.name;
      try {
        columns[f.name] = L10N.getStr("table.headers." + type + "." + f.name);
      } catch (e) {
        console.error("Unable to localize table header type:" + type +
                      " key:" + f.name);
      }
    });

    this.table.setColumns(columns, null, HIDDEN_COLUMNS);
    this.hideSidebar();

    yield this.makeFieldsEditable(editableFields);
  },

  /**
   * Populates or updates the rows in the storage table.
   *
   * @param {array[object]} data
   *        Array of objects to be populated in the storage table
   * @param {Constant} reason
   *        See REASON constant at top of file.
   */
  populateTable: function (data, reason) {
    for (let item of data) {
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
          // Update without flashing the row.
          this.table.push(item, true);
          break;
        case REASON.NEW_ROW:
        case REASON.NEXT_50_ITEMS:
          // Update and flash the row.
          this.table.push(item, false);
          break;
        case REASON.UPDATE:
          this.table.update(item);
          if (item == this.table.selectedRow && !this.sidebar.hidden) {
            this.displayObjectSidebar();
          }
          break;
      }

      this.shouldLoadMoreItems = true;
    }
  },

  /**
   * Handles keypress event on the body table to close the sidebar when open
   *
   * @param {DOMEvent} event
   *        The event passed by the keypress event.
   */
  handleKeypress: function (event) {
    if (event.keyCode == event.DOM_VK_ESCAPE && !this.sidebar.hidden) {
      // Stop Propagation to prevent opening up of split console
      this.hideSidebar();
      event.stopPropagation();
      event.preventDefault();
    }
  },

  /**
   * Handles filtering the table
   */
  filterItems() {
    let value = this.searchBox.value;
    this.table.filterItems(value, ["valueActor"]);
    this._panelDoc.documentElement.classList.toggle("filtering", !!value);
  },

  /**
   * Handles endless scrolling for the table
   */
  handleScrollEnd: function () {
    if (!this.shouldLoadMoreItems) {
      return;
    }
    this.shouldLoadMoreItems = false;
    this.itemOffset += 50;

    let item = this.tree.selectedItem;
    let [type, host] = item;
    let names = null;
    if (item.length > 2) {
      names = [JSON.stringify(item.slice(2))];
    }
    this.fetchStorageObjects(type, host, names, REASON.NEXT_50_ITEMS);
  },

  /**
   * Fires before a cell context menu with the "Delete" action is shown.
   * If the currently selected storage object doesn't support removing items, prevent
   * showing the menu.
   */
  onTablePopupShowing: function (event) {
    let selectedItem = this.tree.selectedItem;
    let type = selectedItem[0];
    let actor = this.getCurrentActor();

    // IndexedDB only supports removing items from object stores (level 4 of the tree)
    if (!actor.removeItem || (type === "indexedDB" && selectedItem.length !== 4)) {
      event.preventDefault();
      return;
    }

    let rowId = this.table.contextMenuRowId;
    let data = this.table.items.get(rowId);
    let name = addEllipsis(data[this.table.uniqueId]);

    this._tablePopupDelete.setAttribute("label",
      L10N.getFormatStr("storage.popupMenu.deleteLabel", name));

    if (type === "cookies") {
      let host = addEllipsis(data.host);

      this._tablePopupDeleteAllFrom.hidden = false;
      this._tablePopupDeleteAllFrom.setAttribute("label",
        L10N.getFormatStr("storage.popupMenu.deleteAllFromLabel", host));
    } else {
      this._tablePopupDeleteAllFrom.hidden = true;
    }
  },

  onTreePopupShowing: function (event) {
    let showMenu = false;
    let selectedItem = this.tree.selectedItem;

    if (selectedItem) {
      let type = selectedItem[0];
      let actor = this.storageTypes[type];

      // The delete all (aka clear) action is displayed for IndexedDB object stores
      // (level 4 of tree) and for the whole host (level 2 of tree) of other storage
      // types (cookies, localStorage, ...).
      let showDeleteAll = actor.removeAll &&
        (selectedItem.length === (type === "indexedDB" ? 4 : 2));

      this._treePopupDeleteAll.hidden = !showDeleteAll;

      // The action to delete database is available for IndexedDB databases, i.e.,
      // at level 3 of the tree.
      let showDeleteDb = actor.removeDatabase && selectedItem.length === 3;
      this._treePopupDeleteDatabase.hidden = !showDeleteDb;
      if (showDeleteDb) {
        let dbName = addEllipsis(selectedItem[2]);
        this._treePopupDeleteDatabase.setAttribute("label",
          L10N.getFormatStr("storage.popupMenu.deleteLabel", dbName));
      }

      showMenu = showDeleteAll || showDeleteDb;
    }

    if (!showMenu) {
      event.preventDefault();
    }
  },

  /**
   * Handles removing an item from the storage
   */
  onRemoveItem: function () {
    let [, host, ...path] = this.tree.selectedItem;
    let actor = this.getCurrentActor();
    let rowId = this.table.contextMenuRowId;
    let data = this.table.items.get(rowId);
    let name = data[this.table.uniqueId];
    if (path.length > 0) {
      name = JSON.stringify([...path, name]);
    }
    actor.removeItem(host, name);
  },

  /**
   * Handles removing all items from the storage
   */
  onRemoveAll: function () {
    // Cannot use this.currentActor() if the handler is called from the
    // tree context menu: it returns correct value only after the table
    // data from server are successfully fetched (and that's async).
    let [type, host, ...path] = this.tree.selectedItem;
    let actor = this.storageTypes[type];
    let name = path.length > 0 ? JSON.stringify(path) : undefined;
    actor.removeAll(host, name);
  },

  /**
   * Handles removing all cookies with exactly the same domain as the
   * cookie in the selected row.
   */
  onRemoveAllFrom: function () {
    let [, host] = this.tree.selectedItem;
    let actor = this.getCurrentActor();
    let rowId = this.table.contextMenuRowId;
    let data = this.table.items.get(rowId);

    actor.removeAll(host, data.host);
  },

  onRemoveDatabase: function () {
    let [type, host, name] = this.tree.selectedItem;
    let actor = this.storageTypes[type];

    actor.removeDatabase(host, name).then(result => {
      if (result.blocked) {
        let notificationBox = this._toolbox.getNotificationBox();
        notificationBox.appendNotification(
          L10N.getFormatStr("storage.idb.deleteBlocked", name),
          "storage-idb-delete-blocked",
          null,
          notificationBox.PRIORITY_WARNING_LOW);
      }
    }).catch(error => {
      let notificationBox = this._toolbox.getNotificationBox();
      notificationBox.appendNotification(
        L10N.getFormatStr("storage.idb.deleteError", name),
        "storage-idb-delete-error",
        null,
        notificationBox.PRIORITY_CRITICAL_LOW);
    });
  }
};
