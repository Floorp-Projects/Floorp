/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// A module for working with chat windows.

this.EXPORTED_SYMBOLS = ["Chat"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

// A couple of internal helper function.
function isWindowChromeless(win) {
  // XXX - stolen from browser-social.js, but there's no obvious place to
  // put this so it can be shared.

  // Is this a popup window that doesn't want chrome shown?
  let docElem = win.document.documentElement;
  // extrachrome is not restored during session restore, so we need
  // to check for the toolbar as well.
  let chromeless = docElem.getAttribute("chromehidden").contains("extrachrome") ||
                   docElem.getAttribute('chromehidden').contains("toolbar");
  return chromeless;
}

function isWindowGoodForChats(win) {
  return !win.closed &&
         !!win.document.getElementById("pinnedchats") &&
         !isWindowChromeless(win) &&
         !PrivateBrowsingUtils.isWindowPrivate(win);
}

function getChromeWindow(contentWin) {
  return contentWin.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShellTreeItem)
                   .rootTreeItem
                   .QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindow);
}

/*
 * The exported Chat object
 */

let Chat = {
  /**
   * Open a new chatbox.
   *
   * @param contentWindow [optional]
   *        The content window that requested this chat.  May be null.
   * @param origin
   *        The origin for the chat.  This is primarily used as an identifier
   *        to help identify all chats from the same provider.
   * @param title
   *        The title to be used if a new chat window is created.
   * @param url
   *        The URL for the that.  Should be under the origin.  If an existing
   *        chatbox exists with the same URL, it will be reused and returned.
   * @param mode [optional]
   *        May be undefined or 'minimized'
   * @param focus [optional]
   *        Indicates if the chatbox should be focused.  If undefined the chat
   *        will be focused if the window is currently handling user input (ie,
   *        if the chat is being opened as a direct result of user input)

   * @return A chatbox binding.  This binding has a number of promises which
   *         can be used to determine when the chatbox is being created and
   *         has loaded.  Will return null if no chat can be created (Which
   *         should only happen in edge-cases)
   */
  open: function(contentWindow, origin, title, url, mode, focus, callback) {
    let chromeWindow = this.findChromeWindowForChats(contentWindow);
    if (!chromeWindow) {
      Cu.reportError("Failed to open a chat window - no host window could be found.");
      return null;
    }

    let chatbar = chromeWindow.document.getElementById("pinnedchats");
    chatbar.hidden = false;
    let chatbox = chatbar.openChat(origin, title, url, mode, callback);
    // getAttention is ignored if the target window is already foreground, so
    // we can call it unconditionally.
    chromeWindow.getAttention();
    // If focus is undefined we want automatic focus handling, and only focus
    // if a direct result of user action.
    if (focus === undefined) {
      let dwu = chromeWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
      focus = dwu.isHandlingUserInput;
    }
    if (focus) {
      chatbar.focus();
    }
    return chatbox;
  },

  /**
   * Close all chats from the specified origin.
   *
   * @param origin
   *        The origin from which all chats should be closed.
   */
  closeAll: function(origin) {
    // close all attached chat windows
    let winEnum = Services.wm.getEnumerator("navigator:browser");
    while (winEnum.hasMoreElements()) {
      let win = winEnum.getNext();
      let chatbar = win.document.getElementById("pinnedchats");
      if (!chatbar)
        continue;
      let chats = [c for (c of chatbar.children) if (c.content.getAttribute("origin") == origin)];
      [c.close() for (c of chats)];
    }

    // close all standalone chat windows
    winEnum = Services.wm.getEnumerator("Social:Chat");
    while (winEnum.hasMoreElements()) {
      let win = winEnum.getNext();
      if (win.closed)
        continue;
      let chatOrigin = win.document.getElementById("chatter").content.getAttribute("origin");
      if (origin == chatOrigin)
        win.close();
    }
  },

  /**
   * Focus the chatbar associated with a window
   *
   * @param window
   */
  focus: function(win) {
    let chatbar = win.document.getElementById("pinnedchats");
    if (chatbar && !chatbar.hidden) {
      chatbar.focus();
    }

  },

  // This is exported as socialchat.xml needs to find a window when a chat
  // is re-docked.
  findChromeWindowForChats: function(preferredWindow) {
    if (preferredWindow) {
      preferredWindow = getChromeWindow(preferredWindow);
      if (isWindowGoodForChats(preferredWindow)) {
        return preferredWindow;
      }
    }
    // no good - we just use the "most recent" browser window which can host
    // chats (we used to try and "group" all chats in the same browser window,
    // but that didn't work out so well - see bug 835111

    // Try first the most recent window as getMostRecentWindow works
    // even on platforms where getZOrderDOMWindowEnumerator is broken
    // (ie. Linux).  This will handle most cases, but won't work if the
    // foreground window is a popup.

    let mostRecent = Services.wm.getMostRecentWindow("navigator:browser");
    if (isWindowGoodForChats(mostRecent))
      return mostRecent;

    let topMost, enumerator;
    // *sigh* - getZOrderDOMWindowEnumerator is broken except on Mac and
    // Windows.  We use BROKEN_WM_Z_ORDER as that is what some other code uses
    // and a few bugs recommend searching mxr for this symbol to identify the
    // workarounds - we want this code to be hit in such searches.
    let os = Services.appinfo.OS;
    const BROKEN_WM_Z_ORDER = os != "WINNT" && os != "Darwin";
    if (BROKEN_WM_Z_ORDER) {
      // this is oldest to newest and no way to change the order.
      enumerator = Services.wm.getEnumerator("navigator:browser");
    } else {
      // here we explicitly ask for bottom-to-top so we can use the same logic
      // where BROKEN_WM_Z_ORDER is true.
      enumerator = Services.wm.getZOrderDOMWindowEnumerator("navigator:browser", false);
    }
    while (enumerator.hasMoreElements()) {
      let win = enumerator.getNext();
      if (!win.closed && isWindowGoodForChats(win))
        topMost = win;
    }
    return topMost;
  },
}
