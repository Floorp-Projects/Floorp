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

var _elementIDs = ["advancedJavaAllow", "enableJavaScript", "enableImagePref",
                   "popupPolicy", "allowWindowMoveResize", "allowWindowFlip", "allowHideStatusBar", 
                   "allowWindowStatusChange", "allowImageSrcChange"];

var gImagesPref, gImagesEnabled, gImagesRestricted;
var policyButton = null;
var manageTree = null;
var persissionsExist = null;

const nsIPermissionManager = Components.interfaces.nsIPermissionManager;

function Startup()
{
  policyButton = document.getElementById("popupPolicy");
  manageTree = document.getElementById("permissionsTree");
  loadPermissions();
  javascriptEnabledChange()
  
  gImagesPref = document.getElementById("enableImagePref");
  gImagesEnabled = document.getElementById("enableImages");
  gImagesRestricted = document.getElementById("enableRestricted");
  var prefValue = gImagesPref.getAttribute("value");
  if (!prefValue)
    prefValue = "0";
  switch (prefValue) {
  case "1": gImagesRestricted.checked=true;
  case "0": gImagesEnabled.checked=true;
  }
  if (!gImagesEnabled.checked)
    gImagesRestricted.disabled=true;
}

function updateImagePref()
{
  if (!gImagesEnabled.checked) {
    gImagesPref.setAttribute("value", 2)
    gImagesRestricted.disabled=true;
  } else {
    gImagesPref.setAttribute("value", gImagesRestricted.checked?1:0)
    gImagesRestricted.disabled=false;
  }
}

function viewImages() 
{
  openDialog("chrome://browser/content/cookieviewer/CookieViewer.xul","_blank",
              "chrome,resizable=yes,modal", "imageManager" );
}

function advancedJavaScript()
{
  openDialog("chrome://browser/content/pref/pref-advancedscripts.xul", "", 
             "chrome,modal");
}

function javascriptEnabledChange()
{
  var isEnabled = document.getElementById("enableJavaScript").checked;
  var advancedButton = document.getElementById("advancedJavascript");
  advancedButton.disabled = !isEnabled;
}

function togglePermissionEnabling() 
{
  var enabled = policyButton.checked;
  var add = document.getElementById("addPermission");
  var remove1 = document.getElementById("removePermission");
  var remove2 = document.getElementById("removeAllPermissions");
  var description = document.getElementById("popupDescription");
  add.disabled = !enabled;
  if (enabled && persissionsExist) {
    remove1.disabled = false;
    remove2.disabled = false;
  } else {
    remove1.disabled = true;
    remove2.disabled = true;
  }
  description.disabled = !enabled;
  manageTree.disabled = !enabled;
}

/*** =================== PERMISSIONS CODE =================== ***/

var permissionsTreeView = {
  rowCount : 0,
  setTree : function(tree){},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column){
    var rv="";
    if (column=="siteCol") {
      rv = permissions[row].rawHost;
    } else if (column=="statusCol") {
      rv = permissions[row].capability;
    }
    return rv;
  },
  isSeparator : function(index) {return false;},
  isSorted: function() { return false; },
  isContainer : function(index) {return false;},
  cycleHeader : function(aColId, aElt) {},
  getRowProperties : function(row,column,prop){},
  getColumnProperties : function(column,columnElement,prop){},
  getCellProperties : function(row,prop){}
};
var permissionsTree;

var permissions           = [];
var deletedPermissions   = [];

function Permission(number, host, rawHost, type, capability) {
  this.number = number;
  this.host = host;
  this.rawHost = rawHost;
  this.type = type;
  this.capability = capability;
}

var permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);
var popupmanager = Components.classes["@mozilla.org/PopupWindowManager;1"].getService();
popupmanager = popupmanager.QueryInterface(Components.interfaces.nsIPopupWindowManager);

function DeleteAllFromTree
    (tree, view, table, deletedTable, removeButton, removeAllButton) {

  // remove all items from table and place in deleted table
  for (var i=0; i<table.length; i++) {
    deletedTable[deletedTable.length] = table[i];
  }
  table.length = 0;

  // clear out selections
  tree.treeBoxObject.view.selection.select(-1); 

  // redisplay
  view.rowCount = 0;
  tree.treeBoxObject.invalidate();


  // disable buttons
  persissionsExist = false;
  togglePermissionEnabling();
}

