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

var _elementIDs = ["advancedJavaAllow", "enableSoftwareInstall", 
                   "enableJavaScript", "enableImagePref",
                   "popupPolicy", "allowWindowMoveResize", 
                   "allowWindowFlip", "allowControlContextMenu", 
                   "allowHideStatusBar", "allowWindowStatusChange", 
                   "allowImageSrcChange"];
var gImagesPref, gImagesEnabled, gImagesRestricted;

function Startup()
{
  updateButtons('popup', 'install', 'javascript');
  
  gImagesPref = document.getElementById("enableImagePref");
  gImagesEnabled = document.getElementById("enableImages");
  gImagesRestricted = document.getElementById("enableRestricted");
  var prefValue = gImagesPref.getAttribute("value");
  if (!prefValue)
    prefValue = "0";
  switch (prefValue) {
  case "1": 
    gImagesRestricted.checked = true;
  case "0": 
    gImagesEnabled.checked = true;
    break;
  }
  if (!gImagesEnabled.checked)
    gImagesRestricted.disabled = true;

  if (parent.hPrefWindow.getPrefIsLocked("network.image.imageBehavior")) {  
    gImagesRestricted.disabled = true;
    gImagesEnabled.disabled = true;
  }
}

function updateImagePref()
{
  if (!parent.hPrefWindow.getPrefIsLocked("network.image.imageBehavior")) {
    if (!gImagesEnabled.checked) {
      gImagesPref.setAttribute("value", 2)
      gImagesRestricted.disabled = true;
    } else {
      gImagesPref.setAttribute("value", gImagesRestricted.checked ? 1 : 0)
      gImagesRestricted.disabled = false;
    }
  } else {
    gImagesRestricted.disabled = true;
    gImagesEnabled.disabled = true;
  }
}

function advancedJavaScript()
{
  openDialog("chrome://browser/content/pref/pref-advancedscripts.xul", "", 
             "chrome,modal");
}

function updateButtons()
{
  var i;
  var checkbox;
  var button;

  for (i=0; i < arguments.length; ++i) {
    switch (arguments[i]) {
    case "popup":
      checkbox = document.getElementById("popupPolicy");
      button   = document.getElementById("popupPolicyButton");
      break;
    case "install":
      checkbox = document.getElementById("enableSoftwareInstall");
      button   = document.getElementById("enableSoftwareInstallButton");
      break;
    case "javascript":
      checkbox = document.getElementById("enableJavaScript");
      button   = document.getElementById("advancedJavascript");
      break;
    }
    button.disabled = !checkbox.checked;
  }
}

var gExceptionsParams = {
  install: { blockVisible: false, sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "install" },
  popup:   { blockVisible: false, sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "popup"   },
  image:   { blockVisible: true,  sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "image"   },
};

function showExceptions(aEvent)
{
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var existingWindow = wm.getMostRecentWindow("exceptions");
  if (existingWindow) {
    existingWindow.setHost("");
    existingWindow.focus();
  }
  else {
    const kURL = "chrome://browser/content/cookieviewer/CookieExceptions.xul";
    var params = gExceptionsParams[aEvent.target.getAttribute("permissiontype")];
    window.openDialog(kURL, "_blank", "chrome,modal,resizable=yes", params);
  }
}

