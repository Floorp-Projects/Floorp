# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
#
# Contributor(s):

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
   
  window.openDialog("chrome://global/content/customizeToolbar.xul", "CustomizeToolbar",
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

// this method is overridden by mail-offline.js if we build with the offline extensions
function CheckOnline()
{
  return true; 
}

function openOptionsDialog(containerID, paneURL, itemID)
{
  //check for an existing pref window and focus it; it's not application modal
  const kWindowMediatorContractID = "@mozilla.org/appshell/window-mediator;1";
  const kWindowMediatorIID = Components.interfaces.nsIWindowMediator;
  const kWindowMediator = Components.classes[kWindowMediatorContractID].getService(kWindowMediatorIID);
  var lastPrefWindow = kWindowMediator.getMostRecentWindow("Mail:Options");
  
  if (lastPrefWindow)
    lastPrefWindow.focus();
  else 
    openDialog("chrome://communicator/content/pref/pref.xul","PrefWindow", 
               "chrome,titlebar,resizable,modal", paneURL, containerID, itemID);
}

function openExtensions(aOpenMode)
{
  const EMTYPE = "Extension:Manager";
  
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var needToOpen = true;
  var windowType = EMTYPE + "-" + aOpenMode;
  var windows = wm.getEnumerator(windowType);
  while (windows.hasMoreElements()) {
    var theEM = windows.getNext().QueryInterface(Components.interfaces.nsIDOMWindowInternal);
    if (theEM.document.documentElement.getAttribute("windowtype") == windowType) {
      theEM.focus();
      needToOpen = false;
      break;
    }
  }

  if (needToOpen) {
    const EMURL = "chrome://mozapps/content/extensions/extensions.xul?type=" + aOpenMode;
    const EMFEATURES = "chrome,dialog=no,resizable";
    window.openDialog(EMURL, "", EMFEATURES);
  }
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

// Macintosh window menu functions

#ifdef XP_MACOSX
const nsIWindowDataSource = Components.interfaces.nsIWindowDataSource;

function checkFocusedWindow()
{
  var windowManagerDS = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService(nsIWindowDataSource);

  var sep = document.getElementById("sep-window-list");
  // Using double parens to avoid warning
  while ((sep = sep.nextSibling)) {
    var url = sep.getAttribute('id');
    var win = windowManagerDS.getWindowForResource(url);
    if (win == window) {
      sep.setAttribute("checked", "true");
      break;
    }
  }
}

function toOpenWindow( aWindow )
{
  aWindow.document.commandDispatcher.focusedWindow.focus();
}

function ShowWindowFromResource( node )
{
    var windowManagerDS = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService(nsIWindowDataSource);
    
    var desiredWindow = null;
    var url = node.getAttribute('id');
    desiredWindow = windowManagerDS.getWindowForResource( url );
    if ( desiredWindow )
    {
            toOpenWindow(desiredWindow);
    }
}
#endif
