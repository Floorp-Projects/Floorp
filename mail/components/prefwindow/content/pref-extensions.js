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

const nsIIOService = Components.interfaces.nsIIOService;
const nsIFileProtocolHandler = Components.interfaces.nsIFileProtocolHandler;
const nsIURL = Components.interfaces.nsIURL;

try {
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
  if (chromeRegistry)
    chromeRegistry = chromeRegistry.QueryInterface(Components.interfaces.nsIXULChromeRegistry);
}
catch(e) {}

function Startup()
{
  var extList = document.getElementById("extList");
  for (var i = 0; i < extList.childNodes.length; ++i) {
    if (extList.childNodes[i].getAttribute("name")) {
      extList.selectItem(extList.childNodes[i]);
      break;
    }
  }
}

function extensionSelect()
{
  var list = document.getElementById("extList");

  if (!list)
    return;

  var selectedItem = list.selectedItems.length ? list.selectedItems[0] : null;
  if (selectedItem) {
    var extName = selectedItem.getAttribute("displayName");
    var nameField = document.getElementById("extDisplayName");
    var author = document.getElementById("extAuthor");
    var authorLabel = document.getElementById("authorLabel");
    var descText = document.createTextNode(selectedItem.getAttribute("description"));
    var description = document.getElementById("extDescription");
    var descriptionLabel = document.getElementById("descriptionLabel");
    var uninstallButton = document.getElementById("uninstallExtension");
    var settingsButton = document.getElementById("extensionSettings");
    
    while (description.hasChildNodes())
      description.removeChild(description.firstChild);

    nameField.setAttribute("value", extName);
    
    author.setAttribute("value", selectedItem.getAttribute("author"));
    authorLabel.removeAttribute("collapsed");

    var authorURL = selectedItem.getAttribute("authorURL");
    if (authorURL != "") {
      author.setAttribute("link", selectedItem.getAttribute("authorURL"));
      author.className = "themesLink";
    }
    else {
      author.removeAttribute("link");
      author.className = "";
    }
    
    settingsButton.disabled = selectedItem.getAttribute("settingsURL") == "";
    
    description.appendChild(descText);
    descriptionLabel.removeAttribute("collapsed");

    updateDisableExtButton(selectedItem);
  }
}

function loadAuthorUrl()
{
  // XXX FIX ME!!!! total hack to abuse messenger this way..but it is our cheap way of launching a url
  // the way we want it to. 
  var authorURL = document.getElementById("extAuthor").getAttribute("link");
  if (authorURL != "")
  {
    var messenger = Components.classes["@mozilla.org/messenger;1"].createInstance();
    messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
    messenger.launchExternalURL(authorURL);  
  }
}

function toggleExtension()
{
  var list = document.getElementById("extList");

  if (!list)
    return;

  var selectedItem = list.selectedItems.length ? list.selectedItems[0] : null;
  if (selectedItem) {
    var disabled = (selectedItem.getAttribute("disabledState") == "true");
    chromeRegistry.setAllowOverlaysForPackage(selectedItem.getAttribute("name"), disabled);
    updateDisableExtButton(selectedItem);
  }   
}

function updateDisableExtButton(item)
{
  var disableButton = document.getElementById("disableExtension");
  if (disableButton.disabled)
    disableButton.disabled = false;

  var prefBundle = document.getElementById("bundle_prefutilities");
  var enableExtension = prefBundle.getString("enableExtension");
  var disableExtension = prefBundle.getString("disableExtension");

  if (item.getAttribute("disabledState") == "true")
    disableButton.setAttribute("label", enableExtension);
  else
    disableButton.setAttribute("label", disableExtension);
}

function showSettings()
{
  var list = document.getElementById("extList");
  var selectedItem = list.selectedItems.length ? list.selectedItems[0] : null;
  if (selectedItem)
    window.openDialog(selectedItem.getAttribute("settingsURL"), "", "chrome,dialog,modal,resizable");
}

///////////////////////////////////////////////////////////////
// functions to support installing of .xpis in thunderbird
///////////////////////////////////////////////////////////////
const nsIFilePicker = Components.interfaces.nsIFilePicker;

function installExtension()
{
  // 1) Prompt the user for the location of the theme to install. Eventually we'll support web locations too.
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, document.getElementById("installExtensions").getAttribute("filepickertitle"), nsIFilePicker.modeOpen);
  
  var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].getService();
  strBundleService = strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);

  var extbundle = strBundleService.createBundle("chrome://communicator/locale/pref/prefutilities.properties");
  var filterLabel = extbundle.GetStringFromName("extensionFilter");
  fp.appendFilter(filterLabel, "*.xpi");
  
  fp.appendFilters(nsIFilePicker.filterAll);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) 
  {
    var ioService = Components.classes['@mozilla.org/network/io-service;1'].getService(nsIIOService);
    var fileProtocolHandler =
    ioService.getProtocolHandler("file").QueryInterface(nsIFileProtocolHandler);
    var url = fileProtocolHandler.newFileURI(fp.file).QueryInterface(nsIURL);
    var xpi = {};
    xpi[decodeURIComponent(url.fileBaseName)] = url.spec;
    InstallTrigger.install(xpi);
  }
}



