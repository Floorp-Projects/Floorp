# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is
# Doron Rosenberg.
# Portions created by the Initial Developer are Copyright (C) 2001
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

var gPluginTypes = null;
var gPluginTypesList = null;

var gPrefs = null;

const kDisabledPluginTypesPref = "browser.download.pluginOverrideTypes";
const kPluginHandlerContractID = "@mozilla.org/content/plugin/document-loader-factory;1";
const kPluginOverrideTypesNotHandled = "browser.download.pluginOverrideTypesNotHandled";

function init()
{
  gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  
  gPluginTypes = new PluginTypes();

  // Initialize the File Type list
  gPluginTypesList = document.getElementById("pluginTypesList");
  gPluginTypesList.treeBoxObject.view = gPluginTypes;
  
  var x = document.documentElement.getAttribute("screenX");
  if (x == "")
    setTimeout(centerOverParent, 0);  
}

// This should actually go into some sort of pref fe utilities file. 
function centerOverParent()
{
  var parent = window.opener;
  
  x = parent.screenX + (parent.outerWidth / 2) - (window.outerWidth / 2);
  y = parent.screenY + (parent.outerHeight / 2) - (window.outerHeight / 2);
  window.moveTo(x, y);
}

function onAccept()
{
  var catman = Components.classes["@mozilla.org/categorymanager;1"].getService(Components.interfaces.nsICategoryManager);

  // Update the disabled plugin types pref and the category manager.
  var disabled = "";
  for (var i = 0; i < gPluginTypes._pluginTypes.length; ++i) {
    var currType = gPluginTypes._pluginTypes[i];

    if (!currType.pluginEnabled) {
      for (var j = 0; j < currType.MIMETypes.length; ++j) {
        disabled += currType.MIMETypes[j] + ",";
        catman.deleteCategoryEntry("Gecko-Content-Viewers", currType.MIMETypes[j], false);
      }
    }
    else {
      // We could have re-activated a deactivated Plugin handler, add all the handlers back again, 
      // replacing the existing ones. 
      for (j = 0; j < currType.MIMETypes.length; ++j)
        catman.addCategoryEntry("Gecko-Content-Viewers", currType.MIMETypes[j], 
                                kPluginHandlerContractID, false, true);
    }
  }
  
  if (disabled != "") {  
    disabled = disabled.substr(0, disabled.length - 1);
    
    gPrefs.setCharPref(kDisabledPluginTypesPref, disabled);
  }
  else if (gPrefs.prefHasUserValue(kDisabledPluginTypesPref)) {
    // Clear the pref if we've restored plugin functionality to all
    // listed types. 
    gPrefs.clearUserPref(kDisabledPluginTypesPref); 
  }
}

function PluginType (aPluginEnabled, aMIMEInfo) 
{
  this.pluginEnabled = aPluginEnabled;
  this.MIMEInfo = aMIMEInfo;
  this.MIMETypes = [this.MIMEInfo.MIMEType];
}

function PluginTypes()
{
  var atomSvc = Components.classes["@mozilla.org/atom-service;1"].getService(Components.interfaces.nsIAtomService);
  this._enabledAtom = atomSvc.getAtom("enabled");

  this._pluginTypes = [];
  this._pluginTypeHash = { };

  // Read enabled plugin type information from the category manager
  var catman = Components.classes["@mozilla.org/categorymanager;1"].getService(Components.interfaces.nsICategoryManager);
  var types = catman.enumerateCategory("Gecko-Content-Viewers");
  while (types.hasMoreElements()) {
    var currType = types.getNext().QueryInterface(Components.interfaces.nsISupportsCString).data;
    var contractid = catman.getCategoryEntry("Gecko-Content-Viewers", currType);
    if (contractid == kPluginHandlerContractID)
      this.addType(currType, true);
  }
  
  // Read disabled plugin type information from preferences
  try {
    var types = gPrefs.getCharPref(kDisabledPluginTypesPref);
    var disabledTypes = types.split(",");
    
    // Don't show types for which there is no plugin configured, even if that type
    // is listed in the override list. This may be because the user is running a different
    // build with a profile that was used in another build that had other plugins installed.
    // We don't want to clobber the override list and we also don't want to display the
    // entries for the plugins that aren't configured. 
    var validOverrides = { };
    if (gPrefs.prefHasUserValue(kPluginOverrideTypesNotHandled)) {
      var notHandledTypes = gPrefs.getCharPref(kPluginOverrideTypesNotHandled);
      notHandledTypes = notHandledTypes.split(",");
      
      for (var i = 0; i < notHandledTypes.length; ++i)
        validOverrides[notHandledTypes[i]] = notHandledTypes[i];
    }
    
    for (i = 0; i < disabledTypes.length; ++i) {
      if (!(disabledTypes[i] in validOverrides)) 
        this.addType(disabledTypes[i], false);
    }
  }
  catch (e) { }
  
  this._pluginTypes.sort(this.sortCallback);
}

