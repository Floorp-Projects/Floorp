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

var gShowDescription = true;
var gData;
const kPrefSvcContractID = "@mozilla.org/preferences;1";
const kPrefSvcIID = Components.interfaces.nsIPref;
const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);

try {
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
  if (chromeRegistry)
    chromeRegistry = chromeRegistry.QueryInterface(Components.interfaces.nsIXULChromeRegistry);
}
catch(e) {}

function Startup()
{
  gData = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-themes.xul"];
  var list = document.getElementById( "skinsList" );
  if ("loaded" in gData && "themeIndex" in gData) {
    list.selectedIndex = gData.themeIndex;    
    return;
  }
  gData.loaded = true;
  parent.hPrefWindow.registerOKCallbackFunc(applyTheme);

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
    if (!theme)
      matches = chromeRegistry.isSkinSelectedForPackage(name, "browser", true);
    else
      matches = name == theme;
    if (matches) {
      list.selectItem(child);
      break;
    }      
  }

  var navbundle = document.getElementById("bundle_navigator");
  var showSkinsDescription = navbundle.getString("showskinsdescription");
  if (showSkinsDescription == "false")
  {
    gShowDescription = false;
    var description = document.getElementById("description");
    while (description.hasChildNodes())
      description.removeChild(description.firstChild);
  }
}

function applyTheme()
{
  var data = parent.hPrefWindow.wsm.dataManager.pageData["chrome://browser/content/pref/pref-themes.xul"];
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

  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Components.interfaces.nsIXULChromeRegistry);

  var oldTheme = false;
  try {
    oldTheme = !chromeRegistry.checkThemeVersion(data.name);
  }
  catch(e) {
  }


  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  if (oldTheme) {
    var title = gNavigatorBundle.getString("oldthemetitle");
    var message = gNavigatorBundle.getString("oldTheme");

    message = message.replace(/%theme_name%/, themeName.getAttribute("displayName"));
    message = message.replace(/%brand%/g, gBrandBundle.getString("brandShortName"));

    if (promptService.confirm(window, title, message)){
      var inUse = chromeRegistry.isSkinSelected(data.name, true);

      chromeRegistry.uninstallSkin(data.name, true);

      var str = Components.classes["@mozilla.org/supports-string;1"]
                          .createInstance(Components.interfaces.nsISupportsString);

      str.data = true;
      pref.setComplexValue("general.skins.removelist." + data.name,
                           Components.interfaces.nsISupportsString, str);
      
      if (inUse)
        chromeRegistry.refreshSkins();
    }

    return;
  }

  var str = Components.classes["@mozilla.org/supports-string;1"]
                      .createInstance(Components.interfaces.nsISupportsString);
  str.data = data.name;
  kPrefSvc.setComplexValue("general.skins.selectedSkin", Components.interfaces.nsISupportsString, str);


  chromeRegistry.selectSkin(data.name, true);                                        
  chromeRegistry.refreshSkins();
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
    var image = document.getElementById("previewImage");
    var descText = document.createTextNode(selectedItem.getAttribute("description"));
    var description = document.getElementById("description");
    var uninstallButton = document.getElementById("uninstallSkin");
    var uninstallLabel = prefbundle.getString("uninstallThemePrefix");

    while (description.hasChildNodes())
      description.removeChild(description.firstChild);

    nameField.setAttribute("value", themeName);
    
    author.setAttribute("value", selectedItem.getAttribute("author"));
    var authorURL = selectedItem.getAttribute("authorURL");
    if (authorURL != "") {
      author.setAttribute("link", selectedItem.getAttribute("authorURL"));
      author.className = "themesLink";
    }
    else {
      author.removeAttribute("link");
      author.className = "";
    }
        
    image.setAttribute("src", selectedItem.getAttribute("image"));

    // XXX - this sucks and should only be temporary.
    var selectedSkin = "";
    try {
      selectedSkin = kPrefSvc.CopyCharPref("general.skins.selectedSkin");
    }
    catch (e) {
    }
    if (!oldTheme) {    
      if( gShowDescription ) 
        description.appendChild(descText);

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

      if( gShowDescription )  {
        descText = document.createTextNode(newText);
        description.appendChild(descText);
      }
    }
  }
  else {
    uninstallButton.disabled = true;
    gData.name = null;
  }
}

function visitLink (aEvent, aAuthor)
{
  var msg = "";
  if (aAuthor)
    msg = "prefCloseThemeAuthorLinkMsg";
  else
    msg = "prefCloseThemeNewThemeLinkMsg";

  var node = aEvent.target;
  while (node.nodeType != Node.ELEMENT_NODE)
    node = node.parentNode;

  var url = node.getAttribute("link");
  if (url != "")
    parent.visitLink("prefCloseThemeLinkTitle", msg, url);
}
