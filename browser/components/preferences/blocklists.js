/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
const BASE_LIST_ID = "base";
const CONTENT_LIST_ID = "content";
const TRACK_SUFFIX = "-track-digest256";
const TRACKING_TABLE_PREF = "urlclassifier.trackingTable";
const LISTS_PREF_BRANCH = "browser.safebrowsing.provider.mozilla.lists.";
const UPDATE_TIME_PREF = "browser.safebrowsing.provider.mozilla.nextupdatetime";

var gBlocklistManager = {
  _type: "",
  _blockLists: [],
  _brandShortName : null,
  _bundle: null,
  _tree: null,

  _view: {
    _rowCount: 0,
    get rowCount() {
      return this._rowCount;
    },
    getCellText(row, column) {
      if (column.id == "listCol") {
        let list = gBlocklistManager._blockLists[row];
        let desc = list.description ? list.description : "";
        let text = gBlocklistManager._bundle.getFormattedString("mozNameTemplate",
                                                                [list.name, desc]);
        return text;
      }
      return "";
    },

    isSeparator(index) { return false; },
    isSorted() { return false; },
    isContainer(index) { return false; },
    setTree(tree) {},
    getImageSrc(row, column) {},
    getProgressMode(row, column) {},
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
    this._bundle = document.getElementById("bundlePreferences");
    let params = window.arguments[0];
    this.init(params);
  },

  init(params) {
    if (this._type) {
      // reusing an open dialog, clear the old observer
      this.uninit();
    }

    this._type = "tracking";
    this._brandShortName = params.brandShortName;

    let blocklistsText = document.getElementById("blocklistsText");
    while (blocklistsText.hasChildNodes()) {
      blocklistsText.removeChild(blocklistsText.firstChild);
    }
    blocklistsText.appendChild(document.createTextNode(params.introText));

    document.title = params.windowTitle;

    let treecols = document.getElementsByTagName("treecols")[0];
    treecols.addEventListener("click", event => {
      if (event.target.nodeName != "treecol" || event.button != 0) {
        return;
      }
    });

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
      const Cc = Components.classes, Ci = Components.interfaces;
      let msg = this._bundle.getFormattedString("blocklistChangeRequiresRestart",
                                                [this._brandShortName]);
      let title = this._bundle.getFormattedString("shouldRestartTitle",
                                                  [this._brandShortName]);
      let shouldProceed = Services.prompt.confirm(window, title, msg);
      if (shouldProceed) {
        let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                           .createInstance(Ci.nsISupportsPRBool);
        Services.obs.notifyObservers(cancelQuit, "quit-application-requested",
                                     "restart");
        shouldProceed = !cancelQuit.data;

        if (shouldProceed) {
          let trackingTable = Services.prefs.getCharPref(TRACKING_TABLE_PREF);
          if (selected.id != CONTENT_LIST_ID) {
            trackingTable = trackingTable.replace("," + CONTENT_LIST_ID + TRACK_SUFFIX, "");
          } else {
            trackingTable += "," + CONTENT_LIST_ID + TRACK_SUFFIX;
          }
          Services.prefs.setCharPref(TRACKING_TABLE_PREF, trackingTable);
          Services.prefs.setCharPref(UPDATE_TIME_PREF, 42);

          Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit |
                                Ci.nsIAppStartup.eRestart);
        }
      }

      // Don't close the dialog in case we didn't quit.
      return;
    }
    window.close();
  },

  _loadBlockLists() {
    this._blockLists = [];

    // Load blocklists into a table.
    let branch = Services.prefs.getBranch(LISTS_PREF_BRANCH);
    let itemArray = branch.getChildList("");
    for (let itemName of itemArray) {
      try {
        this._createOrUpdateBlockList(itemName);
      } catch (e) {
        // Ignore bogus or missing list name.
        continue;
      }
    }

    this._updateTree();
  },

  _createOrUpdateBlockList(itemName) {
    let branch = Services.prefs.getBranch(LISTS_PREF_BRANCH);
    let key = branch.getCharPref(itemName);
    let value = this._bundle.getString(key);

    let suffix = itemName.slice(itemName.lastIndexOf("."));
    let id = itemName.replace(suffix, "");
    let list = this._blockLists.find(el => el.id === id);
    if (!list) {
      list = { id };
      this._blockLists.push(list);
    }
    list.selected = this._getActiveList() === id;

    // Get the property name from the suffix (e.g. ".name" -> "name").
    let prop = suffix.slice(1);
    list[prop] = value;

    return list;
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

function initWithParams(params) {
  gBlocklistManager.init(params);
}
