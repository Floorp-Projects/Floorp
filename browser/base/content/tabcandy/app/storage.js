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
 * Ehsan Akhgari <ehsan@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
// Singleton for permanent storage of TabCandy data.
Storage = {
  GROUP_DATA_IDENTIFIER:  "tabcandy-group",
  GROUPS_DATA_IDENTIFIER: "tabcandy-groups",
  TAB_DATA_IDENTIFIER:    "tabcandy-tab",
  UI_DATA_IDENTIFIER:    "tabcandy-ui",
  VISIBILITY_DATA_IDENTIFIER:    "tabcandy-visibility",

  // ----------
  // Function: onReady
  // Calls callback when Storage is ready for business. Could be immediately if it's already ready
  // or later if needed.
  onReady: function(callback) {
    try {
      // ToDo: the session store doesn't expose any public methods/variables for
      // us to check whether it's loaded or not so using a private one for
      // now.
      var alreadyReady = gWindow.__SSi;
      if (alreadyReady) {
        callback();
      } else {
        var obsService =
          Components.classes["@mozilla.org/observer-service;1"]
          .getService(Components.interfaces.nsIObserverService);
        var observer = {
          observe: function(subject, topic, data) {
            try {
              if (topic == "browser-delayed-startup-finished") {
                if (subject == gWindow) {
                  callback();
                }
              }
            } catch(e) {
              Utils.log(e);
            }
          }
        };

        obsService.addObserver(
          observer, "browser-delayed-startup-finished", false);
      }
    } catch(e) {
      Utils.log(e);
    }
  },

  // ----------
  // Function: init
  // Sets up the object.
  init: function() {
    this._sessionStore =
      Components.classes["@mozilla.org/browser/sessionstore;1"]
        .getService(Components.interfaces.nsISessionStore);
  },

  // ----------
  // Function: wipe
  // Cleans out all the stored data, leaving empty objects.
  wipe: function() {
    try {
      var self = this;

      // ___ Tabs
      Tabs.forEach(function(tab) {
        self.saveTab(tab.raw, null);
      });

      // ___ Other
      this.saveGroupsData(gWindow, {});
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
  saveTab: function(tab, data) {
    Utils.assert('tab', tab);

    this._sessionStore.setTabValue(tab, this.TAB_DATA_IDENTIFIER,
      JSON.stringify(data));
  },

  // ----------
  // Function: getTabData
  // Returns the data object associated with a single tab.
  getTabData: function(tab) {
    Utils.assert('tab', tab);

    var existingData = null;
    try {
/*         Utils.log("readTabData: " + this._sessionStore.getTabValue(tab, this.TAB_DATA_IDENTIFIER)); */
      var tabData = this._sessionStore.getTabValue(tab, this.TAB_DATA_IDENTIFIER);
      if (tabData != "") {
        existingData = JSON.parse(tabData);
      }
    } catch (e) {
      // getWindowValue will fail if the property doesn't exist
      Utils.log(e);
    }

/*     Utils.log('tab', existingData); */
    return existingData;
  },

  // ----------
  // Function: saveGroup
  // Saves the data for a single group, associated with a specific window.
  saveGroup: function(win, data) {
    var id = data.id;
    var existingData = this.readGroupData(win);
    existingData[id] = data;
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
  // Function: deleteGroup
  // Deletes the data for a single group from the given window.
  deleteGroup: function(win, id) {
    var existingData = this.readGroupData(win);
    delete existingData[id];
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
  // Function: readGroupData
  // Returns the data for all groups associated with the given window.
  readGroupData: function(win) {
    var existingData = {};
    try {
/*         Utils.log("readGroupData" + this._sessionStore.getWindowValue(win, this.GROUP_DATA_IDENTIFIER)); */
      existingData = JSON.parse(
        this._sessionStore.getWindowValue(win, this.GROUP_DATA_IDENTIFIER)
      );
    } catch (e) {
      // getWindowValue will fail if the property doesn't exist
      Utils.log("Error in readGroupData: "+e);
    }
    return existingData;
  },

  // ----------
  // Function: saveGroupsData
  // Saves the global data for the <Groups> singleton for the given window.
  saveGroupsData: function(win, data) {
    this.saveData(win, this.GROUPS_DATA_IDENTIFIER, data);
  },

  // ----------
  // Function: readGroupsData
  // Reads the global data for the <Groups> singleton for the given window.
  readGroupsData: function(win) {
    return this.readData(win, this.GROUPS_DATA_IDENTIFIER);
  },

  // ----------
  // Function: saveUIData
  // Saves the global data for the <UIClass> singleton for the given window.
  saveUIData: function(win, data) {
    this.saveData(win, this.UI_DATA_IDENTIFIER, data);
  },

  // ----------
  // Function: readUIData
  // Reads the global data for the <UIClass> singleton for the given window.
  readUIData: function(win) {
    return this.readData(win, this.UI_DATA_IDENTIFIER);
  },

  // ----------
  // Function: saveData
  // Generic routine for saving data to a window.
  saveData: function(win, id, data) {
    try {
      this._sessionStore.setWindowValue(win, id, JSON.stringify(data));
    } catch (e) {
      Utils.log("Error in saveData: "+e);
    }

/*     Utils.log('save data', id, data); */
  },

  // ----------
  // Function: readData
  // Generic routine for reading data from a window.
  readData: function(win, id) {
    var existingData = {};
    try {
      var data = this._sessionStore.getWindowValue(win, id);
      if (data)
        existingData = JSON.parse(data);
    } catch (e) {
      Utils.log("Error in readData: "+e);
    }

/*     Utils.log('read data', id, existingData); */
    return existingData;
  }
};

// ----------
Storage.init();