function DeleteSelectedItemFromTree
    (tree, view, table, deletedTable, removeButton, removeAllButton) {

  // remove selected items from list (by setting them to null) and place in deleted list
  var selections = GetTreeSelections(tree);
  for (var s=selections.length-1; s>= 0; s--) {
    var i = selections[s];
    deletedTable[deletedTable.length] = table[i];
    table[i] = null;
  }

  // collapse list by removing all the null entries
  for (var j=0; j<table.length; j++) {
    if (table[j] == null) {
      var k = j;
      while ((k < table.length) && (table[k] == null)) {
        k++;
      }
      table.splice(j, k-j);
    }
  }

  // redisplay
  var box = tree.treeBoxObject;
  var firstRow = box.getFirstVisibleRow();
  if (firstRow > (table.length-1) ) {
    firstRow = table.length-1;
  }
  view.rowCount = table.length;
  box.rowCountChanged(0, table.length);
  box.scrollToRow(firstRow)

  // update selection and/or buttons
  if (table.length) {

    // update selection
    // note: we need to deselect before reselecting in order to trigger ...Selected method
    var nextSelection = (selections[0] < table.length) ? selections[0] : table.length-1;
    tree.treeBoxObject.view.selection.select(-1); 
    tree.treeBoxObject.view.selection.select(nextSelection);
    
  } else {

    // disable buttons
    document.getElementById(removeButton).setAttribute("disabled", "true")
    document.getElementById(removeAllButton).setAttribute("disabled","true");

    // clear out selections
    tree.treeBoxObject.view.selection.select(-1); 
    persissionsExist = false;
    togglePermissionEnabling();
  }
}

function GetTreeSelections(tree) {
  var selections = [];
  var select = tree.treeBoxObject.selection;
  if (select) {
    var count = select.getRangeCount();
    var min = new Object();
    var max = new Object();
    for (var i=0; i<count; i++) {
      select.getRangeAt(i, min, max);
      for (var k=min.value; k<=max.value; k++) {
        if (k != -1) {
          selections[selections.length] = k;
        }
      }
    }
  }
  return selections;
}

function loadPermissions() {
  // load permissions into a table
  if (!permissionsTree)
    permissionsTree = document.getElementById("permissionsTree");

  var enumerator = permissionmanager.enumerator;
  var count = 0;
  var contentStr;
  var dialogType = "popup"; // Popups
  
  while (enumerator.hasMoreElements()) {
    var nextPermission = enumerator.getNext();
    nextPermission = nextPermission.QueryInterface(Components.interfaces.nsIPermission);
    if (nextPermission.type == dialogType) {
      var host = nextPermission.host;
      permissions[count] = 
        new Permission(count++, host,
                      (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                      nextPermission.type,
                      "");
    }
  }
  permissionsTreeView.rowCount = permissions.length;

  // sort and display the table
  permissionsTree.treeBoxObject.view = permissionsTreeView;
  
  if (permissions.length == 0)
    persissionsExist = false;
  else
    persissionsExist = true;
    
  togglePermissionEnabling();
}

function PermissionSelected() {
  var selections = GetTreeSelections(permissionsTree);
  document.getElementById("removePermission").disabled = (selections.length < 1);
}

function AddPermission() {
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                              .getService(Components.interfaces.nsIPromptService);

  var stringBundle = document.getElementById("stringBundle");
  var message = stringBundle.getString("enterSiteName");
  var title = stringBundle.getString("enterSiteTitle");

  var name = {};
  if (!promptService.prompt(window, title, message, name, null, {}))
      return;
    
  var nameToURI = name.value.replace(" ", "");    
  var uri = Components.classes['@mozilla.org/network/standard-url;1'].createInstance(Components.interfaces.nsIURI);
  uri.spec = nameToURI;
  permissionmanager.add(uri, "popup", nsIPermissionManager.ALLOW_ACTION);
  loadPermissions();
}

function DeletePermission() {
  DeleteSelectedItemFromTree(permissionsTree, permissionsTreeView,
                                permissions, deletedPermissions,
                                "removePermission", "removeAllPermissions");
  FinalizePermissionDeletions();
}

function DeleteAllPermissions() {
  DeleteAllFromTree(permissionsTree, permissionsTreeView,
                        permissions, deletedPermissions,
                        "removePermission", "removeAllPermissions");
  FinalizePermissionDeletions();
}

function FinalizePermissionDeletions() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);

  for (var p=0; p<deletedPermissions.length; p++) {
    permissionmanager.remove(deletedPermissions[p].host, deletedPermissions[p].type);
  }
  deletedPermissions.length = 0;
}

