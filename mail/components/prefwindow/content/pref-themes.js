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
#   Ben Goodger <ben@netscape.com> (Original Author)
#   Blake Ross <blaker@netscape.com>
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

const nsIIOService = Components.interfaces.nsIIOService;
const nsIFileProtocolHandler = Components.interfaces.nsIFileProtocolHandler;
const nsIURL = Components.interfaces.nsIURL;

var gData;

try {
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
  if (chromeRegistry)
    chromeRegistry = chromeRegistry.QueryInterface(Components.interfaces.nsIXULChromeRegistry);
}
catch(e) {}

function Startup()
{
  gData = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-themes.xul"].userData;
  var list = document.getElementById( "skinsList" );
  if ("loaded" in gData && "themeIndex" in gData) {
    list.selectedIndex = gData.themeIndex;    
    return;
  }
  gData.loaded = true;
  parent.hPrefWindow.registerOKCallbackFunc( applySkin );

  const kPrefSvcContractID = "@mozilla.org/preferences;1";
  const kPrefSvcIID = Components.interfaces.nsIPref;
  const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);

  var theme = null;
  try {
    theme = kPrefSvc.getComplexValue("general.skins.selectedSkin",
                                     Components.interfaces.nsISupportsString).data;
  } catch (e) {
  }
  var matches;
  for (var i = 0; i < list.childNodes.length; ++i) {
    var child = list.childNodes[i];
    var name = child.getAttribute("name");
    if (name) {
      if (!theme)
        matches = chromeRegistry.isSkinSelected(name, true) == Components.interfaces.nsIChromeRegistry.FULL;
      else
        matches = name == theme;
      if (matches) {
        list.selectItem(child);
        break;
      }
    }      
  }
}

function applySkin()
{
  var data = parent.hPrefWindow.wsm.dataManager.pageData["chrome://communicator/content/pref/pref-themes.xul"].userData;
  if (data.name == null)
    return;

  const kPrefSvcContractID = "@mozilla.org/preferences;1";
  const kPrefSvcIID = Components.interfaces.nsIPref;
  const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);

  var theme = null;
  try {
    theme = kPrefSvc.getComplexValue("general.skins.selectedSkin",
                                     Components.interfaces.nsISupportsString).data;
  } catch (e) {
  }

  if (theme == data.name) return;

  try {
    var reg = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
    if (reg)
      reg = reg.QueryInterface(Components.interfaces.nsIXULChromeRegistry);
  }
  catch(e) {}

  var inUse = reg.isSkinSelected(data.name, true);
  if (!theme && inUse == Components.interfaces.nsIChromeRegistry.FULL) return;

  var str = Components.classes["@mozilla.org/supports-string;1"]
                      .createInstance(Components.interfaces.nsISupportsString);
  str.data = data.name;

  kPrefSvc.setComplexValue("general.skins.selectedSkin", Components.interfaces.nsISupportsString, str);

  // shut down quicklaunch so the next launch will have the new skin
  var appShell = Components.classes['@mozilla.org/appshell/appShellService;1'].getService();
  appShell = appShell.QueryInterface(Components.interfaces.nsIAppShellService);
  try {
    appShell.nativeAppSupport.isServerMode = false;
  }
  catch(ex) {
  }

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);

  var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].getService(); 
  strBundleService = strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
  var navbundle = strBundleService.createBundle("chrome://navigator/locale/navigator.properties"); 
  var brandbundle = strBundleService.createBundle("chrome://global/locale/brand.properties");
  
  if (promptService && navbundle && brandbundle) {                                                          
    var dialogTitle = navbundle.GetStringFromName("switchskinstitle");          
    var brandName = brandbundle.GetStringFromName("brandShortName");            
    var msg = navbundle.formatStringFromName("switchskins", [brandName], 1);                                
    promptService.alert(window, dialogTitle, msg); 
  }
}

