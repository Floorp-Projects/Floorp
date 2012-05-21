/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// **********
// Title: storage.js

// ##########
// Class: Storage
// Singleton for permanent storage of TabView data.
let Storage = {
  GROUP_DATA_IDENTIFIER: "tabview-group",
  GROUPS_DATA_IDENTIFIER: "tabview-groups",
  TAB_DATA_IDENTIFIER: "tabview-tab",
  UI_DATA_IDENTIFIER: "tabview-ui",

  // ----------
  // Function: toString
  // Prints [Storage] for debug use
  toString: function Storage_toString() {
    return "[Storage]";
  },

  // ----------
  // Function: init
  // Sets up the object.
  init: function Storage_init() {
    this._sessionStore =
      Cc["@mozilla.org/browser/sessionstore;1"].
        getService(Ci.nsISessionStore);
  },

  // ----------
  // Function: uninit
  uninit: function Storage_uninit () {
    this._sessionStore = null;
  },

  // ----------
  // Function: wipe
  // Cleans out all the stored data, leaving empty objects.
  wipe: function Storage_wipe() {
    try {
      var self = this;

      // ___ Tabs
      AllTabs.tabs.forEach(function(tab) {
        self.saveTab(tab, null);
      });

      // ___ Other
      this.saveGroupItemsData(gWindow, {});
      this.saveUIData(gWindow, {});

      this._sessionStore.setWindowValue(gWindow, this.GROUP_DATA_IDENTIFIER,
        JSON.stringify({}));
    } catch (e) {
      Utils.log("Error in wipe: "+e);
    }
  },

  // ----------
  // Function: saveTab
  // Saves the data for a single tab.
  saveTab: function Storage_saveTab(tab, data) {
    Utils.assert(tab, "tab");

    this._sessionStore.setTabValue(tab, this.TAB_DATA_IDENTIFIER,
      JSON.stringify(data));
  },

  // ----------
  // Function: getTabData
  // Load tab data from session store and return it.
  getTabData: function Storage_getTabData(tab) {
    Utils.assert(tab, "tab");

    let existingData = null;

    try {
      let tabData = this._sessionStore.getTabValue(tab, this.TAB_DATA_IDENTIFIER);
      if (tabData != "")
        existingData = JSON.parse(tabData);
    } catch (e) {
      // getTabValue will fail if the property doesn't exist.
      Utils.log(e);
    }

    return existingData;
  },

  // ----------
  // Function: getTabState
  // Returns the current state of the given tab.
  getTabState: function Storage_getTabState(tab) {
    Utils.assert(tab, "tab");
    let tabState;

    try {
      tabState = JSON.parse(this._sessionStore.getTabState(tab));
    } catch (e) {}

    return tabState;
  },

  // ----------
  // Function: saveGroupItem
  // Saves the data for a single groupItem, associated with a specific window.
  saveGroupItem: function Storage_saveGroupItem(win, data) {
    var id = data.id;
    var existingData = this.readGroupItemData(win);
    existingData[id] = data;
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
  // Function: deleteGroupItem
  // Deletes the data for a single groupItem from the given window.
  deleteGroupItem: function Storage_deleteGroupItem(win, id) {
    var existingData = this.readGroupItemData(win);
    delete existingData[id];
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
  // Function: readGroupItemData
  // Returns the data for all groupItems associated with the given window.
  readGroupItemData: function Storage_readGroupItemData(win) {
    var existingData = {};
    let data;
    try {
      data = this._sessionStore.getWindowValue(win, this.GROUP_DATA_IDENTIFIER);
      if (data)
        existingData = JSON.parse(data);
    } catch (e) {
      // getWindowValue will fail if the property doesn't exist
      Utils.log("Error in readGroupItemData: "+e, data);
    }
    return existingData;
  },

  // ----------
  // Function: readWindowBusyState
  // Returns the current busyState for the given window.
  readWindowBusyState: function Storage_readWindowBusyState(win) {
    let state;

    try {
      let data = this._sessionStore.getWindowState(win);
      if (data)
        state = JSON.parse(data);
    } catch (e) {
      Utils.log("Error while parsing window state");
    }

    return (state && state.windows[0].busy);
  },

  // ----------
  // Function: saveGroupItemsData
  // Saves the global data for the <GroupItems> singleton for the given window.
  saveGroupItemsData: function Storage_saveGroupItemsData(win, data) {
    this.saveData(win, this.GROUPS_DATA_IDENTIFIER, data);
  },

  // ----------
  // Function: readGroupItemsData
  // Reads the global data for the <GroupItems> singleton for the given window.
  readGroupItemsData: function Storage_readGroupItemsData(win) {
    return this.readData(win, this.GROUPS_DATA_IDENTIFIER);
  },

  // ----------
  // Function: saveUIData
  // Saves the global data for the <UIManager> singleton for the given window.
  saveUIData: function Storage_saveUIData(win, data) {
    this.saveData(win, this.UI_DATA_IDENTIFIER, data);
  },

  // ----------
  // Function: readUIData
  // Reads the global data for the <UIManager> singleton for the given window.
  readUIData: function Storage_readUIData(win) {
    return this.readData(win, this.UI_DATA_IDENTIFIER);
  },

  // ----------
  // Function: saveVisibilityData
  // Saves visibility for the given window.
  saveVisibilityData: function Storage_saveVisibilityData(win, data) {
    this._sessionStore.setWindowValue(
      win, win.TabView.VISIBILITY_IDENTIFIER, data);
  },

  // ----------
  // Function: saveData
  // Generic routine for saving data to a window.
  saveData: function Storage_saveData(win, id, data) {
    try {
      this._sessionStore.setWindowValue(win, id, JSON.stringify(data));
    } catch (e) {
      Utils.log("Error in saveData: "+e);
    }
  },

  // ----------
  // Function: readData
  // Generic routine for reading data from a window.
  readData: function Storage_readData(win, id) {
    var existingData = {};
    try {
      var data = this._sessionStore.getWindowValue(win, id);
      if (data)
        existingData = JSON.parse(data);
    } catch (e) {
      Utils.log("Error in readData: "+e);
    }

    return existingData;
  }
};

