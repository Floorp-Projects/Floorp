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
  var host = enterNewSite(null);
  if (!host)
    return;
  permissions[permissions.length] = new Permission(permissions.length, host,
                                                   (host.charAt(0)==".") ? host.substring(1,host.length) : host,
                                                   "popup",
                                                   "");
  permissionsTreeView.rowCount = permissions.length;
  permissionsTree.treeBoxObject.rowCountChanged(permissions.length-1, 1);
  permissionsTree.treeBoxObject.ensureRowIsVisible(permissions.length-1);
}

function enterNewSite(site) {
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);

  var stringBundle = document.getElementById("stringBundle");
  var message = stringBundle.getString("enterSiteName");
  var title = stringBundle.getString("enterSiteTitle");

  var name = (!site ? {} : site);
  if (!promptService.prompt(window, title, message, name, null, {}))
      return null;
  var host = name.value.replace(/^\s*([-\w]*:\/+)?/, ""); // trim any leading space and scheme
  try {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    var uri = ioService.newURI("http://"+host, null, null);
    host = uri.host;
    return host;
  } catch(ex) {
    // if we get here, the user has managed to enter something that couldn't get parsed to a real URI
    var alertTitle = stringBundle.getString("addSiteFailedTitle");
    var alertMsg = stringBundle.getFormattedString("addSiteFailedMessage",[host]);
    promptService.alert(window, alertTitle, alertMsg);
  }
  return null;
}

function onPopupPrefsOK()
{
  if (!nsIPermissionManager)
    nsIPermissionManager = Components.interfaces.nsIPermissionManager;

  var permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(nsIPermissionManager);

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
  if (!nsIPermissionManager)
    nsIPermissionManager = Components.interfaces.nsIPermissionManager;

  var permissionmanager = Components.classes["@mozilla.org/permissionmanager;1"].getService();
  permissionmanager = permissionmanager.QueryInterface(nsIPermissionManager);

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



