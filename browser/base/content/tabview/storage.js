/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is storage.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ehsan Akhgari <ehsan@mozilla.com>
 * Ian Gilman <ian@iangilman.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

    if (data != null) {
      let imageData = data.imageData;
      // Remove imageData from payload
      delete data.imageData;

      if (imageData != null)
        ThumbnailStorage.saveThumbnail(tab, imageData);
    }

    this._sessionStore.setTabValue(tab, this.TAB_DATA_IDENTIFIER,
      JSON.stringify(data));
  },

  // ----------
  // Function: getTabData
  // Load tab data from session store and return it. Asynchrously loads the tab's
  // thumbnail from the cache and calls <callback>(imageData) when done.
  getTabData: function Storage_getTabData(tab, callback) {
    Utils.assert(tab, "tab");
    Utils.assert(typeof callback == "function", "callback arg must be a function");

    let existingData = null;

    try {
      let tabData = this._sessionStore.getTabValue(tab, this.TAB_DATA_IDENTIFIER);
      if (tabData != "") {
        existingData = JSON.parse(tabData);
      }
    } catch (e) {
      // getTabValue will fail if the property doesn't exist.
      Utils.log(e);
    }

    if (existingData) {
      ThumbnailStorage.loadThumbnail(
        tab, existingData.url,
        function(status, imageData) { 
          callback(imageData);
        }
      );
    }
    return existingData;
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
  // Function: saveActiveGroupName
  // Saves the active group's name for the given window.
  saveActiveGroupName: function Storage_saveActiveGroupName(win) {
    let groupName = win.TabView.getActiveGroupName();
    this._sessionStore.setWindowValue(
      win, win.TabView.LAST_SESSION_GROUP_NAME_IDENTIFIER, groupName);
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

