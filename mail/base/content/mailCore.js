# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
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

/*
 * Core mail routines used by all of the major mail windows (address book, 3-pane, compose and stand alone message window).
 * Routines to support custom toolbars in mail windows, opening up a new window of a particular type all live here. 
 * Before adding to this file, ask yourself, is this a JS routine that is going to be used by all of the main mail windows?
 */

function CustomizeMailToolbar(id)
{
  // Disable the toolbar context menu items
  var menubar = document.getElementById("mail-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", true);
    
  var customizePopup = document.getElementById("CustomizeMailToolbar"); 
  customizePopup.setAttribute("disabled", "true");

  var wintype = document.documentElement.getAttribute("windowtype");
  wintype = wintype.replace(/:/g, "");

  window.openDialog("chrome://global/content/customizeToolbar.xul",
                    "CustomizeToolbar"+wintype,
                    "chrome,all,dependent", document.getElementById(id));
}

function MailToolboxCustomizeDone(aToolboxChanged)
{
  // Update global UI elements that may have been added or removed

  // Re-enable parts of the UI we disabled during the dialog
  var menubar = document.getElementById("mail-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", false);
  
  // Update (or create) "File" button's tree
  if (document.getElementById("button-file"))
    SetupMoveCopyMenus('button-file', accountManagerDataSource, folderDataSource);

  // make sure the mail views search box is initialized
  if (document.getElementById("mailviews-container"))
    ViewPickerOnLoad();
    
  // make sure the folder location picker is initialized
  if (document.getElementById("folder-location-container"))
  {
    loadFolderViewForTree(gCurrentFolderView, document.getElementById('folderLocationPopup').tree);
    UpdateFolderLocationPicker(gMsgFolderSelected);
  }
 
  gSearchInput = null;
  if (document.getElementById("search-container"))
    GetSearchInput();

  var customizePopup = document.getElementById("CustomizeMailToolbar");
  customizePopup.removeAttribute("disabled");

  // make sure our toolbar buttons have the correct enabled state restored to them...
  if (this.UpdateMailToolbar != undefined)
    UpdateMailToolbar(focus); 
}

function onViewToolbarCommand(aToolbarId, aMenuItemId)
{
  var toolbar = document.getElementById(aToolbarId);
  var menuItem = document.getElementById(aMenuItemId);

  if (!toolbar || !menuItem) return;

  var toolbarCollapsed = toolbar.collapsed;
  
  // toggle the checkbox
  menuItem.setAttribute('checked', toolbarCollapsed);
  
  // toggle visibility of the toolbar
  toolbar.collapsed = !toolbarCollapsed;   

  document.persist(aToolbarId, 'collapsed');
  document.persist(aMenuItemId, 'checked');
}

function toJavaScriptConsole()
{
    toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

const nsIWindowMediator = Components.interfaces.nsIWindowMediator;

function toOpenWindowByType( inType, uri )
{
	var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();

	var	windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);

	var topWindow = windowManagerInterface.getMostRecentWindow( inType );
	
	if ( topWindow )
		topWindow.focus();
	else
		window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}

function toMessengerWindow()
{
  toOpenWindowByType("mail:3pane", "chrome://messenger/content/messenger.xul");
}
    
function toAddressBook() 
{
  toOpenWindowByType("mail:addressbook", "chrome://messenger/content/addressbook/addressbook.xul");
}

function toImport()
{
  window.openDialog("chrome://messenger/content/importDialog.xul","importDialog","chrome, modal, titlebar", {importType: "addressbook"});
}