function HandlePermissionKeyPress(e) {
  if (e.keyCode == 46) {
    DeletePermission();
  }
}

function showFontsAndColors()
{
  openDialog("chrome://browser/content/pref/pref-fonts.xul", "", "centerscreen,chrome,modal=yes,dialog=yes");
}

function saveFontPrefs()
{
  var dataObject = top.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-fonts.xul"];
  var pref = top.hPrefWindow.pref;
  for (var language in dataObject.languageData) {
    for (var type in dataObject.languageData[language].types) {
      var fontPrefString = "font.name." + type + "." + language;
      var currValue = "";
      try {
        currValue = pref.CopyUnicharPref(fontPrefString);
      }
      catch(e) {
      }
      if (currValue != dataObject.languageData[language].types[type])
        pref.SetUnicharPref(fontPrefString, dataObject.languageData[language].types[type]);
    }
    var variableSizePref = "font.size.variable." + language;
    var fixedSizePref = "font.size.fixed." + language;
    var minSizePref = "font.minimum-size." + language;
    var currVariableSize = 12, currFixedSize = 12, minSizeVal = 0;
    try {
      currVariableSize = pref.GetIntPref(variableSizePref);
      currFixedSize = pref.GetIntPref(fixedSizePref );
      minSizeVal = pref.GetIntPref(minSizePref);
    }
    catch(e) {
    }
    if (currVariableSize != dataObject.languageData[language].variableSize)
      pref.SetIntPref(variableSizePref, dataObject.languageData[language].variableSize);
    if (currFixedSize != dataObject.languageData[language].fixedSize)
      pref.SetIntPref(fixedSizePref, dataObject.languageData[language].fixedSize);
    if (minSizeVal != dataObject.languageData[language].minSize) {
      pref.SetIntPref(minSizePref, dataObject.languageData[language].minSize);
    }
  }

  // font scaling
  var fontDPI = parseInt(dataObject.fontDPI);
  var myFonts = dataObject.dataEls["useMyFonts"].checked;
  var defaultFont = dataObject.defaultFont;
  var myColors = dataObject.dataEls["useMyColors"].checked;
  try {
    var currDPI = pref.GetIntPref("browser.display.screen_resolution");
    var currFonts = pref.GetIntPref("browser.display.use_document_fonts");
    var currColors = pref.GetBoolPref("browser.display.use_document_colors");
    var currDefault = pref.CopyUnicharPref( "font.default" );
  }
  catch(e) {
  }

  if (currDPI != fontDPI)
    pref.SetIntPref("browser.display.screen_resolution", fontDPI);
  if (currFonts == myFonts)
    pref.SetIntPref("browser.display.use_document_fonts", !myFonts);
  if(currDefault != defaultFont)
    pref.SetUnicharPref( "font.default", defaultFont );
  if (currColors == myColors)
    pref.SetBoolPref("browser.display.use_document_colors", !myColors);

  var items = ["foregroundText", "background", "unvisitedLinks", "visitedLinks"];
  var prefs = ["browser.display.foreground_color", "browser.display.background_color",
                "browser.anchor_color", "browser.visited_color"];
  var prefvalue;
  for (var i = 0; i < items.length; ++i) {
    prefvalue = dataObject.dataEls[items[i]].value;
    pref.SetUnicharPref(prefs[i], prefvalue)
  }
  items = ["browserUseSystemColors", "browserUnderlineAnchors"];
  prefs = ["browser.display.use_system_colors", "browser.underline_anchors"];
  for (var i = 0; i < items.length; ++i) {
    prefvalue = dataObject.dataEls[items[i]].checked;
    pref.SetBoolPref(prefs[i], prefvalue)
  }
}

