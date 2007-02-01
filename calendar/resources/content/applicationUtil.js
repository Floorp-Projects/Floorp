/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Calendar code.
 *
 * The Initial Developer of the Original Code is Eric Belhaire.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Matthew Willis <mattwillis@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


const nsIWindowMediator = Components.interfaces.nsIWindowMediator;

// "About Sunbird" dialog
function openAboutDialog()
{
  var url = "chrome://calendar/content/aboutDialog.xul";
  var name = "About";

#ifdef XP_MACOSX
  // Define minimizable=no although it does nothing on OS X (bug 287162).
  // Remove this comment once bug 287162 is fixed
  window.open(url, name, "centerscreen,chrome,resizable=no,minimizable=no");
#else
  window.openDialog(url, name, "modal,centerscreen,chrome,resizable=no");
#endif
}

function toOpenWindowByType(inType, uri)
{
    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
    var windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);
    var topWindow = windowManagerInterface.getMostRecentWindow(inType);

    if (topWindow)
        topWindow.focus();
    else
        window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}

function toBrowser()
{
    toOpenWindowByType("navigator:browser", "");
}

function toJavaScriptConsole()
{
    toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

#ifndef MOZ_SUNBIRD
function toMessengerWindow()
{
    toOpenWindowByType("mail:3pane", "chrome://messenger/content/messenger.xul");
}
    
function toAddressBook() 
{
    toOpenWindowByType("mail:addressbook", "chrome://messenger/content/addressbook/addressbook.xul");
}
#endif

function launchBrowser(UrlToGoTo)
{
  if (!UrlToGoTo) {
    return;
  }

  // 0. Prevent people from trying to launch URLs such as javascript:foo();
  //    by only allowing URLs starting with http or https.
  // XXX: We likely will want to do this using nsIURLs in the future to
  //      prevent sneaky nasty escaping issues, but this is fine for now.
  if (UrlToGoTo.indexOf("http") != 0) {
    Components.utils.reportError ("launchBrowser: " +
                                  "Invalid URL provided: " + UrlToGoTo +
                                  " Only http:// and https:// URLs are valid.");
    return;
  }

  // 1. try to get (most recent) browser window, in case in browser app.
  var navWindow;
  try {
    var wm = (Components
              .classes["@mozilla.org/appshell/window-mediator;1"]
              .getService(Components.interfaces.nsIWindowMediator));
    navWindow = wm.getMostRecentWindow("navigator:browser");
  } catch (e) {
    dump("launchBrowser (getMostRecentWindow) exception:\n" + e + "\n");
  }
  if (navWindow) {
    if ("delayedOpenTab" in navWindow)
      navWindow.delayedOpenTab(UrlToGoTo);
    else if ("loadURI" in navWindow)
      navWindow.loadURI(UrlToGoTo);
    else
      navWindow.content.location.href = UrlToGoTo;
    return;
  }

  // 2. try a new browser window, in case in suite (seamonkey)
  var messenger;
  try {
    var messenger = (Components
                     .classes["@mozilla.org/messenger;1"]
                     .createInstance());
    messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
  } catch (e) {
    dump("launchBrowser (messenger) exception:\n"+e+"\n");
  }
  if (messenger) {
    messenger.launchExternalURL(UrlToGoTo);  
    return;
  } 

  // 3. try an external app, in case not in a browser app (SB, TB, etc).
  var externalLoader =
    (Components
     .classes["@mozilla.org/uriloader/external-protocol-service;1"]
     .getService(Components.interfaces.nsIExternalProtocolService));
  var nsURI = (Components
               .classes["@mozilla.org/network/io-service;1"]
               .getService(Components.interfaces.nsIIOService)
               .newURI(UrlToGoTo, null, null));
  externalLoader.loadUrl(nsURI);
}


function goToggleToolbar(id, elementID)
{
    var toolbar = document.getElementById(id);
    var element = document.getElementById(elementID);
    if (toolbar) {
        var isHidden = toolbar.hidden;
        toolbar.hidden = !isHidden;
        document.persist(id, 'hidden');
        if (element) {
            element.setAttribute("checked", isHidden ? "true" : "false");
            document.persist(elementID, 'checked');
        }
    }
}


function goOpenAddons()
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


function showElement(elementId)
{
    try {
        document.getElementById(elementId).removeAttribute("hidden");
    } catch (e) {
        dump("showElement: Couldn't remove hidden attribute from " + elementId + "\n");
    }
}


function hideElement(elementId)
{
    try {
        document.getElementById(elementId).setAttribute("hidden", "true");
    } catch (e) {
        dump("hideElement: Couldn't set hidden attribute on " + elementId + "\n");
    }
}


function enableElement(elementId)
{
    try {
        //document.getElementById(elementId).setAttribute("disabled", "false");

        // call remove attribute beacuse some widget code checks for the presense of a 
        // disabled attribute, not the value.
        document.getElementById(elementId).removeAttribute("disabled");
    } catch (e) {
        dump("enableElement: Couldn't remove disabled attribute on " + elementId + "\n");
    }
}


function disableElement(elementId)
{
    try {
        document.getElementById(elementId).setAttribute( "disabled", "true");
    } catch (e) {
        dump("disableElement: Couldn't set disabled attribute to true on " +
             elementId + "\n");
    }
}


/**
*   Helper function for filling the form,
*   Set the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to set
*      newValue      - value to set property to ( if undefined no change is made )
*      propertyName  - OPTIONAL name of property to set, default is "value",
*                      use "checked" for radios & checkboxes, "data" for
*                      drop-downs
*/
function setElementValue(elementId, newValue, propertyName)
{
    var undefined;

    if (newValue !== undefined) {
        var field = document.getElementById(elementId);

        if (newValue === false) {
            try {
                field.removeAttribute(propertyName);
            } catch (e) {
                dump("setFieldValue: field.removeAttribute couldn't remove " +
                propertyName + " from " + elementId + " e: " + e + "\n");
            }
        } else if (propertyName) {
            try {
                field.setAttribute(propertyName, newValue);
            } catch (e) {
                dump("setFieldValue: field.setAttribute couldn't set " +
                propertyName + " from " + elementId + " to " + newValue +
                " e: " + e + "\n");
            }
        } else {
            field.value = newValue;
        }
    }
}


/**
*   Helper function for getting data from the form,
*   Get the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to get from
*      propertyName  - OPTIONAL name of property to set, default is "value",
*                      use "checked" for radios & checkboxes, "data" for
*                      drop-downs
*   RETURN
*      newValue      - value of property
*/
function getElementValue(elementId, propertyName)
{
    var field = document.getElementById(elementId);

    if (propertyName)
        return field[propertyName];
    return field.value;
}


function processEnableCheckbox(checkboxId, elementId)
{
    if (document.getElementById(checkboxId).checked)
        enableElement(elementId);
    else
        disableElement(elementId);
}


/*
 *  Enable/disable button if there are children in a listbox
 */
function updateListboxDeleteButton(listboxId, buttonId)
{
    if ( document.getElementById(listboxId).getRowCount() > 0 )
        enableElement(buttonId);
    else
        disableElement(buttonId);
}


/*
 *  Update plural singular menu items
 */
function updateMenuLabels(lengthFieldId, menuId )
{
    var field = document.getElementById(lengthFieldId);
    var menu  = document.getElementById(menuId);

    // figure out whether we should use singular or plural
    var length = field.value;

    var newLabelNumber;

    // XXX This assumes that "0 days, minutes, etc." is plural in other languages.
    if ( ( Number(length) == 0 ) || ( Number(length) > 1 ) )
        newLabelNumber = "label2"
    else
        newLabelNumber = "label1"

    // see what we currently show and change it if required
    var oldLabelNumber = menu.getAttribute("labelnumber");

    if ( newLabelNumber != oldLabelNumber ) {
        // remember what we are showing now
        menu.setAttribute("labelnumber", newLabelNumber);

        // update the menu items
        var items = menu.getElementsByTagName("menuitem");

        for( var i = 0; i < items.length; ++i ) {
            var menuItem = items[i];
            var newLabel = menuItem.getAttribute(newLabelNumber);
            menuItem.label = newLabel;
            menuItem.setAttribute("label", newLabel);
        }

        // force the menu selection to redraw
        var saveSelectedIndex = menu.selectedIndex;
        menu.selectedIndex = -1;
        menu.selectedIndex = saveSelectedIndex;
    }
}


/** Select value in menuList.  Throws string if no such value. **/

function menuListSelectItem(menuListId, value)
{
    var menuList = document.getElementById(menuListId);
    var index = menuListIndexOf(menuList, value);
    if (index != -1) {
        menuList.selectedIndex = index;
    } else {
        throw "menuListSelectItem: No such Element: "+value;
    }
}


/** Find index of menuitem with the given value, or return -1 if not found. **/

function menuListIndexOf(menuList, value)
{
    var items = menuList.menupopup.childNodes;
    var index = -1;
    for ( var i = 0; i < items.length; i++ ) {
        var element = items[i];
        if (element.nodeName == "menuitem")
            index++;
        if (element.getAttribute("value") == value)
            return index;
    }
    return -1; // not found
}

function radioGroupSelectItem(radioGroupId, id)
{
    var radioGroup = document.getElementById(radioGroupId);
    var index = radioGroupIndexOf(radioGroup, id);
    if (index != -1) {
        radioGroup.selectedIndex = index;
    } else {
        throw "radioGroupSelectItem: No such Element: "+id;
    }
}

function radioGroupIndexOf(radioGroup, id)
{
    var items = radioGroup.getElementsByTagName("radio");
    var i;
    for (i in items) {
        if (items[i].getAttribute("id") == id)
            return i;
    }
    return -1; // not found
}

function hasPositiveIntegerValue(elementId)
{
    var value = document.getElementById(elementId).value;
    if (value && (parseInt(value) == value) && value > 0)
        return true;
    return false;
}

/**
 * We recreate the View > Toolbars menu each time it is opened to include any
 * user-created toolbars.
 */
function sbOnViewToolbarsPopupShowing(aEvent)
{
    var popup = aEvent.target;
    var i;

    // Empty the menu
    for (i = popup.childNodes.length-1; i >= 0; i--) {
        var deadItem = popup.childNodes[i];
        if (deadItem.hasAttribute("toolbarindex")) {
            deadItem.removeEventListener("command", sbOnViewToolbarCommand, false);
            popup.removeChild(deadItem);
        }
    }

    var firstMenuItem = popup.firstChild;

    var toolbox = document.getElementById("calendar-toolbox");
    for (i = 0; i < toolbox.childNodes.length; i++) {
        var toolbar = toolbox.childNodes[i];
        var toolbarName = toolbar.getAttribute("toolbarname");
        var type = toolbar.getAttribute("type");
        if (toolbarName && type != "menubar") {
            var menuItem = document.createElement("menuitem");
            menuItem.setAttribute("toolbarindex", i);
            menuItem.setAttribute("type", "checkbox");
            menuItem.setAttribute("label", toolbarName);
            menuItem.setAttribute("accesskey", toolbar.getAttribute("accesskey"));
            menuItem.setAttribute("checked", toolbar.getAttribute("collapsed") != "true");
            popup.insertBefore(menuItem, firstMenuItem);

            menuItem.addEventListener("command", sbOnViewToolbarCommand, false);
        }
    }
}

/**
 * Toggles the visibility of the associated toolbar when fired.
 */
function sbOnViewToolbarCommand(aEvent)
{
    var toolbox = document.getElementById("calendar-toolbox");
    var index = aEvent.originalTarget.getAttribute("toolbarindex");
    var toolbar = toolbox.childNodes[index];

    toolbar.collapsed = (aEvent.originalTarget.getAttribute("checked") != "true");
    document.persist(toolbar.id, "collapsed");
}

/**
 * Checks for available updates using AUS
 */
function sbCheckForUpdates()
{
    var um = Components.classes["@mozilla.org/updates/update-manager;1"]
                       .getService(Components.interfaces.nsIUpdateManager);
    var prompter = Components.classes["@mozilla.org/updates/update-prompt;1"]
                             .createInstance(Components.interfaces.nsIUpdatePrompt);

    // If there's an update ready to be applied, show the "Update Downloaded"
    // UI instead and let the user know they have to restart the application
    // for the changes to be applied.
    if (um.activeUpdate && um.activeUpdate.state == "pending") {
        prompter.showUpdateDownloaded(um.activeUpdate);
    } else {
        prompter.checkForUpdates();
    }
}
