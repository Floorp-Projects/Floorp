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
   if( navigator.vendor != "Mozilla Sunbird" ) {
     var navWindow;

     // if not, get the most recently used browser window
       try {
         var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
           .getService(Components.interfaces.nsIWindowMediator);
         navWindow = wm.getMostRecentWindow("navigator:browser");
       } catch (e) {
         dump("launchBrowser: Exception: " + e + "\n");
       }
     
     if (navWindow) {
       if ("delayedOpenTab" in navWindow)
         navWindow.delayedOpenTab(UrlToGoTo);
       else if ("loadURI" in navWindow)
         navWindow.loadURI(UrlToGoTo);
       else
         navWindow.content.location.href = UrlToGoTo;
     }
     // if all else fails, open a new window 
     else {

       var ass = Components.classes["@mozilla.org/appshell/appShellService;1"].getService(Components.interfaces.nsIAppShellService);
       w = ass.hiddenDOMWindow;

       w.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", UrlToGoTo );
     }
   } 
   else
     {
       try {
         var messenger = Components.classes["@mozilla.org/messenger;1"].createInstance();
         messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
         messenger.launchExternalURL(UrlToGoTo);  
       } catch (e) {
         dump("launchBrowser: Exception: "+e+"\n");
       }
     }
   //window.open(UrlToGoTo, "_blank", "chrome,menubar,toolbar,resizable,dialog=no");
   //window.open( UrlToGoTo, "calendar-opened-window" );
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


function goOpenExtensions(aOpenMode)
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
        debug("EMURL: "+EMURL+"\n");
        window.openDialog(EMURL, "", EMFEATURES);
    }
}


function showElement(elementId)
{
    try {
        debug("showElement: showing " + elementId);
        document.getElementById(elementId).removeAttribute("hidden");
    } catch (e) {
        dump("showElement: Couldn't remove hidden attribute from " + elementId + "\n");
    }
}


function hideElement(elementId)
{
    try {
        debug("hideElement: hiding " + elementId);
        document.getElementById(elementId).setAttribute("hidden", "true");
    } catch (e) {
        dump("hideElement: Couldn't set hidden attribute on " + elementId + "\n");
    }
}


function enableElement(elementId)
{
    try {
        debug("enableElement: enabling " + elementId);
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
        debug("disableElement: disabling " + elementId);
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

function debug(text)
{
    if(gDebugEnabled)
        dump(text + "\n");
}


function debugVar(variable)
{
    if(gDebugEnabled)
        dump(variable + ": " + variable + "\n");
}


