# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Alec Flett <alecf@netscape.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

/**
 * Communicator Shared Utility Library
 * for shared application glue for the Communicator suite of applications
 **/

var goPrefWindow = 0;

function getBrowserURL()
{
  return "chrome://browser/content/browser.xul";
}

function goToggleToolbar( id, elementID )
{
  var toolbar = document.getElementById(id);
  var element = document.getElementById(elementID);
  if (toolbar)
  {
    var isHidden = toolbar.hidden;
    toolbar.hidden = !isHidden;
    document.persist(id, 'hidden');
    if (element) {
      element.setAttribute("checked", isHidden ? "true" : "false");
      document.persist(elementID, 'checked');
    }
  }
}

// urlPref: lets each application have its own throbber URL. example: "messenger.throbber.url"
// event: lets shift+click open it in a new window, etc.
function goClickThrobber( urlPref, e )
{
  var url;
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    url = pref.getComplexValue(urlPref, Components.interfaces.nsIPrefLocalizedString).data;
  }

  catch(e) {
    url = null;
  }

  if ( url )
    openUILink(url, e);
}

function getTopWin()
{
    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
    var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    var topWindowOfType = windowManagerInterface.getMostRecentWindow( "navigator:browser" );

    if (topWindowOfType) {
        return topWindowOfType;
    }
    return null;
}

function openTopWin( url )
{
    openUILink(url, {})
}



function getBoolPref ( prefname, def )
{
  try { 
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefBranch);
    return pref.getBoolPref(prefname);
  }
  catch(er) {
    return def;
  }
}
  

// openUILink handles clicks on UI elements that cause URLs to load.
function openUILink( url, e, ignoreButton, ignoreAlt )
{
  var where = whereToOpenLink(e, ignoreButton, ignoreAlt);
  openUILinkIn(url, where);
}


/* whereToOpenLink() looks at an event to decide where to open a link.
 *
 * The event may be a mouse event (click, double-click, middle-click) or keypress event (enter).
 *
 * On Windows, the modifiers are:
 * Ctrl        new tab, selected
 * Shift       new window
 * Ctrl+Shift  new tab, in background
 * Alt         save
 *
 * You can swap Ctrl and Ctrl+shift by toggling the hidden pref
 * browser.tabs.loadBookmarksInBackground (not browser.tabs.loadInBackground, which
 * is for content area links).
 *
 * Middle-clicking is the same as Ctrl+clicking (it opens a new tab) and it is
 * subject to the shift modifier and pref in the same way.
 *
 * Exceptions: 
 * - Alt is ignored for menu items selected using the keyboard so you don't accidentally save stuff.  
 *    (Currently, the Alt isn't sent here at all for menu items, but that will change in bug 126189.)
 * - Alt is hard to use in context menus, because pressing Alt closes the menu.
 * - Alt can't be used on the bookmarks toolbar because Alt is used for "treat this as something draggable".
 * - The button is ignored for the middle-click-paste-URL feature, since it's always a middle-click.
 */
