const kIMig = Components.interfaces.nsIMailProfileMigrator;
const kIPStartup = Components.interfaces.nsIProfileStartup;
const kProfileMigratorContractIDPrefix = "@mozilla.org/profile/migrator;1?app=mail&type=";

var MigrationWizard = {
  _source: "",                  // Source Profile Migrator ContractID suffix
  _itemsFlags: kIMig.ALL,       // Selected Import Data Sources (16-bit bitfield)
  _selectedProfile: null,       // Selected Profile name to import from
  _wiz: null,
  _migrator: null,
  _autoMigrate: null,

  init: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "Migration:Started", false);
    os.addObserver(this, "Migration:ItemBeforeMigrate", false);
    os.addObserver(this, "Migration:ItemAfterMigrate", false);
    os.addObserver(this, "Migration:Ended", false);
    os.addObserver(this, "Migration:Progress", false);
    
    this._wiz = document.documentElement;

    if ("arguments" in window) {
      this._source = window.arguments[0];
      this._migrator = window.arguments[1].QueryInterface(kIMig);
      this._autoMigrate = window.arguments[2].QueryInterface(kIPStartup);
      
      // Show the "nothing" option in the automigrate case to provide an
      // easily identifiable way to avoid migration and create a new profile.
      var nothing = document.getElementById("nothing");
      nothing.hidden = false;      
    }
  },
  
  uninit: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "Migration:Started");
    os.removeObserver(this, "Migration:ItemBeforeMigrate");
    os.removeObserver(this, "Migration:ItemAfterMigrate");
    os.removeObserver(this, "Migration:Ended");
    os.removeObserver(this, "Migration:Progress");
  },
  
  // 1 - Import Source
  onImportSourcePageShow: function ()
  {
    document.documentElement.getButton("back").disabled = true;
    
    // Figure out what source apps are are available to import from:
    var group = document.getElementById("importSourceGroup");
    for (var i = 0; i < group.childNodes.length; ++i) {
      var suffix = group.childNodes[i].id;
      if (suffix != "nothing") {
        var contractID = kProfileMigratorContractIDPrefix + suffix;
        var migrator = Components.classes[contractID].createInstance(kIMig);
        if (!migrator.sourceExists)
          group.childNodes[i].hidden = true;
      }
    }
    
    var firstNonDisabled = null;
    for (var i = 0; i < group.childNodes.length; ++i) {
      if (!group.childNodes[i].disabled) {
        firstNonDisabled = group.childNodes[i];
        break;
      }
    }

    group.selectedItem = this._source == "" ? firstNonDisabled : document.getElementById(this._source);
  },
  
  onImportSourcePageAdvanced: function ()
  {
    var newSource = document.getElementById("importSourceGroup").selectedItem.id;
    
    if (newSource == "nothing") {
      document.documentElement.cancel();
      return;
    }
    
    if (!this._migrator || (newSource != this._source)) {
      // Create the migrator for the selected source.
      var contractID = kProfileMigratorContractIDPrefix + newSource;
      this._migrator = Components.classes[contractID].createInstance(kIMig);

      this._itemsFlags = kIMig.ALL;
      this._selectedProfile = null;
    }
    if (!this._autoMigrate)
      this._source = newSource;
      
    // check for more than one source profile
    if (this._migrator.sourceHasMultipleProfiles)
      this._wiz.currentPage.next = "selectProfile";
    else {
      this._wiz.currentPage.next = this._autoMigrate ? "migrating" : "importItems";
      var sourceProfiles = this._migrator.sourceProfiles;
      if (sourceProfiles && sourceProfiles.Count() == 1) {
        var profileName = sourceProfiles.QueryElementAt(0, Components.interfaces.nsISupportsString);
        this._selectedProfile = profileName.data;
      }
      else
        this._selectedProfile = "";
    }
  },
  
  // 2 - [Profile Selection]
  onSelectProfilePageShow: function ()
  {
    // Disabling this for now, since we ask about import sources in automigration
    // too and don't want to disable the back button
    // if (this._autoMigrate)
    //   document.documentElement.getButton("back").disabled = true;
      
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
    if (this._autoMigrate)
      this._wiz.currentPage.next = "migrating";
  },
  
  // 3 - ImportItems
  onImportItemsPageShow: function ()
  {
    var dataSources = document.getElementById("dataSources");
    while (dataSources.hasChildNodes())
      dataSources.removeChild(dataSources.firstChild);
    
    var bundle = document.getElementById("bundle");
    
    var items = this._migrator.getMigrateData(this._selectedProfile, this._autoMigrate);
    for (var i = 0; i < 16; ++i) {
      var itemID = (items >> i) & 0x1 ? Math.pow(2, i) : 0;
      if (itemID > 0) {
        var checkbox = document.createElement("checkbox");
        checkbox.id = itemID;
        checkbox.setAttribute("label", bundle.getString(itemID + "_" + this._source));
        dataSources.appendChild(checkbox);
        if (!this._itemsFlags || this._itemsFlags & itemID)
          checkbox.checked = true;
      }
    }
  },

  onImportItemsPageAdvanced: function ()
  {
    var dataSources = document.getElementById("dataSources");
    this._itemsFlags = 0;
    for (var i = 0; i < dataSources.childNodes.length; ++i) {
      var checkbox = dataSources.childNodes[i];
      if (checkbox.localName == "checkbox" && checkbox.checked)
        this._itemsFlags |= parseInt(checkbox.id);
    }
  },
  
  onImportItemCommand: function (aEvent)
  {
    var items = document.getElementById("dataSources");
    var checkboxes = items.getElementsByTagName("checkbox");

    var oneChecked = false;
    for (var i = 0; i < checkboxes.length; ++i) {
      if (checkboxes[i].checked) {
        oneChecked = true;
        break;
      }
    }

    this._wiz.getButton("next").disabled = !oneChecked;
  },
  
  // 4 - Migrating
  onMigratingPageShow: function ()
  {
    this._wiz.getButton("cancel").disabled = true;
    this._wiz.getButton("back").disabled = true;
    this._wiz.getButton("next").disabled = true;
    
    // When automigrating, show all of the data that can be received from this source.
    if (this._autoMigrate)
      this._itemsFlags = this._migrator.getMigrateData(this._selectedProfile, this._autoMigrate);

    this._listItems("migratingItems");
    setTimeout(this.onMigratingMigrate, 0, this);
  },
  
  onMigratingMigrate: function (aOuter)
  {
    aOuter._migrator.migrate(aOuter._itemsFlags, aOuter._autoMigrate, aOuter._selectedProfile);
  },
  
  _listItems: function (aID)
  {
    var items = document.getElementById(aID);
    while (items.hasChildNodes())
      items.removeChild(items.firstChild);
    
    var bundle = document.getElementById("bundle");
    var itemID;
    for (var i = 0; i < 16; ++i) {
      var itemID = (this._itemsFlags >> i) & 0x1 ? Math.pow(2, i) : 0;
      if (itemID > 0) {
        var label = document.createElement("label");
        label.id = itemID + "_migrated";
        try {
          label.setAttribute("value", "- " + bundle.getString(itemID + "_" + this._source));
          items.appendChild(label);
        }
        catch (e) {
          // if the block above throws, we've enumerated all the import data types we
          // currently support and are now just wasting time, break. 
          break;
        }
      }
    }
  },
  
  observe: function (aSubject, aTopic, aData)
  {
    switch (aTopic) {
    case "Migration:Started":
      dump("*** started\n");
      break;
    case "Migration:ItemBeforeMigrate":
      dump("*** before " + aData + "\n");
      var label = document.getElementById(aData + "_migrated");
      if (label)
        label.setAttribute("style", "font-weight: bold");
      break;
    case "Migration:ItemAfterMigrate":
      dump("*** after " + aData + "\n");
      var label = document.getElementById(aData + "_migrated");
      if (label)
        label.removeAttribute("style");
      break;
    case "Migration:Ended":
      dump("*** done\n");
      if (this._autoMigrate) {
        // We're done now.
        this._wiz.advance();
        setTimeout("close()", 5000);
      }
      else {
        var nextButton = this._wiz.getButton("next");
        nextButton.disabled = false;
        nextButton.click();
      }
      break;
    case "Migration:Progress":
      document.getElementById('progressBar').value = aData;
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