// aPaneID
function openOptionsDialog(aPaneID, aTabID)
{
  var prefsService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService).getBranch(null)
  var instantApply = prefsService.getBoolPref("browser.preferences.instantApply");
  var features = "chrome,titlebar,toolbar,centerscreen" + (instantApply ? ",dialog=no" : ",modal");

  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
           .getService(Components.interfaces.nsIWindowMediator);
  
  var win = wm.getMostRecentWindow("Mail:Preferences");
  if (win)
  {
    win.focus();
    if (aPaneID)
    {
      var pane = win.document.getElementById(aPaneID);
      win.document.documentElement.showPane(pane);
      
      // I don't know how to support aTabID for an arbitrary panel when the dialog is already open
      // This is complicated because showPane is asynchronous (it could trigger a dynamic overlay)
      // so our tab element may not be accessible right away...
    }
  }
  else 
    openDialog("chrome://messenger/content/preferences/preferences.xul","Preferences", features, aPaneID, aTabID);
}

function openAddonsMgr()
{
  const EMTYPE = "Extension:Manager";
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var theEM = wm.getMostRecentWindow(EMTYPE);
  if (theEM) {
    theEM.focus();
    return;
  }

  const EMURL = "chrome://mozapps/content/extensions/extensions.xul";
  const EMFEATURES = "chrome,menubar,extra-chrome,toolbar,dialog=no,resizable";
  window.openDialog(EMURL, "", EMFEATURES);
}

function SetBusyCursor(window, enable)
{
    // setCursor() is only available for chrome windows.
    // However one of our frames is the start page which 
    // is a non-chrome window, so check if this window has a
    // setCursor method
    if ("setCursor" in window) {
        if (enable)
            window.setCursor("wait");
        else
            window.setCursor("auto");
    }

	var numFrames = window.frames.length;
	for(var i = 0; i < numFrames; i++)
		SetBusyCursor(window.frames[i], enable);
}

function openAboutDialog()
{
#ifdef XP_MACOSX
  window.open("chrome://messenger/content/aboutDialog.xul", "About", "centerscreen,chrome,resizable=no");
#else
  window.openDialog("chrome://messenger/content/aboutDialog.xul", "About", "modal,centerscreen,chrome,resizable=no");
#endif
}

/**
 * Opens region specific web pages for the application like the release notes, the help site, etc. 
 *   aResourceName --> the string resource ID in region.properties to load. 
 */
function openRegionURL(aResourceName)
{
  var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                          .getService(Components.interfaces.nsIXULAppInfo);
  try {
    var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].getService(Components.interfaces.nsIStringBundleService);
    var regionBundle = strBundleService.createBundle("chrome://messenger-region/locale/region.properties");
    // the release notes are special and need to be formatted with the app version
    var urlToOpen;
    if (aResourceName == "releaseNotesURL")
      urlToOpen = regionBundle.formatStringFromName(aResourceName, [appInfo.version], 1);
    else
      urlToOpen = regionBundle.GetStringFromName(aResourceName);
      
    var uri = Components.classes["@mozilla.org/network/io-service;1"]
              .getService(Components.interfaces.nsIIOService)
              .newURI(urlToOpen, null, null);

    var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                      .getService(Components.interfaces.nsIExternalProtocolService);
    protocolSvc.loadUrl(uri);
  } catch (ex) {}
}

/**
 *  Fetches the url for the passed in pref name, formats it and then loads it in the default
 *  browser.
 *
 *  @param aPrefName - name of the pref that holds the url we want to format and open
 */
function openFormattedRegionURL(aPrefName)
{
  var formattedUrl = getFormattedRegionURL(aPrefName);
  
  var uri = Components.classes["@mozilla.org/network/io-service;1"].
                       getService(Components.interfaces.nsIIOService).
                       newURI(formattedUrl, null, null);

  var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"].
                               getService(Components.interfaces.nsIExternalProtocolService);
  protocolSvc.loadUrl(uri);  
}

/**
 *  Fetches the url for the passed in pref name and uses the URL formatter service to 
 *    process it.
 *
 *  @param aPrefName - name of the pref that holds the url we want to format and open
 *  @returns the formatted url string
 */
function getFormattedRegionURL(aPrefName)
{
  var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"].
                             getService(Components.interfaces.nsIURLFormatter);
  return formatter.formatURLPref(aPrefName);
}