function uninstallSkin()
{
  var list = document.getElementById("skinsList");
  var selectedSkinItem = list.selectedItems[0];
  var skinName = selectedSkinItem.getAttribute("name");
  var inUse = chromeRegistry.isSkinSelected(skinName, true);
  chromeRegistry.uninstallSkin(skinName, true);
  if (inUse)
    chromeRegistry.refreshSkins();
  list.selectedIndex = 0;
}

function themeSelect()
{
  var list = document.getElementById("skinsList");

  if (!list)
    return;

  var prefbundle = document.getElementById("bundle_prefutilities");

  var selectedItem = list.selectedItems.length ? list.selectedItems[0] : null;
  if (selectedItem && selectedItem.getAttribute("skin") == "true") {
    var themeName = selectedItem.getAttribute("displayName");
    var skinName = selectedItem.getAttribute("name");
    gData.name = skinName;
    gData.themeIndex = list.selectedIndex;

    var oldTheme;
    try {
      oldTheme = !chromeRegistry.checkThemeVersion(skinName);
    }
    catch(e) {
      oldTheme = false;
    }

    var nameField = document.getElementById("displayName");
    var author = document.getElementById("author");
    var authorLabel = document.getElementById("authorLabel");
    var image = document.getElementById("previewImage");
    var uninstallButton = document.getElementById("uninstallSkin");
    var uninstallLabel = prefbundle.getString("uninstallThemePrefix");

    nameField.setAttribute("value", themeName);
    author.setAttribute("value", selectedItem.getAttribute("author"));
    authorLabel.removeAttribute("collapsed");
    image.setAttribute("src", selectedItem.getAttribute("image"));

    // XXX - this sucks and should only be temporary.
    var selectedSkin = "";
    try {
      const kPrefSvcContractID = "@mozilla.org/preferences;1";
      const kPrefSvcIID = Components.interfaces.nsIPref;
      const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
      selectedSkin = kPrefSvc.CopyCharPref("general.skins.selectedSkin");
    }
    catch (e) {
    }
    if (!oldTheme) {    

      var locType = selectedItem.getAttribute("loctype");
      uninstallButton.disabled = (selectedSkin == skinName) || (locType == "install");
      
      uninstallLabel = uninstallLabel.replace(/%theme_name%/, themeName);
      uninstallButton.label = uninstallLabel;
    }
    else {
      var brandbundle = document.getElementById("bundle_brand");

      uninstallLabel = uninstallLabel.replace(/%theme_name%/, themeName);
      uninstallButton.label = uninstallLabel;

      uninstallButton.disabled = selectedSkin == skinName;

      var newText = prefbundle.getString("oldTheme");
      newText = newText.replace(/%theme_name%/, themeName);
      
      newText = newText.replace(/%brand%/g, brandbundle.getString("brandShortName"));

    }
  }
  else {
    uninstallButton.disabled = true;
    gData.name = null;
  }
}

///////////////////////////////////////////////////////////////
// functions to support installing of themes in thunderbird
///////////////////////////////////////////////////////////////
const nsIFilePicker = Components.interfaces.nsIFilePicker;

function installSkin()
{
  // 1) Prompt the user for the location of the theme to install. Eventually we'll support web locations too.
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, document.getElementById("installSkin").getAttribute("filepickertitle"), nsIFilePicker.modeOpen);

  var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].getService();
  strBundleService = strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);

  var extbundle = strBundleService.createBundle("chrome://communicator/locale/pref/prefutilities.properties");
  var themesFilter = extbundle.GetStringFromName("themesFilter");
  fp.appendFilter(themesFilter, "*.jar");
  fp.appendFilters(nsIFilePicker.filterAll);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) 
  {
    var ioService =
    Components.classes['@mozilla.org/network/io-service;1'].getService(nsIIOService);
    var fileProtocolHandler =
    ioService.getProtocolHandler("file").QueryInterface(nsIFileProtocolHandler);
    var url = fileProtocolHandler.newFileURI(fp.file).QueryInterface(nsIURL);
    InstallTrigger.installChrome(InstallTrigger.SKIN, url.spec, decodeURIComponent(url.fileBaseName));
  }
}



