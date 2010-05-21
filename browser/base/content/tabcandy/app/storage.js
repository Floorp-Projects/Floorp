// Title: storage.js (revision-a)

// ##########
Storage = {
  GROUP_DATA_IDENTIFIER:  "tabcandy-group",
  GROUPS_DATA_IDENTIFIER: "tabcandy-groups",
  TAB_DATA_IDENTIFIER:    "tabcandy-tab",

  // ----------
  init: function() {
    this._sessionStore = Components.classes["@mozilla.org/browser/sessionstore;1"]
                                   .getService(Components.interfaces.nsISessionStore);
  },

  // ----------
  saveTab: function(tab, data) {
/*     Utils.log("AAAAAAAAAAAAAAAAAAAAAAAAAAA saving tab data, tab: " + tab.toSource() + ", data: "+data.toSource()); */
    this._sessionStore.setTabValue(tab, this.TAB_DATA_IDENTIFIER,
      JSON.stringify(data));
  },

  // ----------
  getTabData: function(tab) {
    var existingData = null;
    try {
/*         Utils.log("readTabData: " + this._sessionStore.getTabValue(tab, this.TAB_DATA_IDENTIFIER)); */
      var tabData = this._sessionStore.getTabValue(tab, this.TAB_DATA_IDENTIFIER);
      if (tabData != "") {
        existingData = JSON.parse(tabData);
      }
    } catch (e) {
      // getWindowValue will fail if the property doesn't exist
      Utils.log("Error in readTabData: "+e);
    }
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
/*     Utils.log("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGG " + data.toSource()); */
    this._sessionStore.setWindowValue(win, this.GROUPS_DATA_IDENTIFIER,
      JSON.stringify(data));
  },

  // ----------
  readGroupsData: function(win) {
    var existingData = {};
    try {
/*         Utils.log("readGroupsData:   "+this._sessionStore.getWindowValue(win, this.GROUPS_DATA_IDENTIFIER)); */
      existingData = JSON.parse(
        this._sessionStore.getWindowValue(win, this.GROUPS_DATA_IDENTIFIER)
      );
    } catch (e) {
      // getWindowValue will fail if the property doesn't exist
    }
    return existingData;
  },
  
  // ----------
  read: function() {
    var data = {};
    var file = this.getFile();
    if(file.exists()) {
      var fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].  
                              createInstance(Components.interfaces.nsIFileInputStream);  
      var cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].  
                              createInstance(Components.interfaces.nsIConverterInputStream);  
      fstream.init(file, -1, 0, 0);  
      cstream.init(fstream, "UTF-8", 0, 0); // you can use another encoding here if you wish  
      
      let (str = {}) {  
        cstream.readString(-1, str); // read the whole file and put it in str.value  
        if(str.value)
          data = JSON.parse(str.value);  
      }  
      cstream.close(); // this closes fstream  
    }
   
    return data;
  },
  
  // ----------
  write: function(data) {
    var file = this.getFile();
    var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"].  
                             createInstance(Components.interfaces.nsIFileOutputStream);  
    foStream.init(file, 0x02 | 0x08 | 0x20, 0666, 0);   
    var str = JSON.stringify(data);
    foStream.write(str, str.length);
    foStream.close();
  },
  
  // ----------  
  getFile: function() {
    var file = Components.classes["@mozilla.org/file/directory_service;1"].  
      getService(Components.interfaces.nsIProperties).  
      get("ProfD", Components.interfaces.nsIFile);  
      
    file.append('tabcandy');
    if(!file.exists())
      file.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0777);        
      
    file.append(Switch.name + '.json');
    return file;
  }
};

Storage.init();

