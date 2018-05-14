/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
const BASE_LIST_ID = "base";
const CONTENT_LIST_ID = "content";
const TRACK_SUFFIX = "-track-digest256";
const TRACKING_TABLE_PREF = "urlclassifier.trackingTable";
const LISTS_PREF_BRANCH = "browser.safebrowsing.provider.mozilla.lists.";

var gBlocklistManager = {
  _type: "",
  _blockLists: [],
  _tree: null,

  _view: {
    _rowCount: 0,
    get rowCount() {
      return this._rowCount;
    },
    getCellText(row, column) {
      if (column.id == "listCol") {
        let list = gBlocklistManager._blockLists[row];
        return list.name;
      }
      return "";
    },

    isSeparator(index) { return false; },
    isSorted() { return false; },
    isContainer(index) { return false; },
    setTree(tree) {},
    getImageSrc(row, column) {},
    getCellValue(row, column) {
      if (column.id == "selectionCol")
        return gBlocklistManager._blockLists[row].selected;
      return undefined;
    },
    cycleHeader(column) {},
    getRowProperties(row) { return ""; },
    getColumnProperties(column) { return ""; },
    getCellProperties(row, column) {
      if (column.id == "selectionCol") {
        return "checkmark";
      }

      return "";
    }
  },

  onWindowKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_ESCAPE) {
      window.close();
    } else if (event.keyCode == KeyEvent.DOM_VK_RETURN) {
      gBlocklistManager.onApplyChanges();
    }
  },

  onLoad() {
    let params = window.arguments[0];
    this.init(params);
  },

  init(params) {
    if (this._type) {
      // reusing an open dialog, clear the old observer
      this.uninit();
    }

    this._type = "tracking";

    this._loadBlockLists();
  },

  uninit() {},

  onListSelected() {
    for (let list of this._blockLists) {
      list.selected = false;
    }
    this._blockLists[this._tree.currentIndex].selected = true;

    this._updateTree();
  },

  onApplyChanges() {
    let activeList = this._getActiveList();
    let selected = null;
    for (let list of this._blockLists) {
      if (list.selected) {
        selected = list;
        break;
      }
    }

    if (activeList !== selected.id) {
      let trackingTable = Services.prefs.getCharPref(TRACKING_TABLE_PREF);
      if (selected.id != CONTENT_LIST_ID) {
        trackingTable = trackingTable.replace("," + CONTENT_LIST_ID + TRACK_SUFFIX, "");
      } else {
        trackingTable += "," + CONTENT_LIST_ID + TRACK_SUFFIX;
      }
      Services.prefs.setCharPref(TRACKING_TABLE_PREF, trackingTable);

      // Force an update after changing the tracking protection table.
      let listmanager = Cc["@mozilla.org/url-classifier/listmanager;1"]
                        .getService(Ci.nsIUrlListManager);
      if (listmanager) {
        listmanager.forceUpdates(trackingTable);
      }
    }

    window.close();
  },

  async _loadBlockLists() {
    this._blockLists = [];

    // Load blocklists into a table.
    let branch = Services.prefs.getBranch(LISTS_PREF_BRANCH);
    let itemArray = branch.getChildList("");
    for (let itemName of itemArray) {
      try {
        let list = await this._createBlockList(itemName);
        this._blockLists.push(list);
      } catch (e) {
        // Ignore bogus or missing list name.
        continue;
      }
    }

    this._updateTree();
  },

  async _createBlockList(id) {
    let branch = Services.prefs.getBranch(LISTS_PREF_BRANCH);
    let l10nKey = branch.getCharPref(id);
    let [listName, description] = await document.l10n.formatValues([
      {id: `blocklist-item-${l10nKey}-name`},
      {id: `blocklist-item-${l10nKey}-desc`},
    ]);
    let name = await document.l10n.formatValue(
      "blocklist-item-list-template", {listName, description});

    return {
      id,
      name,
      selected: this._getActiveList() === id,
    };
  },

  _updateTree() {
    this._tree = document.getElementById("blocklistsTree");
    this._view._rowCount = this._blockLists.length;
    this._tree.view = this._view;
  },

  _getActiveList() {
    let trackingTable = Services.prefs.getCharPref(TRACKING_TABLE_PREF);
    return trackingTable.includes(CONTENT_LIST_ID) ? CONTENT_LIST_ID : BASE_LIST_ID;
  }
};
