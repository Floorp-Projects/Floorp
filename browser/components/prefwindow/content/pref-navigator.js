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

/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 */

const nsIPrefService    = Components.interfaces.nsIPrefService;
const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;

const PREFSERVICE_CONTRACTID    = "@mozilla.org/preferences-service;1";

function setHomePageToCurrentPage()
{
  if (top.opener) {
    var homePageField = document.getElementById("browserStartupHomepage");
    var newVal = "";

    var browser = top.opener.document.getElementById("content");
    var l = browser.mPanelContainer.childNodes.length;
    for (var i = 0; i < l; i++) {
      if (i)
        newVal += "|";
      newVal += browser.mPanelContainer.childNodes[i].webNavigation.currentURI.spec;
    }
    
    homePageField.value = newVal;
  }
}

function setHomePageToBookmark()
{
  var rv = { url: null };
  openDialog("chrome://browser/content/bookmarks/selectBookmark.xul", "", 
             "centerscreen,chrome,modal=yes,dialog=yes,resizable=yes", rv);
  if (rv.url) {
    var homePageField = document.getElementById("browserStartupHomepage");
    homePageField.value = rv.url;
  }
}

function setHomePageToDefaultPage()
{
  var prefService = Components.classes[PREFSERVICE_CONTRACTID]
                              .getService(nsIPrefService);
  var pref = prefService.getDefaultBranch(null);
  var url = pref.getComplexValue("browser.startup.homepage",
                                  nsIPrefLocalizedString).data;
  var homePageField = document.getElementById("browserStartupHomepage");
  homePageField.value = url;
}

function onOK() {
  if (!('homepage' in parent)) return;

  // Replace pipes with commas to look nicer.
  parent.homepage = parent.homepage.replace(/\|/g,', ');
  
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
  var eb = windowManagerInterface.getEnumerator("navigator:browser");
  while (eb.hasMoreElements()) {
    // Update the home button tooltip.
    var domWin = eb.getNext().QueryInterface(Components.interfaces.nsIDOMWindow);
    domWin.document.getElementById("home-button").setAttribute("tooltiptext", parent.homepage);
  }
}

// check download directory is valid
function checkDownloadDirectory() {
   var dloadDir = Components.classes["@mozilla.org/file/local;1"]
                            .createInstance(Components.interfaces.nsILocalFile);
   if (!dloadDir) 
     return false;

   var givenValue = document.getElementById("defaultDir");
   var downloadDir = document.getElementById("downloadDir");
   if (downloadDir.selectedItem == document.getElementById("alwaysAskRadio"))
     return;
   
   try {
     dloadDir.initWithPath(givenValue.value);
     dloadDir.isDirectory();
   }
   catch(ex) {
     var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                   .getService(Components.interfaces.nsIPromptService);
     var prefbundle = document.getElementById("bundle_prefutilities");

     if (givenValue.value == "") { 
       // no directory, reset back to Always Ask
       downloadDir.selectedItem = document.getElementById("alwaysAskRadio");
     } else {
       var checkValue = {value:false};

       var title = prefbundle.getString("downloadDirTitle");
       var description = prefbundle.getFormattedString("invalidDirPopup", [givenValue.value]);
       var buttonPressed = promptService.confirmEx(window, 
         title, description,
         (promptService.BUTTON_TITLE_YES * promptService.BUTTON_POS_0) +
         (promptService.BUTTON_TITLE_NO * promptService.BUTTON_POS_1),
         null, null, null, null, checkValue);

       if (buttonPressed != 0) {
         // they don't want to create the directory
         return;
       }
       try { 
         dloadDir.create(nsIFile.DIRECTORY_TYPE, 0755);
       } catch(ex) {
         title = prefbundle.getString("invalidDirPopupTitle");
         description = prefbundle.getFormattedString("invalidDir", [givenValue.value])
         promptService.alert(parent, title, description);
       }
     }
  }
}

function Startup()
{  
  if (top.opener) {
    var browser = top.opener.document.getElementById("content");
    var l = browser.mPanelContainer.childNodes.length;

    if (l > 1) {
      var useButton = document.getElementById("browserUseCurrent");
      useButton.label = useButton.getAttribute("label2");
    }
  }
  parent.hPrefWindow.registerOKCallbackFunc(onOK);
}
      
function showConnections()
{
  openDialog("chrome://browser/content/pref/pref-connection.xul", "", "centerscreen,chrome,modal=yes,dialog=yes");
}

