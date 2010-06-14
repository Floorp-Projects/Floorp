// Title: storage.js (revision-a)

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

