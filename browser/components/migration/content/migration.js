const nsIBPM = Components.interfaces.nsIBrowserProfileMigrator;

function MigrationItem(aID, aKey)
{
  this._id = aID;
  this._key = aKey;
}

var MigrationWizard = {
  _items:    [new MigrationItem(nsIBPM.SETTINGS,  "settings"),
              new MigrationItem(nsIBPM.COOKIES,   "cookies"),
              new MigrationItem(nsIBPM.HISTORY,   "history"),
              new MigrationItem(nsIBPM.FORMDATA,  "formdata"),
              new MigrationItem(nsIBPM.PASSWORDS, "passwords"),
              new MigrationItem(nsIBPM.BOOKMARKS, "bookmarks"),
              new MigrationItem(nsIBPM.OTHERDATA, "otherdata")],
  _dataSources: { 
#ifdef XP_WIN
    "ie":       { _migrate: [nsIBPM.SETTINGS, nsIBPM.COOKIES, nsIBPM.HISTORY, nsIBPM.FORMDATA, nsIBPM.PASSWORDS, nsIBPM.BOOKMARKS], 
                   _import: [0, 1, 2, 3, 4, 5] },
#endif
#ifdef XP_MACOSX
    "safari":   { _migrate: [nsIBPM.SETTINGS, nsIBPM.COOKIES, nsIBPM.HISTORY, nsIBPM.BOOKMARKS],
                   _import: [0, 1, 2, 5] },
    "omniweb":  { _migrate: [],
                   _import: [] },
    "macie":    { _migrate: [],
                   _import: [] },
#endif
    "opera":    { _migrate: [nsIBPM.SETTINGS, nsIBPM.COOKIES, nsIBPM.HISTORY, nsIBPM.BOOKMARKS, nsIBPM.OTHERDATA],    
                   _import: [0, 1, 2, 5, 6] },
    "dogbert":  { _migrate: [nsIBPM.SETTINGS, nsIBPM.COOKIES, nsIBPM.BOOKMARKS],          
                   _import: [1, 5] },
    "seamonkey":{ _migrate: [nsIBPM.SETTINGS, nsIBPM.COOKIES, nsIBPM.HISTORY, nsIBPM.PASSWORDS, nsIBPM.BOOKMARKS, nsIBPM.OTHERDATA], 
                   _import: [1, 4, 5] },
  },
  
  _source: "",
  _itemsFlags: 0,
  _selectedIndices: [],
  _selectedProfile: null,
  _wiz: null,
  _migrator: null,
  _autoMigrate: false,

  init: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "Migration:Started", false);
    os.addObserver(this, "Migration:ItemBeforeMigrate", false);
    os.addObserver(this, "Migration:ItemAfterMigrate", false);
    os.addObserver(this, "Migration:Ended", false);
    
    this._wiz = document.documentElement;

    if ("arguments" in window) {
      this._source = window.arguments[0];
      this._migrator = window.arguments[1].QueryInterface(nsIBPM);
      this._automigrate = true;
      
      // Advance past the first page
      this._wiz.advance();
    }
  },
  
  uninit: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "Migration:Started");
    os.removeObserver(this, "Migration:ItemBeforeMigrate");
    os.removeObserver(this, "Migration:ItemAfterMigrate");
    os.removeObserver(this, "Migration:Ended");
  },
  
  // 1 - Import Source
  onImportSourcePageShow: function ()
  {
    document.documentElement.getButton("back").disabled = true;

    var importSourceGroup = document.getElementById("importSourceGroup");
    importSourceGroup.selectedItem = document.getElementById(this._source == "" ? "ie" : this._source);
  },
  
  onImportSourcePageAdvanced: function ()
  {
    if (!this._automigrate)
      this._source = document.getElementById("importSourceGroup").selectedItem.id;

    // Create the migrator for the selected source.
    if (!this._migrator) {
      var contractID = "@mozilla.org/profile/migrator;1?app=browser&type=" + this._source;
      dump("*** contractID = " + contractID + "\n");
      this._migrator = Components.classes[contractID].createInstance(nsIBPM);
    }
    
    dump("*** source = " + this._source + "\n");

    switch (this._source) {
# Opera only supports profiles on Windows. 
#ifdef XP_WIN
    case "opera":
#endif
    case "dogbert":
    case "seamonkey":
      // check for more than one Opera profile
      this._wiz.currentPage.next = this._migrator.sourceHasMultipleProfiles ? "selectProfile" : "importItems";
      break;
    default:
      // Don't show the Select Profile page for sources that don't support
      // multiple profiles
      this._wiz.currentPage.next = "importItems";
      break;
    }
  },
  
  // 2 - [Profile Selection]
  onSelectProfilePageShow: function ()
  {
    if (this._automigrate)
      document.documentElement.getButton("back").disabled = true;
      
    var profiles = document.getElementById("profiles");
    while (profiles.hasChildNodes()) 
      profiles.removeChild(profiles.firstChild);
    
    var sourceProfiles = this._migrator.sourceProfiles;
    var count = sourceProfiles.Count();
    for (var i = 0; i < count; ++i) {
      var item = document.createElement("radio");
      var str = sourceProfiles.QueryElementAt(i, Components.interfaces.nsISupportsString);
      item.id = str.data;
      item.setAttribute("label", str.data);
      profiles.appendChild(item);
    }
    
    profiles.selectedItem = this._selectedProfile ? document.getElementById(this._selectedProfile) : profiles.firstChild;
  },
  
  onSelectProfilePageRewound: function ()
  {
    var profiles = document.getElementById("profiles");
    this._selectedProfile = profiles.selectedItem.id;
  },
  
  onSelectProfilePageAdvanced: function ()
  {
    var profiles = document.getElementById("profiles");
    this._selectedProfile = profiles.selectedItem.id;
    
    // If we're automigrating, don't show the item selection page, just grab everything.
    if (this._automigrate) {
      this._itemsFlags = nsIBPM.ALL;
      this._selectedIndices = this._dataSources[this._source]._migrate;
      this._wiz.currentPage.next = "migrating";
    }
  },
  
  // 3 - ImportItems
  onImportItemsPageShow: function ()
  {
    var dataSources = document.getElementById("dataSources");
    while (dataSources.hasChildNodes())
      dataSources.removeChild(dataSources.firstChild);
    
    var bundle = document.getElementById("bundle");
    
    var ds = this._dataSources[this._source]._import;
    for (var i = 0; i < ds.length; ++i) {
      var item = this._items[ds[i]];
      var checkbox = document.createElement("checkbox");
      checkbox.id = item._id;
      checkbox.setAttribute("label", bundle.getString(item._key + "_" + this._source));
      dataSources.appendChild(checkbox);
      if (!this._itemsFlags || this._itemsFlags & item._id)
        checkbox.checked = true;
    }
  },

  onImportItemsPageAdvanced: function ()
  {
    var dataSources = document.getElementById("dataSources");
    var params = 0;
    this._selectedIndices = [];
    for (var i = 0; i < dataSources.childNodes.length; ++i) {
      var checkbox = dataSources.childNodes[i];
      if (checkbox.localName == "checkbox") {
        if (checkbox.checked) {
          params |= parseInt(checkbox.id);
          this._selectedIndices.push(parseInt(checkbox.id));
        }
      }
    }
    this._itemsFlags = params;
  },
  
  onImportItemCommand: function (aEvent)
  {
    var items = document.getElementById("dataSources");
    var checkboxes = items.getElementsByTagName("checkbox");
    
    var oneChecked = false;
    for (var i = 0; i < checkboxes.length; ++i)
      oneChecked = checkboxes[i].checked;
    
    this._wiz.getButton("next").disabled = !oneChecked;
  },
  
  // 4 - Migrating
  onMigratingPageShow: function ()
  {
    this._wiz.getButton("cancel").disabled = true;
    this._wiz.getButton("back").disabled = true;
    this._wiz.getButton("next").disabled = true;

    this._listItems("migratingItems");
    setTimeout(this.onMigratingMigrate, 0, this);
  },
  
  onMigratingMigrate: function (aOuter)
  {
    aOuter._migrator.migrate(aOuter._itemsFlags, aOuter._automigrate, aOuter._selectedProfile);
  },
  
  _listItems: function (aID)
  {
    var items = document.getElementById(aID);
    while (items.hasChildNodes())
      items.removeChild(items.firstChild);
    
    var idToIndex = { "1": 0, "2": 1, "4": 2, "8": 3, "16": 4, "32": 5, "64": 6 };
    var bundle = document.getElementById("bundle");
    for (var i = 0; i < this._selectedIndices.length; ++i) {
      var index = this._selectedIndices[i];
      var label = document.createElement("label");
      var item = this._items[idToIndex[index.toString()]];
      label.id = item._key;
      label.setAttribute("value", bundle.getString(item._key + "_" + this._source));
      items.appendChild(label);
    } 
  },
  
  observe: function (aSubject, aTopic, aData)
  {
    var itemToIndex = { "settings": 0, "cookies": 1, "history": 2, "formdata": 3, "passwords": 4, "bookmarks": 5, "otherdata": 6 };
    switch (aTopic) {
    case "Migration:Started":
      dump("*** started\n");
      break;
    case "Migration:ItemBeforeMigrate":
      dump("*** before " + aData + "\n");
      var index = itemToIndex[aData];
      var item = this._items[index];
      var label = document.getElementById(item._key);
      if (label)
        label.setAttribute("style", "font-weight: bold");
      break;
    case "Migration:ItemAfterMigrate":
      dump("*** after " + aData + "\n");
      var index = itemToIndex[aData];
      var item = this._items[index];
      var label = document.getElementById(item._key);
      if (label)
        label.removeAttribute("style");
      break;
    case "Migration:Ended":
      dump("*** done\n");
      if (this._automigrate) {
        // We're done now.
        window.close();
      }
      else {
        var nextButton = this._wiz.getButton("next");
        nextButton.disabled = false;
        nextButton.click();
      }
      break;
    }
  },
  
  onDonePageShow: function ()
  {
    this._wiz.getButton("cancel").disabled = true;
    this._wiz.getButton("back").disabled = true;
    this._wiz.getButton("finish").disabled = false;
    this._listItems("doneItems");
  }
};

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is The Browser Profile Migrator.
#
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@bengoodger.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

