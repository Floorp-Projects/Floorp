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
Storage = {
  GROUP_DATA_IDENTIFIER:  "tabcandy-group",
  GROUPS_DATA_IDENTIFIER: "tabcandy-groups",
  TAB_DATA_IDENTIFIER:    "tabcandy-tab",
  UI_DATA_IDENTIFIER:    "tabcandy-ui",

  // ----------
  init: function() {
    this._sessionStore = Components.classes["@mozilla.org/browser/sessionstore;1"]
                                   .getService(Components.interfaces.nsISessionStore);
  },

  // ----------
  wipe: function() {
    try {
      var win = Utils.getCurrentWindow();
      
      var self = this;
      
      // ___ Tabs
      Tabs.forEach(function(tab) {
        self.saveTab(tab.raw, null);
      });
      
      // ___ Other
      this.saveGroupsData(win, {});
      this.saveUIData(win, {});
      
      this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
        JSON.stringify({}));
    } catch (e) {
      Utils.log("Error in wipe: "+e);
    }
  },
  
  // ----------
  saveTab: function(tab, data) {
    Utils.assert('tab', tab);

    this._sessionStore.setTabValue(tab, this.TAB_DATA_IDENTIFIER,
      JSON.stringify(data));
  },

  // ----------
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
  saveGroup: function(win, data) {
    var id = data.id;
    var existingData = this.readGroupData(win);
    existingData[id] = data;
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
  deleteGroup: function(win, id) {
    var existingData = this.readGroupData(win);
    delete existingData[id];
    this._sessionStore.setWindowValue(win, this.GROUP_DATA_IDENTIFIER,
      JSON.stringify(existingData));
  },

  // ----------
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
  saveGroupsData: function(win, data) {
    this.saveData(win, this.GROUPS_DATA_IDENTIFIER, data);
  },

  // ----------
  readGroupsData: function(win) {
    return this.readData(win, this.GROUPS_DATA_IDENTIFIER);
  },
  
  // ----------
  saveUIData: function(win, data) {
    this.saveData(win, this.UI_DATA_IDENTIFIER, data);
  },

  // ----------
  readUIData: function(win) {
    return this.readData(win, this.UI_DATA_IDENTIFIER);
  },
  
  // ----------
  saveData: function(win, id, data) {
    try {
      this._sessionStore.setWindowValue(win, id, JSON.stringify(data));
    } catch (e) {
      Utils.log("Error in saveData: "+e);
    }
    
/*     Utils.log('save data', id, data); */
  },

  // ----------
  readData: function(win, id) {
    var existingData = {};
    try {
      var data = this._sessionStore.getWindowValue(win, id);
      if(data)
        existingData = JSON.parse(data);
    } catch (e) {
      Utils.log("Error in readData: "+e);
    }
    
/*     Utils.log('read data', id, existingData); */
    return existingData;
  }
};

Storage.init();