function whereToOpenLink( e, ignoreButton, ignoreAlt )
{
  if (e == null)
    e = { shiftKey:false, ctrlKey:false, metaKey:false, altKey:false, button:0 };

  var shift = e.shiftKey;
  var ctrl =  e.ctrlKey;
  var meta =  e.metaKey;
  var alt  =  e.altKey && !ignoreAlt;

  // ignoreButton allows "middle-click paste" to use function without always opening in a new window.
  var middle = !ignoreButton && e.button == 1;
  var middleUsesTabs = getBoolPref("browser.tabs.opentabfor.middleclick", true);

  // Don't do anything special with right-mouse clicks.  They're probably clicks on context menu items.

#ifdef XP_MACOSX
  if (meta || (middle && middleUsesTabs)) {
#else
  if (ctrl || (middle && middleUsesTabs)) {
#endif
    if (shift)
      return "tabshifted";
    else
      return "tab";
  }
  else if (alt) {
    return "save";
  }
  else if (shift || (middle && !middleUsesTabs)) {
    return "window";
  }
  else {
    return "current";
  }
}

/* openUILinkIn opens a URL in a place specified by the parameter |where|.
 *
 * |where| can be:
 *  "current"     current tab            (if there aren't any browser windows, then in a new window instead)
 *  "tab"         new tab                (if there aren't any browser windows, then in a new window instead)
 *  "tabshifted"  same as "tab" but in background if default is to select new tabs, and vice versa
 *  "window"      new window
 *  "save"        save to disk (with no filename hint!)
 */
function openUILinkIn( url, where )
{
  if (!where)
    return;

  if ((url == null) || (url == "")) 
    return;
  // xlate the URL if necessary
  if (url.indexOf("urn:") == 0) {
      url = xlateURL(url);        // does RDF urn expansion
  }
  // avoid loading "", since this loads a directory listing
  if (url == "") {
      url = "about:blank";
  }

  if (where == "save") {
    saveURL(url, null, null, true);
    return;
  }

  var w = (where == "window") ? null : getTopWin();
  if (!w) {
    openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
    return;
  }
  var browser = w.document.getElementById("content");

  switch (where) {
  case "current":
    browser.loadURI(url);
    w._content.focus();
    break;
  case "tabshifted":
  case "tab":
    var tab = browser.addTab(url);

    // We check the pref here, rather than in whereToOpenLink, because an "open link in tab"
    // context menu item could call openUILinkwhere directly.
    if ((where == "tab") ^ getBoolPref("browser.tabs.loadBookmarksInBackground", false)) {
      browser.selectedTab = tab;
      w._content.focus();
    }

    break;
  }
}

// Used as an onclick handler for UI elements with link-like behavior.
// e.g. onclick="checkForMiddleClick(this, event);"
function checkForMiddleClick(node, event)
{
  if (event.button == 1) {
    /* Execute the node's oncommand.
     *
     * Using eval() because of bug 246720.  Would like to use node.oncommand(event).
     *
     * Since we're using eval():
     *
     * |event| is correct because the name of this function's formal parameter matches
     * the automatic name of the formal parameter for oncommand, |event|.
     *
     * |this| is incorrect.  To make it correct, we would have to use Function.call.
     */
    eval(node.getAttribute("oncommand"));

    // If the middle-click was on part of a menu, close the menu.
    // (Menus close automatically with left-click but not with middle-click.)
    closeMenus(event.target);
  }
}

// Closes all popups that are ancestors of the node.
function closeMenus(node)
{
  if ("tagName" in node) {
    if (node.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
    && (node.tagName == "menupopup" || node.tagName == "popup"))
      node.hidePopup();

    closeMenus(node.parentNode);
  }
}

// update menu items that rely on focus
function goUpdateGlobalEditMenuItems()
{
  goUpdateCommand('cmd_undo');
  goUpdateCommand('cmd_redo');
  goUpdateCommand('cmd_cut');
  goUpdateCommand('cmd_copy');
  goUpdateCommand('cmd_paste');
  goUpdateCommand('cmd_selectAll');
  goUpdateCommand('cmd_delete');
}

// update menu items that rely on the current selection
function goUpdateSelectEditMenuItems()
{
  goUpdateCommand('cmd_cut');
  goUpdateCommand('cmd_copy');
  goUpdateCommand('cmd_delete');
  goUpdateCommand('cmd_selectAll');
}

// update menu items that relate to undo/redo
function goUpdateUndoEditMenuItems()
{
  goUpdateCommand('cmd_undo');
  goUpdateCommand('cmd_redo');
}

// update menu items that depend on clipboard contents
function goUpdatePasteMenuItems()
{
  goUpdateCommand('cmd_paste');
}

// Gather all descendent text under given document node.
function gatherTextUnder ( root ) 
{
  var text = "";
  var node = root.firstChild;
  var depth = 1;
  while ( node && depth > 0 ) {
    // See if this node is text.
    if ( node.nodeName == "#text" ) {
      // Add this text to our collection.
      text += " " + node.data;
    } else if ( node.nodeType == Node.ELEMENT_NODE 
                && node.localName.toUpperCase() == "IMG" ) {
      // If it has an alt= attribute, use that.
      var altText = node.getAttribute( "alt" );
      if ( altText && altText != "" ) {
        text = altText;
        break;
      }
    }
    // Find next node to test.
    // First, see if this node has children.
    if ( node.hasChildNodes() ) {
      // Go to first child.
      node = node.firstChild;
      depth++;
    } else {
      // No children, try next sibling.
      if ( node.nextSibling ) {
        node = node.nextSibling;
      } else {
        // Last resort is our next oldest uncle/aunt.
        node = node.parentNode.nextSibling;
        depth--;
      }
    }
  }
  // Strip leading whitespace.
  text = text.replace( /^\s+/, "" );
  // Strip trailing whitespace.
  text = text.replace( /\s+$/, "" );
  // Compress remaining whitespace.
  text = text.replace( /\s+/g, " " );
  return text;
}

function getShellService()
{
  var shell = null;
  try {
    shell = Components.classes["@mozilla.org/browser/shell-service;1"]
      .getService(Components.interfaces.nsIShellService);
  } catch (e) {}
  return shell;
}
