/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu} = require("chrome");
const EventEmitter = require("devtools/shared/event-emitter");
const {LocalizationHelper} = require("devtools/client/shared/l10n");

loader.lazyRequireGetter(this, "TreeWidget",
                         "devtools/client/shared/widgets/TreeWidget", true);
loader.lazyRequireGetter(this, "TableWidget",
                         "devtools/client/shared/widgets/TableWidget", true);
loader.lazyImporter(this, "ViewHelpers",
  "resource://devtools/client/shared/widgets/ViewHelpers.jsm");
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
var StorageUI = this.StorageUI = function StorageUI(front, target, panelWin) {
  EventEmitter.decorate(this);

  this._target = target;
  this._window = panelWin;
  this._panelDoc = panelWin.document;
  this.front = front;

  let treeNode = this._panelDoc.getElementById("storage-tree");
  this.tree = new TreeWidget(treeNode, {defaultType: "dir"});
  this.onHostSelect = this.onHostSelect.bind(this);
  this.tree.on("select", this.onHostSelect);

  let tableNode = this._panelDoc.getElementById("storage-table");
  this.table = new TableWidget(tableNode, {
    emptyText: L10N.getStr("table.emptyText"),
    highlightUpdated: true,
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

  this.front.listStores().then(storageTypes => {
    this.populateStorageTree(storageTypes);
  }).then(null, console.error);

  this.onUpdate = this.onUpdate.bind(this);
  this.front.on("stores-update", this.onUpdate);
  this.onCleared = this.onCleared.bind(this);
  this.front.on("stores-cleared", this.onCleared);

  this.handleKeypress = this.handleKeypress.bind(this);
  this._panelDoc.addEventListener("keypress", this.handleKeypress);
};

exports.StorageUI = StorageUI;

StorageUI.prototype = {

  storageTypes: null,
  shouldResetColumns: true,
  shouldLoadMoreItems: true,

  set animationsEnabled(value) {
    this._panelDoc.documentElement.classList.toggle("no-animate", !value);
  },

  destroy: function() {
    this.table.off(TableWidget.EVENTS.ROW_SELECTED, this.displayObjectSidebar);
    this.table.off(TableWidget.EVENTS.SCROLL_END, this.handleScrollEnd);
    this.table.off(TableWidget.EVENTS.CELL_EDIT, this.editItem);
    this.table.destroy();

    this.front.off("stores-update", this.onUpdate);
    this.front.off("stores-cleared", this.onCleared);
    this._panelDoc.removeEventListener("keypress", this.handleKeypress);
    this.searchBox.removeEventListener("input", this.filterItems);
    this.searchBox = null;
  },

  /**
   * Empties and hides the object viewer sidebar
   */
  hideSidebar: function() {
    this.view.empty();
    this.sidebar.hidden = true;
    this.table.clearSelection();
  },

  getCurrentActor: function() {
    let type = this.table.datatype;

    return this.storageTypes[type];
  },

  makeFieldsEditable: function() {
    let actor = this.getCurrentActor();

    if (typeof actor.getEditableFields !== "undefined") {
      actor.getEditableFields().then(fields => {
        this.table.makeFieldsEditable(fields);
      });
    } else if (this.table._editableFieldsEngine) {
      this.table._editableFieldsEngine.destroy();
    }
  },

  editItem: function(eventType, data) {
    let actor = this.getCurrentActor();

    actor.editItem(data);
  },

  /**
   * Removes the given item from the storage table. Reselects the next item in
   * the table and repopulates the sidebar with that item's data if the item
   * being removed was selected.
   */
  removeItemFromTable: function(name) {
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
   * @param {object} argument0
   *        An object containing which storage types were cleared
   */
  onCleared: function(response) {
    let [type, host] = this.tree.selectedItem;
    if (response.hasOwnProperty(type) && response[type].indexOf(host) > -1) {
      this.table.clear();
      this.hideSidebar();
      this.emit("store-objects-cleared");
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
  onUpdate: function({ changed, added, deleted }) {
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
  handleAddedItems: function(added) {
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
  handleDeletedItems: function(deleted) {
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
        } else if (this.tree.isSelected([type, host])) {
          for (let name of deleted[type][host]) {
            try {
              // trying to parse names in case its for indexedDB
              let names = JSON.parse(name);
              if (!this.tree.isSelected([type, host, names[0], names[1]])) {
                return;
              }
              if (!names[2]) {
                this.tree.selectPreviousItem();
                this.tree.remove([type, host, names[0], names[1]]);
                this.table.clear();
                this.hideSidebar();
              } else {
                this.removeItemFromTable(names[2]);
              }
            } catch (ex) {
              this.removeItemFromTable(name);
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
  handleChangedItems: function(changed) {
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
  fetchStorageObjects: function(type, host, names, reason) {
    let fetchOpts = reason === REASON.NEXT_50_ITEMS ? {offset: this.itemOffset}
                                                    : {};
    let storageType = this.storageTypes[type];

    if (reason !== REASON.NEXT_50_ITEMS &&
        reason !== REASON.UPDATE &&
        reason !== REASON.NEW_ROW &&
        reason !== REASON.POPULATE) {
      throw new Error("Invalid reason specified");
    }

    storageType.getStoreObjects(host, names, fetchOpts).then(({data}) => {
      if (!data.length) {
        this.emit("store-objects-updated");
        return;
      }
      if (this.shouldResetColumns) {
        this.resetColumns(data[0], type);
      }
      this.populateTable(data, reason);
      this.emit("store-objects-updated");

      this.makeFieldsEditable();
    }, Cu.reportError);
  },

  /**
   * Populates the storage tree which displays the list of storages present for
   * the page.
   *
   * @param {object} storageTypes
   *        List of storages and their corresponding hosts returned by the
   *        StorageFront.listStores call.
   */
  populateStorageTree: function(storageTypes) {
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
              this.fetchStorageObjects(type, host, [name], REASON.POPULATE);
            }
          } catch (ex) {
            // Do Nothing
          }
        }
        if (!this.tree.selectedItem) {
          this.tree.selectedItem = [type, host];
          this.fetchStorageObjects(type, host, null, REASON.POPULATE);
        }
      }
    }
  },

  /**
   * Populates the selected entry from teh table in the sidebar for a more
   * detailed view.
   */
  displayObjectSidebar: function() {
    let item = this.table.selectedRow;
    if (!item) {
      // Make sure that sidebar is hidden and return
      this.sidebar.hidden = true;
      return;
    }
    this.sidebar.hidden = false;
    this.view.empty();
    let mainScope = this.view.addScope(L10N.getStr("storage.data.label"));
    mainScope.expanded = true;

    if (item.name && item.valueActor) {
      let itemVar = mainScope.addItem(item.name + "", {}, {relaxed: true});

      item.valueActor.string().then(value => {
        // The main area where the value will be displayed
        itemVar.setGrip(value);

        // May be the item value is a json or a key value pair itself
        this.parseItemValue(item.name, value);

        // By default the item name and value are shown. If this is the only
        // information available, then nothing else is to be displayed.
        let itemProps = Object.keys(item);
        if (itemProps.length == 3) {
          this.emit("sidebar-updated");
          return;
        }

        // Display any other information other than the item name and value
        // which may be available.
        let rawObject = Object.create(null);
        let otherProps =
          itemProps.filter(e => e != "name" &&
                                e != "value" &&
                                e != "valueActor");
        for (let prop of otherProps) {
          rawObject[prop] = item[prop];
        }
        itemVar.populate(rawObject, {sorted: true});
        itemVar.twisty = true;
        itemVar.expanded = true;
        this.emit("sidebar-updated");
      });
      return;
    }

    // Case when displaying IndexedDB db/object store properties.
    for (let key in item) {
      mainScope.addItem(key, {}, true).setGrip(item[key]);
      this.parseItemValue(key, item[key]);
    }
    this.emit("sidebar-updated");
  },

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
  parseItemValue: function(name, value) {
    let json = null;
    try {
      json = JSON.parse(value);
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
  _extractKeyValPairs: function(value) {
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
        let regex = new RegExp("^([^" + kv + p + "]*" + kv + "+[^" + kv + p +
                               "]*" + p + "*)+$", "g");
        if (value.match && value.match(regex) && value.includes(kv) &&
            (value.includes(p) || value.split(kv).length == 2)) {
          return makeObject(kv, p);
        }
      }
    }
    // Testing for array
    for (let p of separators) {
      let regex = new RegExp("^[^" + p + "]+(" + p + "+[^" + p + "]*)+$", "g");
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
  onHostSelect: function(event, item) {
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
    this.shouldResetColumns = true;
    this.fetchStorageObjects(type, host, names, REASON.POPULATE);
    this.itemOffset = 0;
  },

  /**
   * Resets the column headers in the storage table with the pased object `data`
   *
   * @param {object} data
   *        The object from which key and values will be used for naming the
   *        headers of the columns
   * @param {string} type
   *        The type of storage corresponding to the after-reset columns in the
   *        table.
   */
  resetColumns: function(data, type) {
    let columns = {};
    let uniqueKey = null;
    for (let key in data) {
      if (!uniqueKey) {
        this.table.uniqueId = uniqueKey = key;
      }
      columns[key] = key;
      try {
        columns[key] = L10N.getStr("table.headers." + type + "." + key);
      } catch (e) {
        console.error("Unable to localize table header type:" + type +
                      " key:" + key);
      }
    }
    this.table.setColumns(columns, null, HIDDEN_COLUMNS);
    this.table.datatype = type;
    this.shouldResetColumns = false;
    this.hideSidebar();
  },

  /**
   * Populates or updates the rows in the storage table.
   *
   * @param {array[object]} data
   *        Array of objects to be populated in the storage table
   * @param {Constant} reason
   *        See REASON constant at top of file.
   */
  populateTable: function(data, reason) {
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
  handleKeypress: function(event) {
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
  handleScrollEnd: function() {
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
  }
};