PluginTypes.prototype = {
  addType: function (aMIMEType, aPluginEnabled)
  {
    var mimeInfo = this.getMIMEInfoForType(aMIMEType);
    if (mimeInfo) {
      // We only want to show one entry per extension, even if several MIME
      // types map to that extension, e.g. audio/wav, audio/x-wav. Hash the
      // primary extension for the type to prevent duplicates. If we encounter
      // a duplicate, record the additional MIME type so we know to deactivate
      // support for it too if the user deactivates support for the extension.
      try {
        var primExt = mimeInfo.primaryExtension;
      } catch (e) { }

      if (primExt) {
        // If there's no extension, there's nothing to do here.
        if (!(primExt in this._pluginTypeHash)) {
          var pluginType = new PluginType(aPluginEnabled, mimeInfo);
          this._pluginTypeHash[primExt] = pluginType;
          this._pluginTypes.push(pluginType);
        }
        // We check that the primary extension has already been hashed
        // by a plugin that is installed in this build before adding a disable
        // override to the list. Otherwise we might end up showing disabled
        // plugin entries for plugins that actually aren't installed and configured
        // in this build, but were for another build that was using this profile.
        else {
          // Append this MIME type to the list of MIME types for the extension.
          var pluginType = this._pluginTypeHash[primExt];
          pluginType.MIMETypes.push(mimeInfo.MIMEType);
        }
      }
    }
  },

  sortCallback: function (a, b)
  {
    try {
      var ac = a.MIMEInfo.primaryExtension.toLowerCase();
      var bc = b.MIMEInfo.primaryExtension.toLowerCase();
    } catch (e) { }
    
    if (ac < bc) 
      return -1;
    else if (ac > bc) 
      return 1;
    return 0;
  },
#if 0
  sortAscending: true,
  sortCallback: function (a, b)
  {
    var ac = gPluginTypes.getSortProperty(a);
    var bc = gPluginTypes.getSortProperty(b);
    
    if (ac < bc) 
      return gPluginTypes.sortAscending ? -1 : 1;
    else if (ac > bc) 
      return gPluginTypes.sortAscending ? 1 : -1;
    return 0;
  },
  
  sortProperty: "fileExtension",
  getSortProperty: function (aObject)
  {
    switch (sortProperty) {
    case "fileExtension":
      return aObject.MIMEInfo.primaryExtension.toLowerCase();
    case "fileType":
      var desc = aObject.MIMEInfo.description;
      // XXXben localize
      return desc || aObject.MIMEInfo.primaryExtension.toUpperCase() + " file";
    case "pluginEnabled":
      return aObject.pluginEnabled;
    }
    return null;
  },
#endif

  getMIMEInfoForType: function (aType)
  {
    try {
      var mimeSvc = Components.classes["@mozilla.org/mime;1"].getService(Components.interfaces.nsIMIMEService);
      return mimeSvc.getFromTypeAndExtension(aType, "");
    }
    catch (e) { }
    
    return null;
  },

  // nsITreeView
  get rowCount() 
  { 
    return this._pluginTypes.length; 
  },

  getCellProperties: function (aRow, aCol, aProperties) 
  { 
    if (aCol.id == "pluginEnabled") {
      if (this._pluginTypes[aRow].pluginEnabled) 
        aProperties.AppendElement(this._enabledAtom);
    }
  },
  
  getImageSrc: function (aRow, aCol) 
  { 
    if (aCol.id == "fileExtension") 
      return "moz-icon://goat." + this._pluginTypes[aRow].MIMEInfo.primaryExtension + "?size=16";
    
    return null;
  },
  
  getCellText: function (aRow, aCol) 
  { 
    var mimeInfo = this._pluginTypes[aRow].MIMEInfo;
    
    if (aCol.id == "fileType") {
      var desc = mimeInfo.description;
      // XXXben localize
      return desc || mimeInfo.primaryExtension.toUpperCase() + " file";
    }
    else if (aCol.id == "fileExtension")
      return mimeInfo.primaryExtension.toUpperCase();

    return "";    
  },
  
  cycleCell: function (aRow, aCol) 
  { 
    if (aCol.id == "pluginEnabled") {
      this._pluginTypes[aRow].pluginEnabled = !this._pluginTypes[aRow].pluginEnabled;
      gPluginTypesList.treeBoxObject.invalidateCell(aRow, aCol);
    }
  },

  selection: null,
  
  // Stuff we don't need...
  isContainer: function (aRow) { return false; },
  isContainerOpen: function (aRow) { return false; },
  isContainerEmpty: function (aRow) { return true; },
  isSeparator: function (aRow) { return false; },
  isSorted: function () { return false; },
  canDrop: function (aRow, aOrientation) { return false; },
  drop: function (aRow, aOrientation) { },
  getParentIndex: function (aRow) { },
  hasNextSibling: function (aRow, aNextIndex) { },
  getLevel: function (aRow) { },
  getProgressMode: function (aRow, aCol) { },
  getCellValue: function (aRow, aCol) { },
  setTree: function (aTree) { },
  toggleOpenState: function (aRow) { },
  cycleHeader: function (aCol) { },
  selectionChanged: function () { },
  isEditable: function (aRow, aCol) { },
  setCellValue: function (aRow, aCol, aValue) { },
  setCellText: function (aRow, aCol, aValue) { },
  performAction: function (aAction) { },
  performActionOnRow: function (aAction, aRow) { },
  performActionOnCell: function (aAction, aRow, aCol) { },
  getRowProperties: function (aRow, aProperties) { },
  getColumnProperties: function (aCol, aProperties) { }

};

