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
var permType = "popup";
var gImagesPref, gImagesEnabled, gImagesRestricted;

function Startup()
{
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
  
  top.hPrefWindow.registerOKCallbackFunc(onPopupPrefsOK);
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
  openDialog("chrome://browser/content/pref/pref-features-images.xul","_blank",
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

function AddPermission() {
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                              .getService(Components.interfaces.nsIPromptService);

  var stringBundle = document.getElementById("stringBundle");
  var message = stringBundle.getString("enterSiteName");
  var title = stringBundle.getString("enterSiteTitle");

  var name = {};
  if (!promptService.prompt(window, title, message, name, null, {}))
      return;
  
  var host = name.value.replace(/ /g, "");
  permissions[permissions.length] = new Permission(permissions.length, host,
                                                   (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                                                   "popup",
                                                   "");
  permissionsTreeView.rowCount = permissions.length;
  permissionsTree.treeBoxObject.rowCountChanged(permissions.length-1, 1);
  permissionsTree.treeBoxObject.ensureRowIsVisible(permissions.length-1)
}

function showFontsAndColors()
{
  openDialog("chrome://browser/content/pref/pref-fonts.xul", "", "centerscreen,chrome,modal=yes,dialog=yes");
}

function saveFontPrefs()
{
  var dataObject = top.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-fonts.xul"].userData;
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
  for (i = 0; i < items.length; ++i) {
    prefvalue = dataObject.dataEls[items[i]].checked;
    pref.SetBoolPref(prefs[i], prefvalue)
  }
}

function onPopupPrefsOK()
{
  var permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);

  var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-features.xul"].userData;
  if ('deletedPermissions' in dataObject) {
    for (var p = 0; p < dataObject.deletedPermissions.length; ++p) {
      permissionmanager.remove(dataObject.deletedPermissions[p].host, dataObject.deletedPermissions[p].type);
    }
  }
  
  if ('permissions' in dataObject) {
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);    

    for (p = 0; p < dataObject.permissions.length; ++p) {
      uri.spec = dataObject.permissions[p].host;
      if (permissionmanager.testPermission(uri, "popup") != dataObject.permissions[p].perm)
        permissionmanager.add(uri, "popup", nsIPermissionManager.ALLOW_ACTION);
    }
  }
}

function onImagePrefsOK()
{
  var permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(Components.interfaces.nsIPermissionManager);

  var dataObject = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-features-images.xul"].userData;
  if ('deletedPermissions' in dataObject) {
    for (var p = 0; p < dataObject.deletedPermissions.length; ++p) {
      permissionmanager.remove(dataObject.deletedPermissions[p].host, dataObject.deletedPermissions[p].type);
    }
  }
  
  if ('permissions' in dataObject) {
    var uri = Components.classes["@mozilla.org/network/standard-url;1"]
                        .createInstance(Components.interfaces.nsIURI);    

    for (p = 0; p < dataObject.permissions.length; ++p) {
      uri.spec = dataObject.permissions[p].host;
      if (permissionmanager.testPermission(uri, "image") != dataObject.permissions[p].perm)
        permissionmanager.add(uri, "image", nsIPermissionManager.ALLOW_ACTION);
    }
  }
}



