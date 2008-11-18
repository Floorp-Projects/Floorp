# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Blake Ross <blake@cs.stanford.edu>
#   David Hyatt <hyatt@mozilla.org>
#   Peter Annema <disttsc@bart.nl>
#   Dean Tessman <dean_tessman@hotmail.com>
#   Kevin Puetz <puetzk@iastate.edu>
#   Ben Goodger <ben@netscape.com>
#   Pierre Chanial <chanial@noos.fr>
#   Jason Eager <jce2@po.cwru.edu>
#   Joe Hewitt <hewitt@netscape.com>
#   Alec Flett <alecf@netscape.com>
#   Asaf Romano <mozilla.mano@sent.com>
#   Jason Barnabe <jason_barnabe@fastmail.fm>
#   Peter Parente <parente@cs.unc.edu>
#   Giorgio Maone <g.maone@informaction.com>
#   Tom Germeau <tom.germeau@epigoon.com>
#   Jesse Ruderman <jruderman@gmail.com>
#   Joe Hughes <joe@retrovirus.com>
#   Pamela Greene <pamg.bugs@gmail.com>
#   Michael Ventnor <m.ventnor@gmail.com>
#   Simon Bünzli <zeniko@gmail.com>
#   Johnathan Nightingale <johnath@mozilla.com>
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#   Dão Gottwald <dao@mozilla.com>
#   Thomas K. Dyas <tdyas@zecador.org>
#   Edward Lee <edward.lee@engineering.uiuc.edu>
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

let Ci = Components.interfaces;
let Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;

const MAX_HISTORY_MENU_ITEMS = 15;

// We use this once, for Clear Private Data
const GLUE_CID = "@mozilla.org/browser/browserglue;1";

var gCharsetMenu = null;
var gLastBrowserCharset = null;
var gPrevCharset = null;
var gProxyFavIcon = null;
var gLastValidURLStr = "";
var gProgressCollapseTimer = null;
var appCore = null;
var gSidebarCommand = "";
var gInPrintPreviewMode = false;
let gDownloadMgr = null;

// Global variable that holds the nsContextMenu instance.
var gContextMenu = null;

var gChromeState = null; // chrome state before we went into print preview

var gSanitizeListener = null;

var gAutoHideTabbarPrefListener = null;
var gBookmarkAllTabsHandler = null;

#ifdef XP_MACOSX
var gClickAndHoldTimer = null;
#endif

#ifndef XP_MACOSX
var gEditUIVisible = true;
#endif

[
  ["gBrowser",            "content"],
  ["gNavToolbox",         "navigator-toolbox"],
  ["gURLBar",             "urlbar"],
  ["gNavigatorBundle",    "bundle_browser"],
  ["gProgressMeterPanel", "statusbar-progresspanel"],
  ["gFindBar",            "FindToolbar"]
].forEach(function (elementGlobal) {
  var [name, id] = elementGlobal;
  window.__defineGetter__(name, function () {
    var element = document.getElementById(id);
    if (!element)
      return null;
    delete window[name];
    return window[name] = element;
  });
  window.__defineSetter__(name, function (val) {
    delete window[name];
    return window[name] = val;
  });
});

__defineGetter__("gPrefService", function() {
  delete this.gPrefService;
  return this.gPrefService = Cc["@mozilla.org/preferences-service;1"].
                             getService(Ci.nsIPrefBranch2);
});

/**
* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/
function pageShowEventHandlers(event) {
  // Filter out events that are not about the document load we are interested in
  if (event.originalTarget == content.document) {
    charsetLoadListener(event);
    XULBrowserWindow.asyncUpdateUI();
  }
}

/**
 * Determine whether or not the content area is displaying a page with frames,
 * and if so, toggle the display of the 'save frame as' menu item.
 **/
function getContentAreaFrameCount()
{
  var saveFrameItem = document.getElementById("menu_saveFrame");
  if (!content || !content.frames.length || !isContentFrame(document.commandDispatcher.focusedWindow))
    saveFrameItem.setAttribute("hidden", "true");
  else
    saveFrameItem.removeAttribute("hidden");
}

function UpdateBackForwardCommands(aWebNavigation) {
  var backBroadcaster = document.getElementById("Browser:Back");
  var forwardBroadcaster = document.getElementById("Browser:Forward");

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = backBroadcaster.hasAttribute("disabled");
  var forwardDisabled = forwardBroadcaster.hasAttribute("disabled");
  if (backDisabled == aWebNavigation.canGoBack) {
    if (backDisabled)
      backBroadcaster.removeAttribute("disabled");
    else
      backBroadcaster.setAttribute("disabled", true);
  }

  if (forwardDisabled == aWebNavigation.canGoForward) {
    if (forwardDisabled)
      forwardBroadcaster.removeAttribute("disabled");
    else
      forwardBroadcaster.setAttribute("disabled", true);
  }
}

#ifdef XP_MACOSX
/**
 * Click-and-Hold implementation for the Back and Forward buttons
 * XXXmano: should this live in toolbarbutton.xml?
 */
function ClickAndHoldMouseDownCallback(aButton)
{
  aButton.open = true;
  gClickAndHoldTimer = null;
}

function ClickAndHoldMouseDown(aEvent)
{
  /**
   * 1. Only left click starts the click and hold timer.
   * 2. Exclude the dropmarker area. This is done by excluding
   *    elements which target their events directly to the toolbarbutton
   *    element, i.e. when the nearest-parent-element which allows-events
   *    is the toolbarbutton element itself.
   * 3. Do not start the click-and-hold timer if the toolbarbutton is disabled.
   */
  if (aEvent.button != 0 ||
      aEvent.originalTarget == aEvent.currentTarget ||
      aEvent.currentTarget.disabled)
    return;

  gClickAndHoldTimer =
    setTimeout(ClickAndHoldMouseDownCallback, 500, aEvent.currentTarget);
}

function MayStopClickAndHoldTimer(aEvent)
{
  // Note passing null here is a no-op
  clearTimeout(gClickAndHoldTimer);
}

function ClickAndHoldStopEvent(aEvent)
{
  if (aEvent.originalTarget.localName != "menuitem" &&
      aEvent.currentTarget.open)
    aEvent.stopPropagation();
}

function SetClickAndHoldHandlers()
{
  function _addClickAndHoldListenersOnElement(aElm)
  {
    aElm.addEventListener("mousedown",
                          ClickAndHoldMouseDown,
                          false);
    aElm.addEventListener("mouseup",
                          MayStopClickAndHoldTimer,
                          false);
    aElm.addEventListener("mouseout",
                          MayStopClickAndHoldTimer,
                          false);
    
    // don't propagate onclick and oncommand events after
    // click-and-hold opened the drop-down menu
    aElm.addEventListener("command",
                          ClickAndHoldStopEvent,
                          true);  
    aElm.addEventListener("click",
                          ClickAndHoldStopEvent,
                          true);  
  }

  // Bug 414797: Clone the dropmarker's menu into both the back and
  // the forward buttons.
  var unifiedButton = document.getElementById("unified-back-forward-button");
  if (unifiedButton && !unifiedButton._clickHandlersAttached)  {
    var popup = document.getElementById("back-forward-dropmarker")
                       .firstChild.cloneNode(true);
    var backButton = document.getElementById("back-button");
    backButton.setAttribute("type", "menu-button");
    backButton.appendChild(popup);
    _addClickAndHoldListenersOnElement(backButton);
    var forwardButton = document.getElementById("forward-button");
    popup = popup.cloneNode(true);
    forwardButton.setAttribute("type", "menu-button");
    forwardButton.appendChild(popup);    
    _addClickAndHoldListenersOnElement(forwardButton);
    unifiedButton._clickHandlersAttached = true;
  }
}
#endif

function BookmarkThisTab() {
  PlacesCommandHook.bookmarkPage(gBrowser.mContextTab.linkedBrowser,
                                 PlacesUtils.bookmarksMenuFolderId, true);
}

const gSessionHistoryObserver = {
  observe: function(subject, topic, data)
  {
    if (topic != "browser:purge-session-history")
      return;

    var backCommand = document.getElementById("Browser:Back");
    backCommand.setAttribute("disabled", "true");
    var fwdCommand = document.getElementById("Browser:Forward");
    fwdCommand.setAttribute("disabled", "true");

    if (gURLBar) {
      // Clear undo history of the URL bar
      gURLBar.editor.transactionManager.clear()
    }
  }
};

/**
 * Given a starting docshell and a URI to look up, find the docshell the URI
 * is loaded in. 
 * @param   aDocument
 *          A document to find instead of using just a URI - this is more specific. 
 * @param   aDocShell
 *          The doc shell to start at
 * @param   aSoughtURI
 *          The URI that we're looking for
 * @returns The doc shell that the sought URI is loaded in. Can be in 
 *          subframes.
 */
function findChildShell(aDocument, aDocShell, aSoughtURI) {
  aDocShell.QueryInterface(Components.interfaces.nsIWebNavigation);
  aDocShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  var doc = aDocShell.getInterface(Components.interfaces.nsIDOMDocument);
  if ((aDocument && doc == aDocument) || 
      (aSoughtURI && aSoughtURI.spec == aDocShell.currentURI.spec))
    return aDocShell;

  var node = aDocShell.QueryInterface(Components.interfaces.nsIDocShellTreeNode);
  for (var i = 0; i < node.childCount; ++i) {
    var docShell = node.getChildAt(i);
    docShell = findChildShell(aDocument, docShell, aSoughtURI);
    if (docShell)
      return docShell;
  }
  return null;
}

const gPopupBlockerObserver = {
  _reportButton: null,
  _kIPM: Components.interfaces.nsIPermissionManager,

  onUpdatePageReport: function (aEvent)
  {
    if (aEvent.originalTarget != gBrowser.selectedBrowser)
      return;

    if (!this._reportButton)
      this._reportButton = document.getElementById("page-report-button");

    if (!gBrowser.pageReport) {
      // Hide the popup blocker statusbar button
      this._reportButton.removeAttribute("blocked");

      return;
    }

    this._reportButton.setAttribute("blocked", true);

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!gBrowser.pageReport.reported) {
      if (gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var bundle_browser = document.getElementById("bundle_browser");
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var message;
        var popupCount = gBrowser.pageReport.length;
#ifdef XP_WIN
        var popupButtonText = bundle_browser.getString("popupWarningButton");
        var popupButtonAccesskey = bundle_browser.getString("popupWarningButton.accesskey");
#else
        var popupButtonText = bundle_browser.getString("popupWarningButtonUnix");
        var popupButtonAccesskey = bundle_browser.getString("popupWarningButtonUnix.accesskey");
#endif
        if (popupCount > 1)
          message = bundle_browser.getFormattedString("popupWarningMultiple", [brandShortName, popupCount]);
        else
          message = bundle_browser.getFormattedString("popupWarning", [brandShortName]);

        var notificationBox = gBrowser.getNotificationBox();
        var notification = notificationBox.getNotificationWithValue("popup-blocked");
        if (notification) {
          notification.label = message;
        }
        else {
          var buttons = [{
            label: popupButtonText,
            accessKey: popupButtonAccesskey,
            popup: "blockedPopupOptions",
            callback: null
          }];

          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          notificationBox.appendNotification(message, "popup-blocked",
                                             "chrome://browser/skin/Info.png",
                                             priority, buttons);
        }
      }

      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      gBrowser.pageReport.reported = true;
    }
  },

  toggleAllowPopupsForSite: function (aEvent)
  {
    var currentURI = gBrowser.selectedBrowser.webNavigation.currentURI;
    var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                       .getService(this._kIPM);
    var shouldBlock = aEvent.target.getAttribute("block") == "true";
    var perm = shouldBlock ? this._kIPM.DENY_ACTION : this._kIPM.ALLOW_ACTION;
    pm.add(currentURI, "popup", perm);

    gBrowser.getNotificationBox().removeCurrentNotification();
  },

  fillPopupList: function (aEvent)
  {
    var bundle_browser = document.getElementById("bundle_browser");
    // XXXben - rather than using |currentURI| here, which breaks down on multi-framed sites
    //          we should really walk the pageReport and create a list of "allow for <host>"
    //          menuitems for the common subset of hosts present in the report, this will
    //          make us frame-safe.
    //
    // XXXjst - Note that when this is fixed to work with multi-framed sites,
    //          also back out the fix for bug 343772 where
    //          nsGlobalWindow::CheckOpenAllow() was changed to also
    //          check if the top window's location is whitelisted.
    var uri = gBrowser.selectedBrowser.webNavigation.currentURI;
    var blockedPopupAllowSite = document.getElementById("blockedPopupAllowSite");
    try {
      blockedPopupAllowSite.removeAttribute("hidden");

      var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                        .getService(this._kIPM);
      if (pm.testPermission(uri, "popup") == this._kIPM.ALLOW_ACTION) {
        // Offer an item to block popups for this site, if a whitelist entry exists
        // already for it.
        var blockString = bundle_browser.getFormattedString("popupBlock", [uri.host]);
        blockedPopupAllowSite.setAttribute("label", blockString);
        blockedPopupAllowSite.setAttribute("block", "true");
      }
      else {
        // Offer an item to allow popups for this site
        var allowString = bundle_browser.getFormattedString("popupAllow", [uri.host]);
        blockedPopupAllowSite.setAttribute("label", allowString);
        blockedPopupAllowSite.removeAttribute("block");
      }
    }
    catch (e) {
      blockedPopupAllowSite.setAttribute("hidden", "true");
    }

    if (gPrivateBrowsingUI.privateBrowsingEnabled)
      blockedPopupAllowSite.setAttribute("disabled", "true");

    var item = aEvent.target.lastChild;
    while (item && item.getAttribute("observes") != "blockedPopupsSeparator") {
      var next = item.previousSibling;
      item.parentNode.removeChild(item);
      item = next;
    }

    var foundUsablePopupURI = false;
    var pageReport = gBrowser.pageReport;
    if (pageReport) {
      for (var i = 0; i < pageReport.length; ++i) {
        var popupURIspec = pageReport[i].popupWindowURI.spec;

        // Sometimes the popup URI that we get back from the pageReport
        // isn't useful (for instance, netscape.com's popup URI ends up
        // being "http://www.netscape.com", which isn't really the URI of
        // the popup they're trying to show).  This isn't going to be
        // useful to the user, so we won't create a menu item for it.
        if (popupURIspec == "" || popupURIspec == "about:blank" ||
            popupURIspec == uri.spec)
          continue;

        // Because of the short-circuit above, we may end up in a situation
        // in which we don't have any usable popup addresses to show in
        // the menu, and therefore we shouldn't show the separator.  However,
        // since we got past the short-circuit, we must've found at least
        // one usable popup URI and thus we'll turn on the separator later.
        foundUsablePopupURI = true;

        var menuitem = document.createElement("menuitem");
        var label = bundle_browser.getFormattedString("popupShowPopupPrefix",
                                                      [popupURIspec]);
        menuitem.setAttribute("label", label);
        menuitem.setAttribute("popupWindowURI", popupURIspec);
        menuitem.setAttribute("popupWindowFeatures", pageReport[i].popupWindowFeatures);
        menuitem.setAttribute("popupWindowName", pageReport[i].popupWindowName);
        menuitem.setAttribute("oncommand", "gPopupBlockerObserver.showBlockedPopup(event);");
        menuitem.requestingWindow = pageReport[i].requestingWindow;
        menuitem.requestingDocument = pageReport[i].requestingDocument;
        aEvent.target.appendChild(menuitem);
      }
    }

    // Show or hide the separator, depending on whether we added any
    // showable popup addresses to the menu.
    var blockedPopupsSeparator =
      document.getElementById("blockedPopupsSeparator");
    if (foundUsablePopupURI)
      blockedPopupsSeparator.removeAttribute("hidden");
    else
      blockedPopupsSeparator.setAttribute("hidden", true);

    var blockedPopupDontShowMessage = document.getElementById("blockedPopupDontShowMessage");
    var showMessage = gPrefService.getBoolPref("privacy.popups.showBrowserMessage");
    blockedPopupDontShowMessage.setAttribute("checked", !showMessage);
    if (aEvent.target.localName == "popup")
      blockedPopupDontShowMessage.setAttribute("label", bundle_browser.getString("popupWarningDontShowFromMessage"));
    else
      blockedPopupDontShowMessage.setAttribute("label", bundle_browser.getString("popupWarningDontShowFromStatusbar"));
  },

  showBlockedPopup: function (aEvent)
  {
    var target = aEvent.target;
    var popupWindowURI = target.getAttribute("popupWindowURI");
    var features = target.getAttribute("popupWindowFeatures");
    var name = target.getAttribute("popupWindowName");

    var dwi = target.requestingWindow;

    // If we have a requesting window and the requesting document is
    // still the current document, open the popup.
    if (dwi && dwi.document == target.requestingDocument) {
      dwi.open(popupWindowURI, name, features);
    }
  },

  editPopupSettings: function ()
  {
    var host = "";
    try {
      var uri = gBrowser.selectedBrowser.webNavigation.currentURI;
      host = uri.host;
    }
    catch (e) { }

    var bundlePreferences = document.getElementById("bundle_preferences");
    var params = { blockVisible   : false,
                   sessionVisible : false,
                   allowVisible   : true,
                   prefilledHost  : host,
                   permissionType : "popup",
                   windowTitle    : bundlePreferences.getString("popuppermissionstitle"),
                   introText      : bundlePreferences.getString("popuppermissionstext") };
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                        .getService(Components.interfaces.nsIWindowMediator);
    var existingWindow = wm.getMostRecentWindow("Browser:Permissions");
    if (existingWindow) {
      existingWindow.initWithParams(params);
      existingWindow.focus();
    }
    else
      window.openDialog("chrome://browser/content/preferences/permissions.xul",
                        "_blank", "resizable,dialog=no,centerscreen", params);
  },

  dontShowMessage: function ()
  {
    var showMessage = gPrefService.getBoolPref("privacy.popups.showBrowserMessage");
    var firstTime = gPrefService.getBoolPref("privacy.popups.firstTime");

    // If the info message is showing at the top of the window, and the user has never
    // hidden the message before, show an info box telling the user where the info
    // will be displayed.
    if (showMessage && firstTime)
      this._displayPageReportFirstTime();

    gPrefService.setBoolPref("privacy.popups.showBrowserMessage", !showMessage);

    gBrowser.getNotificationBox().removeCurrentNotification();
  },

  _displayPageReportFirstTime: function ()
  {
    window.openDialog("chrome://browser/content/pageReportFirstTime.xul", "_blank",
                      "dependent");
  }
};

const gXPInstallObserver = {
  _findChildShell: function (aDocShell, aSoughtShell)
  {
    if (aDocShell == aSoughtShell)
      return aDocShell;

    var node = aDocShell.QueryInterface(Components.interfaces.nsIDocShellTreeNode);
    for (var i = 0; i < node.childCount; ++i) {
      var docShell = node.getChildAt(i);
      docShell = this._findChildShell(docShell, aSoughtShell);
      if (docShell == aSoughtShell)
        return docShell;
    }
    return null;
  },

  _getBrowser: function (aDocShell)
  {
    for (var i = 0; i < gBrowser.browsers.length; ++i) {
      var browser = gBrowser.getBrowserAtIndex(i);
      if (this._findChildShell(browser.docShell, aDocShell))
        return browser;
    }
    return null;
  },

  observe: function (aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    var browserBundle = document.getElementById("bundle_browser");
    switch (aTopic) {
    case "xpinstall-install-blocked":
      var installInfo = aSubject.QueryInterface(Components.interfaces.nsIXPIInstallInfo);
      var win = installInfo.originatingWindow;
      var shell = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                     .getInterface(Components.interfaces.nsIWebNavigation)
                     .QueryInterface(Components.interfaces.nsIDocShell);
      var browser = this._getBrowser(shell);
      if (browser) {
        var host = installInfo.originatingURI.host;
        var brandShortName = brandBundle.getString("brandShortName");
        var notificationName, messageString, buttons;
        if (!gPrefService.getBoolPref("xpinstall.enabled")) {
          notificationName = "xpinstall-disabled"
          if (gPrefService.prefIsLocked("xpinstall.enabled")) {
            messageString = browserBundle.getString("xpinstallDisabledMessageLocked");
            buttons = [];
          }
          else {
            messageString = browserBundle.getFormattedString("xpinstallDisabledMessage",
                                                             [brandShortName, host]);

            buttons = [{
              label: browserBundle.getString("xpinstallDisabledButton"),
              accessKey: browserBundle.getString("xpinstallDisabledButton.accesskey"),
              popup: null,
              callback: function editPrefs() {
                gPrefService.setBoolPref("xpinstall.enabled", true);
                return false;
              }
            }];
          }
        }
        else {
          notificationName = "xpinstall"
          messageString = browserBundle.getFormattedString("xpinstallPromptWarning",
                                                           [brandShortName, host]);

          buttons = [{
            label: browserBundle.getString("xpinstallPromptAllowButton"),
            accessKey: browserBundle.getString("xpinstallPromptAllowButton.accesskey"),
            popup: null,
            callback: function() {
              var mgr = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                                  .createInstance(Components.interfaces.nsIXPInstallManager);
              mgr.initManagerWithInstallInfo(installInfo);
              return false;
            }
          }];
        }

        var notificationBox = gBrowser.getNotificationBox(browser);
        if (!notificationBox.getNotificationWithValue(notificationName)) {
          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          const iconURL = "chrome://mozapps/skin/update/update.png";
          notificationBox.appendNotification(messageString, notificationName,
                                             iconURL, priority, buttons);
        }
      }
      break;
    }
  }
};

// Simple gestures support
//
// As per bug #412486, web content must not be allowed to receive any
// simple gesture events.  Multi-touch gesture APIs are in their
// infancy and we do NOT want to be forced into supporting an API that
// will probably have to change in the future.  (The current Mac OS X
// API is undocumented and was reverse-engineered.)  Until support is
// implemented in the event dispatcher to keep these events as
// chrome-only, we must listen for the simple gesture events during
// the capturing phase and call stopPropagation on every event.

let gGestureSupport = {
  /**
   * Add or remove mouse gesture event listeners
   *
   * @param aAddListener
   *        True to add/init listeners and false to remove/uninit
   */
  init: function GS_init(aAddListener) {
    const gestureEvents = ["SwipeGesture",
      "MagnifyGestureStart", "MagnifyGestureUpdate", "MagnifyGesture",
      "RotateGestureStart", "RotateGestureUpdate", "RotateGesture"];

    let addRemove = aAddListener ? window.addEventListener :
      window.removeEventListener;

    for each (let event in gestureEvents)
      addRemove("Moz" + event, this, true);
  },

  /**
   * Dispatch events based on the type of mouse gesture event.
   * For now, make sure to stop propagation of every gesture event
   *
   * @param aEvent
   *        The gesture event to handle
   */
  handleEvent: function GS_handleEvent(aEvent) {
    aEvent.stopPropagation();

    switch (aEvent.type) {
      case "MozSwipeGesture":
        return this.onSwipe(aEvent);
      case "MozMagnifyGestureStart":
      case "MozRotateGestureStart":
        return this.onStart(aEvent);
      case "MozMagnifyGestureUpdate":
        return this._handleUpdate(aEvent, 100, "pinch.out", "pinch.in");
      case "MozRotateGestureUpdate":
        return this._handleUpdate(aEvent, 22.5, "twist.right", "twist.left");
    }
  },

  /**
   * Convert a gesture and pressed keys into the corresponding command action.
   * The preference must have "shift" before "alt" before "ctrl" before "meta"
   * with each separated by periods.
   *
   * @param aGestureKeys
   *        An array that has the gesture type as the first element and
   *        additional elements for each key pressed
   * @return Id of the command to execute
   */
  _getCommand: function GS__getCommand(aGestureKeys) {
    const gestureBranch = "browser.gesture."
    try {
      return gPrefService.getCharPref(gestureBranch + aGestureKeys.join("."));
    }
    // No preference is set, so don't give a command
    catch (e) {}
  },

  /**
   * Generator producing the powerset of the input array where the first result
   * is the complete set and the last result (before StopIteration) is empty.
   *
   * @param aArray
   *        Source array containing any number of elements
   * @yield Array that is a subset of the input array from full set to empty
   */
  _power: function GS__power(aArray) {
    // Create a bitmask based on the length of the array
    let num = 1 << aArray.length;
    while (--num >= 0)
      // Only select array elements where the current bit is set
      yield aArray.reduce(function(aPrev, aCurr, aIndex) {
        if (num & 1 << aIndex)
          aPrev.push(aCurr);
        return aPrev;
      }, []);
  },

  /**
   * Determine what action to do for the gesture based on which keys are
   * pressed and which commands are set
   *
   * @param aEvent
   *        The original gesture event to convert into a fake click event
   * @param aGesture
   *        Name of the gesture
   */
  _doAction: function GS__doAction(aEvent, aGesture) {
    // Create a fake event that pretends the gesture is a button click
    let fakeEvent = { shiftKey: aEvent.shiftKey, ctrlKey: aEvent.ctrlKey,
      metaKey: aEvent.metaKey, altKey: aEvent.altKey, button: 0 };

    // Create an array of pressed keys in a fixed order so that a command for
    // "meta" is preferred over "ctrl" when both buttons are pressed (and a
    // command for both don't exist)
    let keyCombos = [];
    const keys = ["shift", "alt", "ctrl", "meta"];
    for each (let key in keys)
      if (aEvent[key + "Key"]) 
        keyCombos.push(key);

    try {
      // Try each combination of key presses in decreasing order for commands
      for (let subCombo in this._power(keyCombos)) {
        let command = this._getCommand([aGesture].concat(subCombo));
        // Do the command if we found one to do
        if (command) {
          let node = document.getElementById(command);
          // Use the command element if it exists
          if (node && node.hasAttribute("oncommand"))
            // XXX: Use node.oncommand(event) once bug 246720 is fixed
            return node.getAttribute("disabled") == "true" ? true :
              new Function("event", node.getAttribute("oncommand")).
              call(node, fakeEvent);

          // Otherwise it should be a "standard" command
          return goDoCommand(command);
        }
      }
    }
    // The generator ran out of key combinations, so just do nothing
    catch (e) {}
  },

  /**
   * Convert the swipe gesture into a browser action based on the direction
   *
   * @param aEvent
   *        The swipe event to handle
   */
  onSwipe: function GS_onSwipe(aEvent) {
    switch (aEvent.direction) {
      case SimpleGestureEvent.DIRECTION_LEFT:
        return this._doAction(aEvent, "swipe.left");
      case SimpleGestureEvent.DIRECTION_RIGHT:
        return this._doAction(aEvent, "swipe.right");
      case SimpleGestureEvent.DIRECTION_UP:
        return this._doAction(aEvent, "swipe.up");
      case SimpleGestureEvent.DIRECTION_DOWN:
        return this._doAction(aEvent, "swipe.down");
    }
  },

  // Keep track of offsets for continual motion events, e.g., zoom and rotate
  _lastOffset: 0,

  /**
   * Handle the beginning of a continual motion event
   *
   * @param aEvent
   *        The continual motion event
   */
  onStart: function GS_onStart(aEvent) {
    this._lastOffset = 0;
  },

  /**
   * Helper function to determine if a continual motion event has passed some
   * threshold and should trigger some action. If the action is triggered, the
   * tracking of the motion is reset as if a new motion has started.
   *
   * @param aEvent
   *        The continual motion event to handle
   * @param aThreshold
   *        Minimum positive/negative difference before the action is triggered
   * @param aInc
   *        Name of the gesture for increasing motion
   * @param aDec
   *        Name of the gesture for decreasing motion
   */
  _handleUpdate: function GS__handleUpdate(aEvent, aThreshold, aInc, aDec) {
    // Update the offset with new event data
    this._lastOffset += aEvent.delta;

    // Do the gesture action when we pass the threshold and then reset motion
    if (Math.abs(this._lastOffset) > aThreshold) {
      this._doAction(aEvent, this._lastOffset > 0 ? aInc : aDec);
      this.onStart(aEvent);
    }
  },
};

function BrowserStartup() {
  var uriToLoad = null;

  // window.arguments[0]: URI to load (string), or an nsISupportsArray of
  //                      nsISupportsStrings to load, or a xul:tab of
  //                      a tabbrowser, which will be replaced by this
  //                      window (for this case, all other arguments are
  //                      ignored).
  //                 [1]: character set (string)
  //                 [2]: referrer (nsIURI)
  //                 [3]: postData (nsIInputStream)
  //                 [4]: allowThirdPartyFixup (bool)
  if ("arguments" in window && window.arguments[0])
    uriToLoad = window.arguments[0];

  var isLoadingBlank = uriToLoad == "about:blank";
  var mustLoadSidebar = false;

  prepareForStartup();

#ifdef ENABLE_PAGE_CYCLER
  appCore.startPageCycler();
#else
# only load url passed in when we're not page cycling
  if (uriToLoad && !isLoadingBlank) { 
    if (uriToLoad instanceof Ci.nsISupportsArray) {
      let count = uriToLoad.Count();
      let specs = [];
      for (let i = 0; i < count; i++) {
        let urisstring = uriToLoad.GetElementAt(i).QueryInterface(Ci.nsISupportsString);
        specs.push(urisstring.data);
      }

      // This function throws for certain malformed URIs, so use exception handling
      // so that we don't disrupt startup
      try {
        gBrowser.loadTabs(specs, false, true);
      } catch (e) {}
    }
    else if (uriToLoad instanceof XULElement) {
      // swap the given tab with the default about:blank tab and then close
      // the original tab in the other window.

      // Stop the about:blank load
      gBrowser.selectedBrowser.stop();
      // make sure it has a docshell
      gBrowser.selectedBrowser.docShell;

      gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, uriToLoad);
    }
    else if (window.arguments.length >= 3) {
      loadURI(uriToLoad, window.arguments[2], window.arguments[3] || null,
              window.arguments[4] || false);
    }
    // Note: loadOneOrMoreURIs *must not* be called if window.arguments.length >= 3.
    // Such callers expect that window.arguments[0] is handled as a single URI.
    else
      loadOneOrMoreURIs(uriToLoad);
  }
#endif

  if (window.opener && !window.opener.closed) {
    let openerFindBar = window.opener.gFindBar;
    if (openerFindBar && !openerFindBar.hidden &&
        openerFindBar.findMode == gFindBar.FIND_NORMAL)
      gFindBar.open();

    let openerSidebarBox = window.opener.document.getElementById("sidebar-box");
    // If the opener had a sidebar, open the same sidebar in our window.
    // The opener can be the hidden window too, if we're coming from the state
    // where no windows are open, and the hidden window has no sidebar box.
    if (openerSidebarBox && !openerSidebarBox.hidden) {
      let sidebarBox = document.getElementById("sidebar-box");
      let sidebarTitle = document.getElementById("sidebar-title");
      sidebarTitle.setAttribute("value", window.opener.document.getElementById("sidebar-title").getAttribute("value"));
      sidebarBox.setAttribute("width", openerSidebarBox.boxObject.width);
      let sidebarCmd = openerSidebarBox.getAttribute("sidebarcommand");
      sidebarBox.setAttribute("sidebarcommand", sidebarCmd);
      // Note: we're setting 'src' on sidebarBox, which is a <vbox>, not on the
      // <browser id="sidebar">. This lets us delay the actual load until
      // delayedStartup().
      sidebarBox.setAttribute("src", window.opener.document.getElementById("sidebar").getAttribute("src"));
      mustLoadSidebar = true;

      sidebarBox.hidden = false;
      document.getElementById("sidebar-splitter").hidden = false;
      document.getElementById(sidebarCmd).setAttribute("checked", "true");
    }
  }
  else {
    let box = document.getElementById("sidebar-box");
    if (box.hasAttribute("sidebarcommand")) {
      let commandID = box.getAttribute("sidebarcommand");
      if (commandID) {
        let command = document.getElementById(commandID);
        if (command) {
          mustLoadSidebar = true;
          box.hidden = false;
          document.getElementById("sidebar-splitter").hidden = false;
          command.setAttribute("checked", "true");
        }
        else {
          // Remove the |sidebarcommand| attribute, because the element it 
          // refers to no longer exists, so we should assume this sidebar
          // panel has been uninstalled. (249883)
          box.removeAttribute("sidebarcommand");
        }
      }
    }
  }

  // Certain kinds of automigration rely on this notification to complete their
  // tasks BEFORE the browser window is shown.
  Cc["@mozilla.org/observer-service;1"]
    .getService(Ci.nsIObserverService)
    .notifyObservers(null, "browser-window-before-show", "");

  // Set a sane starting width/height for all resolutions on new profiles.
  if (!document.documentElement.hasAttribute("width")) {
    let defaultWidth = 994;
    let defaultHeight;
    if (screen.availHeight <= 600) {
      document.documentElement.setAttribute("sizemode", "maximized");
      defaultWidth = 610;
      defaultHeight = 450;
    }
    else {
      // Create a narrower window for large or wide-aspect displays, to suggest
      // side-by-side page view.
      if (screen.availWidth >= 1600)
        defaultWidth = (screen.availWidth / 2) - 20;
      defaultHeight = screen.availHeight - 10;
#ifdef MOZ_WIDGET_GTK2
      // On X, we're not currently able to account for the size of the window
      // border.  Use 28px as a guess (titlebar + bottom window border)
      defaultHeight -= 28;
#endif
    }
    document.documentElement.setAttribute("width", defaultWidth);
    document.documentElement.setAttribute("height", defaultHeight);
  }

  if (gURLBar && document.documentElement.getAttribute("chromehidden").indexOf("toolbar") != -1) {
    gURLBar.setAttribute("readonly", "true");
    gURLBar.setAttribute("enablehistory", "false");
  }

  setTimeout(delayedStartup, 0, isLoadingBlank, mustLoadSidebar);
}

function HandleAppCommandEvent(evt) {
  evt.stopPropagation();
  switch (evt.command) {
  case "Back":
    BrowserBack();
    break;
  case "Forward":
    BrowserForward();
    break;
  case "Reload":
    BrowserReloadSkipCache();
    break;
  case "Stop":
    BrowserStop();
    break;
  case "Search":
    BrowserSearch.webSearch();
    break;
  case "Bookmarks":
    toggleSidebar('viewBookmarksSidebar');
    break;
  case "Home":
    BrowserHome();
    break;
  default:
    break;
  }
}

function prepareForStartup() {
  gBrowser.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);
  // Note: we need to listen to untrusted events, because the pluginfinder XBL
  // binding can't fire trusted ones (runs with page privileges).
  gBrowser.addEventListener("PluginNotFound", gMissingPluginInstaller.newMissingPlugin, true, true);
  gBrowser.addEventListener("PluginBlocklisted", gMissingPluginInstaller.newMissingPlugin, true, true);
  gBrowser.addEventListener("PluginDisabled", gMissingPluginInstaller.newDisabledPlugin, true, true);
  gBrowser.addEventListener("NewPluginInstalled", gMissingPluginInstaller.refreshBrowser, false);
  gBrowser.addEventListener("NewTab", BrowserOpenTab, false);
  window.addEventListener("AppCommand", HandleAppCommandEvent, true);

  var webNavigation;
  try {
    // Create the browser instance component.
    appCore = Components.classes["@mozilla.org/appshell/component/browser/instance;1"]
                        .createInstance(Components.interfaces.nsIBrowserInstance);
    if (!appCore)
      throw "couldn't create a browser instance";

    webNavigation = getWebNavigation();
    if (!webNavigation)
      throw "no XBL binding for browser";
  } catch (e) {
    alert("Error launching browser window:" + e);
    window.close(); // Give up.
    return;
  }

  // initialize observers and listeners
  // and give C++ access to gBrowser
  XULBrowserWindow.init();
  window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(nsIWebNavigation)
        .QueryInterface(Ci.nsIDocShellTreeItem).treeOwner
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIXULWindow)
        .XULBrowserWindow = window.XULBrowserWindow;
  window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow =
    new nsBrowserAccess();

  // set default character set if provided
  if ("arguments" in window && window.arguments.length > 1 && window.arguments[1]) {
    if (window.arguments[1].indexOf("charset=") != -1) {
      var arrayArgComponents = window.arguments[1].split("=");
      if (arrayArgComponents) {
        //we should "inherit" the charset menu setting in a new window
        getMarkupDocumentViewer().defaultCharacterSet = arrayArgComponents[1];
      }
    }
  }

  // Initialize browser instance..
  appCore.setWebShellWindow(window);

  // Manually hook up session and global history for the first browser
  // so that we don't have to load global history before bringing up a
  // window.
  // Wire up session and global history before any possible
  // progress notifications for back/forward button updating
  webNavigation.sessionHistory = Components.classes["@mozilla.org/browser/shistory;1"]
                                           .createInstance(Components.interfaces.nsISHistory);
  var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  os.addObserver(gBrowser.browsers[0], "browser:purge-session-history", false);

  // remove the disablehistory attribute so the browser cleans up, as
  // though it had done this work itself
  gBrowser.browsers[0].removeAttribute("disablehistory");

  // enable global history
  gBrowser.docShell.QueryInterface(Components.interfaces.nsIDocShellHistory).useGlobalHistory = true;

  // hook up UI through progress listener
  gBrowser.addProgressListener(window.XULBrowserWindow, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
  gBrowser.addTabsProgressListener(window.TabsProgressListener);

  // setup our common DOMLinkAdded listener
  gBrowser.addEventListener("DOMLinkAdded", DOMLinkHandler, false);

  // setup simple gestures support
  gGestureSupport.init(true);
}

function delayedStartup(isLoadingBlank, mustLoadSidebar) {
  var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  os.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
  os.addObserver(gXPInstallObserver, "xpinstall-install-blocked", false);

  BrowserOffline.init();
  OfflineApps.init();

  gBrowser.addEventListener("pageshow", function(evt) { setTimeout(pageShowEventHandlers, 0, evt); }, true);

  // Ensure login manager is up and running.
  Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

  if (mustLoadSidebar) {
    let sidebar = document.getElementById("sidebar");
    let sidebarBox = document.getElementById("sidebar-box");
    sidebar.setAttribute("src", sidebarBox.getAttribute("src"));
  }

  UpdateUrlbarSearchSplitterState();
  
  PlacesStarButton.init();

  // called when we go into full screen, even if it is
  // initiated by a web page script
  window.addEventListener("fullscreen", onFullScreen, true);

  if (isLoadingBlank && gURLBar && isElementVisible(gURLBar))
    focusElement(gURLBar);
  else
    focusElement(content);

  if (gURLBar)
    gURLBar.setAttribute("emptytext", gURLBar.getAttribute("delayedemptytext"));

  gNavToolbox.customizeDone = BrowserToolboxCustomizeDone;
  gNavToolbox.customizeChange = BrowserToolboxCustomizeChange;

  // Set up Sanitize Item
  gSanitizeListener = new SanitizeListener();

  // Enable/Disable auto-hide tabbar
  gAutoHideTabbarPrefListener = new AutoHideTabbarPrefListener();
  gPrefService.addObserver(gAutoHideTabbarPrefListener.domain,
                           gAutoHideTabbarPrefListener, false);

  gPrefService.addObserver(gHomeButton.prefDomain, gHomeButton, false);

  var homeButton = document.getElementById("home-button");
  gHomeButton.updateTooltip(homeButton);
  gHomeButton.updatePersonalToolbarStyle(homeButton);

#ifdef HAVE_SHELL_SERVICE
  // Perform default browser checking (after window opens).
  var shell = getShellService();
  if (shell) {
    var shouldCheck = shell.shouldCheckDefaultBrowser;
    var willRecoverSession = false;
    try {
      var ss = Cc["@mozilla.org/browser/sessionstartup;1"].
               getService(Ci.nsISessionStartup);
      willRecoverSession =
        (ss.sessionType == Ci.nsISessionStartup.RECOVER_SESSION);
    }
    catch (ex) { /* never mind; suppose SessionStore is broken */ }
    if (shouldCheck && !shell.isDefaultBrowser(true) && !willRecoverSession) {
      var brandBundle = document.getElementById("bundle_brand");
      var shellBundle = document.getElementById("bundle_shell");

      var brandShortName = brandBundle.getString("brandShortName");
      var promptTitle = shellBundle.getString("setDefaultBrowserTitle");
      var promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage",
                                                         [brandShortName]);
      var checkboxLabel = shellBundle.getFormattedString("setDefaultBrowserDontAsk",
                                                         [brandShortName]);
      const IPS = Components.interfaces.nsIPromptService;
      var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                                .getService(IPS);
      var checkEveryTime = { value: shouldCheck };
      var rv = ps.confirmEx(window, promptTitle, promptMessage,
                            IPS.STD_YES_NO_BUTTONS,
                            null, null, null, checkboxLabel, checkEveryTime);
      if (rv == 0)
        shell.setDefaultBrowser(true, false);
      shell.shouldCheckDefaultBrowser = checkEveryTime.value;
    }
  }
#endif

  // BiDi UI
  gBidiUI = isBidiEnabled();
  if (gBidiUI) {
    document.getElementById("documentDirection-separator").hidden = false;
    document.getElementById("documentDirection-swap").hidden = false;
    document.getElementById("textfieldDirection-separator").hidden = false;
    document.getElementById("textfieldDirection-swap").hidden = false;
  }

#ifdef XP_MACOSX
  // Setup click-and-hold gestures access to the session history
  // menus if global click-and-hold isn't turned on
  if (!getBoolPref("ui.click_hold_context_menus", false))
    SetClickAndHoldHandlers();
#endif

  // Initialize the full zoom setting.
  // We do this before the session restore service gets initialized so we can
  // apply full zoom settings to tabs restored by the session restore service.
  try {
    FullZoom.init();
  }
  catch(ex) {
    Components.utils.reportError("Failed to init content pref service:\n" + ex);
  }

#ifdef XP_WIN
  // For Vista, flip the default download folder pref once from Desktop to Downloads
  // on new profiles.
  try {
    var sysInfo = Cc["@mozilla.org/system-info;1"].
                  getService(Ci.nsIPropertyBag2);
    if (parseFloat(sysInfo.getProperty("version")) >= 6 &&
        !gPrefService.getPrefType("browser.download.dir") &&
        gPrefService.getIntPref("browser.download.folderList") == 0) {
      var dnldMgr = Cc["@mozilla.org/download-manager;1"]
                              .getService(Ci.nsIDownloadManager);
      gPrefService.setCharPref("browser.download.dir", 
        dnldMgr.defaultDownloadsDirectory.path);
      gPrefService.setIntPref("browser.download.folderList", 1);
    }
  } catch (ex) {
  }
#endif

  // initialize the session-restore service (in case it's not already running)
  if (document.documentElement.getAttribute("windowtype") == "navigator:browser") {
    try {
      var ss = Cc["@mozilla.org/browser/sessionstore;1"].
               getService(Ci.nsISessionStore);
      ss.init(window);
    } catch(ex) {
      dump("nsSessionStore could not be initialized: " + ex + "\n");
    }
  }

  // bookmark-all-tabs command
  gBookmarkAllTabsHandler = new BookmarkAllTabsHandler();

  // Attach a listener to watch for "command" events bubbling up from error
  // pages.  This lets us fix bugs like 401575 which require error page UI to
  // do privileged things, without letting error pages have any privilege
  // themselves.
  gBrowser.addEventListener("command", BrowserOnCommand, false);

  tabPreviews.init();
  ctrlTab.init();

  // Initialize the microsummary service by retrieving it, prompting its factory
  // to create its singleton, whose constructor initializes the service.
  // Started 4 seconds after delayedStartup (before the livemarks service below).
  setTimeout(function() {
    try {
      Cc["@mozilla.org/microsummary/service;1"].getService(Ci.nsIMicrosummaryService);
    } catch (ex) {
      Components.utils.reportError("Failed to init microsummary service:\n" + ex);
    }
  }, 4000);

  // Delayed initialization of the livemarks update timer.
  // Livemark updates don't need to start until after bookmark UI 
  // such as the toolbar has initialized. Starting 5 seconds after
  // delayedStartup in order to stagger this after the microsummary
  // service (see above) and before the download manager starts (see below).
  setTimeout(function() PlacesUtils.livemarks.start(), 5000);

  // Initialize the download manager some time after the app starts so that
  // auto-resume downloads begin (such as after crashing or quitting with
  // active downloads) and speeds up the first-load of the download manager UI.
  // If the user manually opens the download manager before the timeout, the
  // downloads will start right away, and getting the service again won't hurt.
  setTimeout(function() {
    gDownloadMgr = Cc["@mozilla.org/download-manager;1"].
                   getService(Ci.nsIDownloadManager);

    // Initialize the downloads monitor panel listener
    DownloadMonitorPanel.init();
  }, 10000);

#ifndef XP_MACOSX
  updateEditUIVisibility();
  let placesContext = document.getElementById("placesContext");
  placesContext.addEventListener("popupshowing", updateEditUIVisibility, false);
  placesContext.addEventListener("popuphiding", updateEditUIVisibility, false);
#endif

  // initialize the private browsing UI
  gPrivateBrowsingUI.init();
}

function BrowserShutdown()
{
  tabPreviews.uninit();
  ctrlTab.uninit();

  gGestureSupport.init(false);

  try {
    FullZoom.destroy();
  }
  catch(ex) {
    Components.utils.reportError(ex);
  }

  var os = Components.classes["@mozilla.org/observer-service;1"]
    .getService(Components.interfaces.nsIObserverService);
  os.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
  os.removeObserver(gXPInstallObserver, "xpinstall-install-blocked");

  try {
    gBrowser.removeProgressListener(window.XULBrowserWindow);
    gBrowser.removeTabsProgressListener(window.TabsProgressListener);
  } catch (ex) {
  }

  PlacesStarButton.uninit();

  try {
    gPrefService.removeObserver(gAutoHideTabbarPrefListener.domain,
                                gAutoHideTabbarPrefListener);
    gPrefService.removeObserver(gHomeButton.prefDomain, gHomeButton);
  } catch (ex) {
    Components.utils.reportError(ex);
  }

  if (gSanitizeListener)
    gSanitizeListener.shutdown();

  BrowserOffline.uninit();
  OfflineApps.uninit();
  DownloadMonitorPanel.uninit();
  gPrivateBrowsingUI.uninit();

  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
  var enumerator = windowManagerInterface.getEnumerator(null);
  enumerator.getNext();
  if (!enumerator.hasMoreElements()) {
    document.persist("sidebar-box", "sidebarcommand");
    document.persist("sidebar-box", "width");
    document.persist("sidebar-box", "src");
    document.persist("sidebar-title", "value");
  }

  window.XULBrowserWindow.destroy();
  window.XULBrowserWindow = null;
  window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIWebNavigation)
        .QueryInterface(Components.interfaces.nsIDocShellTreeItem).treeOwner
        .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIXULWindow)
        .XULBrowserWindow = null;
  window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = null;

  // Close the app core.
  if (appCore)
    appCore.close();
}

#ifdef XP_MACOSX
// nonBrowserWindowStartup(), nonBrowserWindowDelayedStartup(), and
// nonBrowserWindowShutdown() are used for non-browser windows in
// macBrowserOverlay
function nonBrowserWindowStartup()
{
  // Disable inappropriate commands / submenus
  var disabledItems = ['Browser:SavePage',
                       'Browser:SendLink', 'cmd_pageSetup', 'cmd_print', 'cmd_find', 'cmd_findAgain',
                       'viewToolbarsMenu', 'cmd_toggleTaskbar', 'viewSidebarMenuMenu', 'Browser:Reload',
                       'viewFullZoomMenu', 'pageStyleMenu', 'charsetMenu', 'View:PageSource', 'View:FullScreen',
                       'viewHistorySidebar', 'Browser:AddBookmarkAs', 'View:PageInfo', 'Tasks:InspectPage'];
  var element;

  for (var id in disabledItems)
  {
    element = document.getElementById(disabledItems[id]);
    if (element)
      element.setAttribute("disabled", "true");
  }

  // If no windows are active (i.e. we're the hidden window), disable the close, minimize
  // and zoom menu commands as well
  if (window.location.href == "chrome://browser/content/hiddenWindow.xul")
  {
    var hiddenWindowDisabledItems = ['cmd_close', 'minimizeWindow', 'zoomWindow'];
    for (var id in hiddenWindowDisabledItems)
    {
      element = document.getElementById(hiddenWindowDisabledItems[id]);
      if (element)
        element.setAttribute("disabled", "true");
    }

    // also hide the window-list separator
    element = document.getElementById("sep-window-list");
    element.setAttribute("hidden", "true");
  }


  setTimeout(nonBrowserWindowDelayedStartup, 0);
}

function nonBrowserWindowDelayedStartup()
{
  // initialise the offline listener
  BrowserOffline.init();
  
  // Set up Sanitize Item
  gSanitizeListener = new SanitizeListener();
}

function nonBrowserWindowShutdown()
{
  if (gSanitizeListener)
    gSanitizeListener.shutdown();

  BrowserOffline.uninit();
}
#endif

function AutoHideTabbarPrefListener()
{
  this.toggleAutoHideTabbar();
}

AutoHideTabbarPrefListener.prototype =
{
  domain: "browser.tabs.autoHide",
  observe: function (aSubject, aTopic, aPrefName)
  {
    if (aTopic != "nsPref:changed" || aPrefName != this.domain)
      return;

    this.toggleAutoHideTabbar();
  },

  toggleAutoHideTabbar: function ()
  {
    if (gBrowser.tabContainer.childNodes.length == 1 &&
        window.toolbar.visible) {
      var aVisible = false;
      try {
        aVisible = !gPrefService.getBoolPref(this.domain);
      }
      catch (e) {
      }
      gBrowser.setStripVisibilityTo(aVisible);
    }
  }
}

function SanitizeListener()
{
  gPrefService.addObserver(this.promptDomain, this, false);

  this._defaultLabel = document.getElementById("sanitizeItem")
                               .getAttribute("label");
  this._updateSanitizeItem();

  if (gPrefService.prefHasUserValue(this.didSanitizeDomain)) {
    gPrefService.clearUserPref(this.didSanitizeDomain);
    // We need to persist this preference change, since we want to
    // check it at next app start even if the browser exits abruptly
    gPrefService.QueryInterface(Ci.nsIPrefService).savePrefFile(null);
  }
}

SanitizeListener.prototype =
{
  promptDomain      : "privacy.sanitize.promptOnSanitize",
  didSanitizeDomain : "privacy.sanitize.didShutdownSanitize",

  observe: function (aSubject, aTopic, aPrefName)
  {
    this._updateSanitizeItem();
  },

  shutdown: function ()
  {
    gPrefService.removeObserver(this.promptDomain, this);
  },

  _updateSanitizeItem: function ()
  {
    var label = gPrefService.getBoolPref(this.promptDomain) ?
        gNavigatorBundle.getString("sanitizeWithPromptLabel2") : 
        this._defaultLabel;
    document.getElementById("sanitizeItem").setAttribute("label", label);
  }
}

function gotoHistoryIndex(aEvent)
{
  var index = aEvent.target.getAttribute("index");
  if (!index)
    return false;

  var where = whereToOpenLink(aEvent);

  if (where == "current") {
    // Normal click.  Go there in the current tab and update session history.

    try {
      gBrowser.gotoIndex(index);
    }
    catch(ex) {
      return false;
    }
    return true;
  }
  else {
    // Modified click.  Go there in a new tab/window.
    // This code doesn't copy history or work well with framed pages.

    var sessionHistory = getWebNavigation().sessionHistory;
    var entry = sessionHistory.getEntryAtIndex(index, false);
    var url = entry.URI.spec;
    openUILinkIn(url, where);
    return true;
  }
}

function BrowserForward(aEvent) {
  var where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      gBrowser.goForward();
    }
    catch(ex) {
    }
  }
  else {
    var sessionHistory = getWebNavigation().sessionHistory;
    var currentIndex = sessionHistory.index;
    var entry = sessionHistory.getEntryAtIndex(currentIndex + 1, false);
    var url = entry.URI.spec;
    openUILinkIn(url, where);
  }
}

function BrowserBack(aEvent) {
  var where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      gBrowser.goBack();
    }
    catch(ex) {
    }
  }
  else {
    var sessionHistory = getWebNavigation().sessionHistory;
    var currentIndex = sessionHistory.index;
    var entry = sessionHistory.getEntryAtIndex(currentIndex - 1, false);
    var url = entry.URI.spec;
    openUILinkIn(url, where);
  }
}

function BrowserHandleBackspace()
{
  switch (gPrefService.getIntPref("browser.backspace_action")) {
  case 0:
    BrowserBack();
    break;
  case 1:
    goDoCommand("cmd_scrollPageUp");
    break;
  }
}

function BrowserHandleShiftBackspace()
{
  switch (gPrefService.getIntPref("browser.backspace_action")) {
  case 0:
    BrowserForward();
    break;
  case 1:
    goDoCommand("cmd_scrollPageDown");
    break;
  }
}

function BrowserStop()
{
  try {
    const stopFlags = nsIWebNavigation.STOP_ALL;
    getWebNavigation().stop(stopFlags);
  }
  catch(ex) {
  }
}

function BrowserReloadOrDuplicate(aEvent) {
  var backgroundTabModifier = aEvent.button == 1 ||
#ifdef XP_MACOSX
    aEvent.metaKey;
#else
    aEvent.ctrlKey;
#endif
  if (aEvent.shiftKey && !backgroundTabModifier) {
    BrowserReloadSkipCache();
    return;
  }

  var where = whereToOpenLink(aEvent, false, true);
  if (where == "current")
    BrowserReload();
  else
    openUILinkIn(getWebNavigation().currentURI.spec, where);
}

function BrowserReload() {
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_NONE;
  BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadSkipCache() {
  // Bypass proxy and cache.
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  BrowserReloadWithFlags(reloadFlags);
}

function BrowserHome()
{
  var homePage = gHomeButton.getHomePage();
  loadOneOrMoreURIs(homePage);
}

function BrowserGoHome(aEvent) {
  if (aEvent && "button" in aEvent &&
      aEvent.button == 2) // right-click: do nothing
    return;

  var homePage = gHomeButton.getHomePage();
  var where = whereToOpenLink(aEvent, false, true);
  var urls;

  // openUILinkIn in utilityOverlay.js doesn't handle loading multiple pages
  switch (where) {
  case "current":
    loadOneOrMoreURIs(homePage);
    break;
  case "tabshifted":
  case "tab":
    urls = homePage.split("|");
    var loadInBackground = getBoolPref("browser.tabs.loadBookmarksInBackground", false);
    gBrowser.loadTabs(urls, loadInBackground);
    break;
  case "window":
    OpenBrowserWindow();
    break;
  }
}

function loadOneOrMoreURIs(aURIString)
{
#ifdef XP_MACOSX
  // we're not a browser window, pass the URI string to a new browser window
  if (window.location.href != getBrowserURL())
  {
    window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", aURIString);
    return;
  }
#endif
  // This function throws for certain malformed URIs, so use exception handling
  // so that we don't disrupt startup
  try {
    gBrowser.loadTabs(aURIString.split("|"), false, true);
  } 
  catch (e) {
  }
}

function focusAndSelectUrlBar()
{
  if (gURLBar && isElementVisible(gURLBar) && !gURLBar.readOnly) {
    gURLBar.focus();
    gURLBar.select();
    return true;
  }
  return false;
}

function openLocation()
{
  if (window.fullScreen)
    FullScreen.mouseoverToggle(true);

  if (focusAndSelectUrlBar())
    return;
#ifdef XP_MACOSX
  if (window.location.href != getBrowserURL()) {
    var win = getTopWin();
    if (win) {
      // If there's an open browser window, it should handle this command
      win.focus()
      win.openLocation();
    }
    else {
      // If there are no open browser windows, open a new one
      win = window.openDialog("chrome://browser/content/", "_blank",
                              "chrome,all,dialog=no", "about:blank");
      win.addEventListener("load", openLocationCallback, false);
    }
    return;
  }
#endif
  openDialog("chrome://browser/content/openLocation.xul", "_blank",
             "chrome,modal,titlebar", window);
}

function openLocationCallback()
{
  // make sure the DOM is ready
  setTimeout(function() { this.openLocation(); }, 0);
}

function BrowserOpenTab()
{
  if (!gBrowser) {
    // If there are no open browser windows, open a new one
    window.openDialog("chrome://browser/content/", "_blank",
                      "chrome,all,dialog=no", "about:blank");
    return;
  }
  gBrowser.loadOneTab("about:blank", null, null, null, false, false);
  if (gURLBar)
    gURLBar.focus();
}

/* Called from the openLocation dialog. This allows that dialog to instruct
   its opener to open a new window and then step completely out of the way.
   Anything less byzantine is causing horrible crashes, rather believably,
   though oddly only on Linux. */
function delayedOpenWindow(chrome, flags, href, postData)
{
  // The other way to use setTimeout,
  // setTimeout(openDialog, 10, chrome, "_blank", flags, url),
  // doesn't work here.  The extra "magic" extra argument setTimeout adds to
  // the callback function would confuse prepareForStartup() by making
  // window.arguments[1] be an integer instead of null.
  setTimeout(function() { openDialog(chrome, "_blank", flags, href, null, null, postData); }, 10);
}

/* Required because the tab needs time to set up its content viewers and get the load of
   the URI kicked off before becoming the active content area. */
function delayedOpenTab(aUrl, aReferrer, aCharset, aPostData, aAllowThirdPartyFixup)
{
  gBrowser.loadOneTab(aUrl, aReferrer, aCharset, aPostData, false, aAllowThirdPartyFixup);
}

function BrowserOpenFileWindow()
{
  // Get filepicker component.
  try {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, gNavigatorBundle.getString("openFile"), nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText | nsIFilePicker.filterImages |
                     nsIFilePicker.filterXML | nsIFilePicker.filterHTML);

    if (fp.show() == nsIFilePicker.returnOK)
      openTopWin(fp.fileURL.spec);
  } catch (ex) {
  }
}

function BrowserCloseTabOrWindow() {
#ifdef XP_MACOSX
  // If we're not a browser window, just close the window
  if (window.location.href != getBrowserURL()) {
    closeWindow(true);
    return;
  }
#endif

  // If the current tab is the last one, this will close the window.
  gBrowser.removeCurrentTab();
}

function BrowserTryToCloseWindow()
{
  if (WindowIsClosing()) {
    if (window.fullScreen) {
      gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                   FullScreen._collapseCallback, false);
      document.removeEventListener("keypress", FullScreen._keyToggleCallback, false);
      document.removeEventListener("popupshown", FullScreen._setPopupOpen, false);
      document.removeEventListener("popuphidden", FullScreen._setPopupOpen, false);
      gPrefService.removeObserver("browser.fullscreen", FullScreen);

      var fullScrToggler = document.getElementById("fullscr-toggler");
      if (fullScrToggler) {
        fullScrToggler.removeEventListener("mouseover", FullScreen._expandCallback, false);
        fullScrToggler.removeEventListener("dragenter", FullScreen._expandCallback, false);
      }
    }

    window.close();     // WindowIsClosing does all the necessary checks
  }
}

function loadURI(uri, referrer, postData, allowThirdPartyFixup)
{
  try {
    if (postData === undefined)
      postData = null;
    var flags = nsIWebNavigation.LOAD_FLAGS_NONE;
    if (allowThirdPartyFixup) {
      flags = nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    }
    gBrowser.loadURIWithFlags(uri, flags, referrer, null, postData);
  } catch (e) {
  }
}

function getShortcutOrURI(aURL, aPostDataRef) {
  var shortcutURL = null;
  var keyword = aURL;
  var param = "";
  var searchService = Cc["@mozilla.org/browser/search-service;1"].
                      getService(Ci.nsIBrowserSearchService);

  var offset = aURL.indexOf(" ");
  if (offset > 0) {
    keyword = aURL.substr(0, offset);
    param = aURL.substr(offset + 1);
  }

  if (!aPostDataRef)
    aPostDataRef = {};

  var engine = searchService.getEngineByAlias(keyword);
  if (engine) {
    var submission = engine.getSubmission(param, null);
    aPostDataRef.value = submission.postData;
    return submission.uri.spec;
  }

  [shortcutURL, aPostDataRef.value] =
    PlacesUtils.getURLAndPostDataForKeyword(keyword);

  if (!shortcutURL)
    return aURL;

  var postData = "";
  if (aPostDataRef.value)
    postData = unescape(aPostDataRef.value);

  if (/%s/i.test(shortcutURL) || /%s/i.test(postData)) {
    var charset = "";
    const re = /^(.*)\&mozcharset=([a-zA-Z][_\-a-zA-Z0-9]+)\s*$/;
    var matches = shortcutURL.match(re);
    if (matches)
      [, shortcutURL, charset] = matches;
    else {
      // Try to get the saved character-set.
      try {
        // makeURI throws if URI is invalid.
        // Will return an empty string if character-set is not found.
        charset = PlacesUtils.history.getCharsetForURI(makeURI(shortcutURL));
      } catch (e) {}
    }

    var encodedParam = "";
    if (charset)
      encodedParam = escape(convertFromUnicode(charset, param));
    else // Default charset is UTF-8
      encodedParam = encodeURIComponent(param);

    shortcutURL = shortcutURL.replace(/%s/g, encodedParam).replace(/%S/g, param);

    if (/%s/i.test(postData)) // POST keyword
      aPostDataRef.value = getPostDataStream(postData, param, encodedParam,
                                             "application/x-www-form-urlencoded");
  }
  else if (param) {
    // This keyword doesn't take a parameter, but one was provided. Just return
    // the original URL.
    aPostDataRef.value = null;

    return aURL;
  }

  return shortcutURL;
}
 
function getPostDataStream(aStringData, aKeyword, aEncKeyword, aType) {
  var dataStream = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
  aStringData = aStringData.replace(/%s/g, aEncKeyword).replace(/%S/g, aKeyword);
  dataStream.data = aStringData;
 
  var mimeStream = Cc["@mozilla.org/network/mime-input-stream;1"].
                   createInstance(Ci.nsIMIMEInputStream);
  mimeStream.addHeader("Content-Type", aType);
  mimeStream.addContentLength = true;
  mimeStream.setData(dataStream);
  return mimeStream.QueryInterface(Ci.nsIInputStream);
}

function readFromClipboard()
{
  var url;

  try {
    // Get clipboard.
    var clipboard = Components.classes["@mozilla.org/widget/clipboard;1"]
                              .getService(Components.interfaces.nsIClipboard);

    // Create tranferable that will transfer the text.
    var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);

    trans.addDataFlavor("text/unicode");

    // If available, use selection clipboard, otherwise global one
    if (clipboard.supportsSelectionClipboard())
      clipboard.getData(trans, clipboard.kSelectionClipboard);
    else
      clipboard.getData(trans, clipboard.kGlobalClipboard);

    var data = {};
    var dataLen = {};
    trans.getTransferData("text/unicode", data, dataLen);

    if (data) {
      data = data.value.QueryInterface(Components.interfaces.nsISupportsString);
      url = data.data.substring(0, dataLen.value / 2);
    }
  } catch (ex) {
  }

  return url;
}

function BrowserViewSourceOfDocument(aDocument)
{
  var pageCookie;
  var webNav;

  // Get the document charset
  var docCharset = "charset=" + aDocument.characterSet;

  // Get the nsIWebNavigation associated with the document
  try {
      var win;
      var ifRequestor;

      // Get the DOMWindow for the requested document.  If the DOMWindow
      // cannot be found, then just use the content window...
      //
      // XXX:  This is a bit of a hack...
      win = aDocument.defaultView;
      if (win == window) {
        win = content;
      }
      ifRequestor = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor);

      webNav = ifRequestor.getInterface(nsIWebNavigation);
  } catch(err) {
      // If nsIWebNavigation cannot be found, just get the one for the whole
      // window...
      webNav = getWebNavigation();
  }
  //
  // Get the 'PageDescriptor' for the current document. This allows the
  // view-source to access the cached copy of the content rather than
  // refetching it from the network...
  //
  try{
    var PageLoader = webNav.QueryInterface(Components.interfaces.nsIWebPageDescriptor);

    pageCookie = PageLoader.currentDescriptor;
  } catch(err) {
    // If no page descriptor is available, just use the view-source URL...
  }

  top.gViewSourceUtils.viewSource(webNav.currentURI.spec, pageCookie, aDocument);
}

// doc - document to use for source, or null for this window's document
// initialTab - name of the initial tab to display, or null for the first tab
function BrowserPageInfo(doc, initialTab)
{
  var args = {doc: doc, initialTab: initialTab};
  return toOpenDialogByTypeAndUrl("Browser:page-info",
                                  doc ? doc.location : window.content.document.location,
                                  "chrome://browser/content/pageinfo/pageInfo.xul",
                                  "chrome,toolbar,dialog=no,resizable",
                                  args);
}

#ifdef DEBUG
// Initialize the LeakDetector class.
function LeakDetector(verbose)
{
  this.verbose = verbose;
}

const NS_LEAKDETECTOR_CONTRACTID = "@mozilla.org/xpcom/leakdetector;1";

if (NS_LEAKDETECTOR_CONTRACTID in Components.classes) {
  try {
    LeakDetector.prototype = Components.classes[NS_LEAKDETECTOR_CONTRACTID]
                                       .createInstance(Components.interfaces.nsILeakDetector);
  } catch (err) {
    LeakDetector.prototype = Object.prototype;
  }
} else {
  LeakDetector.prototype = Object.prototype;
}

var leakDetector = new LeakDetector(false);

// Dumps current set of memory leaks.
function dumpMemoryLeaks()
{
  leakDetector.dumpLeaks();
}

// Traces all objects reachable from the chrome document.
function traceChrome()
{
  leakDetector.traceObject(document, leakDetector.verbose);
}

// Traces all objects reachable from the content document.
function traceDocument()
{
  // keep the chrome document out of the dump.
  leakDetector.markObject(document, true);
  leakDetector.traceObject(content, leakDetector.verbose);
  leakDetector.markObject(document, false);
}

// Controls whether or not we do verbose tracing.
function traceVerbose(verbose)
{
  leakDetector.verbose = (verbose == "true");
}
#endif

function URLBarSetURI(aURI, aValid) {
  var value = gBrowser.userTypedValue;
  var valid = false;

  if (!value) {
    let uri = aURI || getWebNavigation().currentURI;

    // Replace "about:blank" with an empty string
    // only if there's no opener (bug 370555).
    if (uri.spec == "about:blank")
      value = content.opener ? "about:blank" : "";
    else
      value = losslessDecodeURI(uri);

    valid = value && (!aURI || aValid);
  }

  gURLBar.value = value;
  SetPageProxyState(valid ? "valid" : "invalid");
}

function losslessDecodeURI(aURI) {
  var value = aURI.spec;
  // Try to decode as UTF-8 if there's no encoding sequence that we would break.
  if (!/%25(?:3B|2F|3F|3A|40|26|3D|2B|24|2C|23)/i.test(value))
    try {
      value = decodeURI(value)
                // 1. decodeURI decodes %25 to %, which creates unintended
                //    encoding sequences. Re-encode it, unless it's part of
                //    a sequence that survived decodeURI, i.e. one for:
                //    ';', '/', '?', ':', '@', '&', '=', '+', '$', ',', '#'
                //    (RFC 3987 section 3.2)
                // 2. Re-encode whitespace so that it doesn't get eaten away
                //    by the location bar (bug 410726).
                .replace(/%(?!3B|2F|3F|3A|40|26|3D|2B|24|2C|23)|[\r\n\t]/ig,
                         encodeURIComponent);
    } catch (e) {}

  // Encode invisible characters (invisible control characters, soft hyphen,
  // zero-width space, BOM, line separator, paragraph separator) (bug 452979)
  value = value.replace(/[\v\x0c\x1c\x1d\x1e\x1f\u00ad\u200b\ufeff\u2028\u2029]/g,
                        encodeURIComponent);

  // Encode bidirectional formatting characters.
  // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
  value = value.replace(/[\u200e\u200f\u202a\u202b\u202c\u202d\u202e]/g,
                        encodeURIComponent);
  return value;
}

function UpdateUrlbarSearchSplitterState()
{
  var splitter = document.getElementById("urlbar-search-splitter");
  var urlbar = document.getElementById("urlbar-container");
  var searchbar = document.getElementById("search-container");

  var ibefore = null;
  if (urlbar && searchbar) {
    if (urlbar.nextSibling == searchbar)
      ibefore = searchbar;
    else if (searchbar.nextSibling == urlbar)
      ibefore = urlbar;
  }

  if (ibefore) {
    if (!splitter) {
      splitter = document.createElement("splitter");
      splitter.id = "urlbar-search-splitter";
      splitter.setAttribute("resizebefore", "flex");
      splitter.setAttribute("resizeafter", "flex");
      splitter.className = "chromeclass-toolbar-additional";
    }
    urlbar.parentNode.insertBefore(splitter, ibefore);
  } else if (splitter)
    splitter.parentNode.removeChild(splitter);
}

var LocationBarHelpers = {
  _timeoutID: null,

  _searchBegin: function LocBar_searchBegin() {
    function delayedBegin(self) {
      self._timeoutID = null;
      document.getElementById("urlbar-throbber").setAttribute("busy", "true");
    }

    this._timeoutID = setTimeout(delayedBegin, 500, this);
  },

  _searchComplete: function LocBar_searchComplete() {
    // Did we finish the search before delayedBegin was invoked?
    if (this._timeoutID) {
      clearTimeout(this._timeoutID);
      this._timeoutID = null;
    }
    document.getElementById("urlbar-throbber").removeAttribute("busy");
  }
};

function UpdatePageProxyState()
{
  if (gURLBar && gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid");
}

function SetPageProxyState(aState)
{
  if (!gURLBar)
    return;

  if (!gProxyFavIcon)
    gProxyFavIcon = document.getElementById("page-proxy-favicon");

  gURLBar.setAttribute("pageproxystate", aState);
  gProxyFavIcon.setAttribute("pageproxystate", aState);

  // the page proxy state is set to valid via OnLocationChange, which
  // gets called when we switch tabs.
  if (aState == "valid") {
    gLastValidURLStr = gURLBar.value;
    gURLBar.addEventListener("input", UpdatePageProxyState, false);

    PageProxySetIcon(gBrowser.mCurrentBrowser.mIconURL);
  } else if (aState == "invalid") {
    gURLBar.removeEventListener("input", UpdatePageProxyState, false);
    PageProxyClearIcon();
  }
}

function PageProxySetIcon (aURL)
{
  if (!gProxyFavIcon)
    return;

  if (!aURL)
    PageProxyClearIcon();
  else if (gProxyFavIcon.getAttribute("src") != aURL)
    gProxyFavIcon.setAttribute("src", aURL);
}

function PageProxyClearIcon ()
{
  gProxyFavIcon.removeAttribute("src");
}


function PageProxyDragGesture(aEvent)
{
  if (gProxyFavIcon.getAttribute("pageproxystate") == "valid") {
    nsDragAndDrop.startDrag(aEvent, proxyIconDNDObserver);
    return true;
  }
  return false;
}

function PageProxyClickHandler(aEvent)
{
  if (aEvent.button == 1 && gPrefService.getBoolPref("middlemouse.paste"))
    middleMousePaste(aEvent);
}

function BrowserImport()
{
#ifdef XP_MACOSX
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var win = wm.getMostRecentWindow("Browser:MigrationWizard");
  if (win)
    win.focus();
  else {
    window.openDialog("chrome://browser/content/migration/migration.xul",
                      "migration", "centerscreen,chrome,resizable=no");
  }
#else
  window.openDialog("chrome://browser/content/migration/migration.xul",
                    "migration", "modal,centerscreen,chrome,resizable=no");
#endif
}

/**
 * Handle command events bubbling up from error page content
 */
function BrowserOnCommand(event) {
    // Don't trust synthetic events
    if (!event.isTrusted)
      return;

    var ot = event.originalTarget;
    var errorDoc = ot.ownerDocument;

    // If the event came from an ssl error page, it is probably either the "Add
    // Exception…" or "Get me out of here!" button
    if (/^about:certerror/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById('exceptionDialogButton')) {
        var params = { exceptionAdded : false };
        
        try {
          switch (gPrefService.getIntPref("browser.ssl_override_behavior")) {
            case 2 : // Pre-fetch & pre-populate
              params.prefetchCert = true;
            case 1 : // Pre-populate
              params.location = errorDoc.location.href;
          }
        } catch (e) {
          Components.utils.reportError("Couldn't get ssl_override pref: " + e);
        }
        
        window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                          '','chrome,centerscreen,modal', params);
        
        // If the user added the exception cert, attempt to reload the page
        if (params.exceptionAdded)
          errorDoc.location.reload();
      }
      else if (ot == errorDoc.getElementById('getMeOutOfHereButton')) {
        getMeOutOfHere();
      }
    }
    else if (/^about:blocked/.test(errorDoc.documentURI)) {
      // The event came from a button on a malware/phishing block page
      // First check whether it's malware or phishing, so that we can
      // use the right strings/links
      var isMalware = /e=malwareBlocked/.test(errorDoc.documentURI);
      
      if (ot == errorDoc.getElementById('getMeOutButton')) {
        getMeOutOfHere();
      }
      else if (ot == errorDoc.getElementById('reportButton')) {
        // This is the "Why is this site blocked" button.  For malware,
        // we can fetch a site-specific report, for phishing, we redirect
        // to the generic page describing phishing protection.
        var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                       .getService(Components.interfaces.nsIURLFormatter);
        
        if (isMalware) {
          // Get the stop badware "why is this blocked" report url,
          // append the current url, and go there.
          try {
            var reportURL = formatter.formatURLPref("browser.safebrowsing.malware.reportURL");
            reportURL += errorDoc.location.href;
            content.location = reportURL;
          } catch (e) {
            Components.utils.reportError("Couldn't get malware report URL: " + e);
          }
        }
        else { // It's a phishing site, not malware
          try {
            content.location = formatter.formatURLPref("browser.safebrowsing.warning.infoURL");
          } catch (e) {
            Components.utils.reportError("Couldn't get phishing info URL: " + e);
          }
        }
      }
      else if (ot == errorDoc.getElementById('ignoreWarningButton')) {
        // Allow users to override and continue through to the site,
        // but add a notify bar as a reminder, so that they don't lose
        // track after, e.g., tab switching.
        gBrowser.loadURIWithFlags(content.location.href,
                                  nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
                                  null, null, null);
        var buttons = [{
          label: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.label"),
          accessKey: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.accessKey"),
          callback: function() { getMeOutOfHere(); }
        }];
        
        if (isMalware) {
          var title = gNavigatorBundle.getString("safebrowsing.reportedAttackSite");
          buttons[1] = {
            label: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.label"),
            accessKey: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.accessKey"),
            callback: function() {
              openUILinkIn(safebrowsing.getReportURL('MalwareError'), 'tab');
            }
          };
        } else {
          title = gNavigatorBundle.getString("safebrowsing.reportedWebForgery");
          buttons[1] = {
            label: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.label"),
            accessKey: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.accessKey"),
            callback: function() {
              openUILinkIn(safebrowsing.getReportURL('Error'), 'tab');
            }
          };
        }
        
        var notificationBox = gBrowser.getNotificationBox();
        notificationBox.appendNotification(
          title,
          "blocked-badware-page",
          "chrome://global/skin/icons/blacklist_favicon.png",
          notificationBox.PRIORITY_CRITICAL_HIGH,
          buttons
        );
      }
    }
}

/**
 * Re-direct the browser to a known-safe page.  This function is
 * used when, for example, the user browses to a known malware page
 * and is presented with about:blocked.  The "Get me out of here!"
 * button should take the user to the default start page so that even
 * when their own homepage is infected, we can get them somewhere safe.
 */
function getMeOutOfHere() {
  // Get the start page from the *default* pref branch, not the user's
  var prefs = Cc["@mozilla.org/preferences-service;1"]
             .getService(Ci.nsIPrefService).getDefaultBranch(null);
  var url = "about:blank";
  try {
    url = prefs.getComplexValue("browser.startup.homepage",
                                Ci.nsIPrefLocalizedString).data;
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.indexOf("|") != -1)
      url = url.split("|")[0];
  } catch(e) {
    Components.utils.reportError("Couldn't get homepage pref: " + e);
  }
  content.location = url;
}

function BrowserFullScreen()
{
  window.fullScreen = !window.fullScreen;
}

function onFullScreen()
{
  FullScreen.toggle();
}

function getWebNavigation()
{
  try {
    return gBrowser.webNavigation;
  } catch (e) {
    return null;
  }
}

function BrowserReloadWithFlags(reloadFlags) {
  /* First, we'll try to use the session history object to reload so
   * that framesets are handled properly. If we're in a special
   * window (such as view-source) that has no session history, fall
   * back on using the web navigation's reload method.
   */

  var webNav = getWebNavigation();
  try {
    var sh = webNav.sessionHistory;
    if (sh)
      webNav = sh.QueryInterface(nsIWebNavigation);
  } catch (e) {
  }

  try {
    webNav.reload(reloadFlags);
  } catch (e) {
  }
}

function toggleAffectedChrome(aHide)
{
  // chrome to toggle includes:
  //   (*) menubar
  //   (*) navigation bar
  //   (*) bookmarks toolbar
  //   (*) tabstrip
  //   (*) browser messages
  //   (*) sidebar
  //   (*) find bar
  //   (*) statusbar

  gNavToolbox.hidden = aHide;
  if (aHide)
  {
    gChromeState = {};
    var sidebar = document.getElementById("sidebar-box");
    gChromeState.sidebarOpen = !sidebar.hidden;
    gSidebarCommand = sidebar.getAttribute("sidebarcommand");

    gChromeState.hadTabStrip = gBrowser.getStripVisibility();
    gBrowser.setStripVisibilityTo(false);

    var notificationBox = gBrowser.getNotificationBox();
    gChromeState.notificationsOpen = !notificationBox.notificationsHidden;
    notificationBox.notificationsHidden = aHide;

    document.getElementById("sidebar").setAttribute("src", "about:blank");
    var statusbar = document.getElementById("status-bar");
    gChromeState.statusbarOpen = !statusbar.hidden;
    statusbar.hidden = aHide;

    gChromeState.findOpen = !gFindBar.hidden;
    gFindBar.close();
  }
  else {
    if (gChromeState.hadTabStrip) {
      gBrowser.setStripVisibilityTo(true);
    }

    if (gChromeState.notificationsOpen) {
      gBrowser.getNotificationBox().notificationsHidden = aHide;
    }

    if (gChromeState.statusbarOpen) {
      var statusbar = document.getElementById("status-bar");
      statusbar.hidden = aHide;
    }

    if (gChromeState.findOpen)
      gFindBar.open();
  }

  if (gChromeState.sidebarOpen)
    toggleSidebar(gSidebarCommand);
}

function onEnterPrintPreview()
{
  gInPrintPreviewMode = true;
  toggleAffectedChrome(true);
}

function onExitPrintPreview()
{
  // restore chrome to original state
  gInPrintPreviewMode = false;
  FullZoom.setSettingValue();
  toggleAffectedChrome(false);
}

function getPPBrowser()
{
  return gBrowser;
}

function getMarkupDocumentViewer()
{
  return gBrowser.markupDocumentViewer;
}

/**
 * Content area tooltip.
 * XXX - this must move into XBL binding/equiv! Do not want to pollute
 *       browser.js with functionality that can be encapsulated into
 *       browser widget. TEMPORARY!
 *
 * NOTE: Any changes to this routine need to be mirrored in ChromeListener::FindTitleText()
 *       (located in mozilla/embedding/browser/webBrowser/nsDocShellTreeOwner.cpp)
 *       which performs the same function, but for embedded clients that
 *       don't use a XUL/JS layer. It is important that the logic of
 *       these two routines be kept more or less in sync.
 *       (pinkerton)
 **/
function FillInHTMLTooltip(tipElement)
{
  var retVal = false;
  if (tipElement.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")
    return retVal;

  const XLinkNS = "http://www.w3.org/1999/xlink";


  var titleText = null;
  var XLinkTitleText = null;
  var direction = tipElement.ownerDocument.dir;

  while (!titleText && !XLinkTitleText && tipElement) {
    if (tipElement.nodeType == Node.ELEMENT_NODE) {
      titleText = tipElement.getAttribute("title");
      XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
      var defView = tipElement.ownerDocument.defaultView;
      // XXX Work around bug 350679:
      // "Tooltips can be fired in documents with no view".
      if (!defView)
        return retVal;
      direction = defView.getComputedStyle(tipElement, "")
        .getPropertyValue("direction");
    }
    tipElement = tipElement.parentNode;
  }

  var tipNode = document.getElementById("aHTMLTooltip");
  tipNode.style.direction = direction;
  
  for each (var t in [titleText, XLinkTitleText]) {
    if (t && /\S/.test(t)) {

      // Per HTML 4.01 6.2 (CDATA section), literal CRs and tabs should be
      // replaced with spaces, and LFs should be removed entirely.
      // XXX Bug 322270: We don't preserve the result of entities like &#13;,
      // which should result in a line break in the tooltip, because we can't
      // distinguish that from a literal character in the source by this point.
      t = t.replace(/[\r\t]/g, ' ');
      t = t.replace(/\n/g, '');

      tipNode.setAttribute("label", t);
      retVal = true;
    }
  }

  return retVal;
}

var proxyIconDNDObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var value = content.location.href;
      var urlString = value + "\n" + content.document.title;
      var htmlString = "<a href=\"" + value + "\">" + value + "</a>";

      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/unicode", value);
      aXferData.data.addDataForFlavour("text/html", htmlString);

      // we're copying the URL from the proxy icon, not moving
      // we specify all of them though, because d&d sucks and OS's
      // get confused if they don't get the one they want
      aDragAction.action =
        Components.interfaces.nsIDragService.DRAGDROP_ACTION_COPY |
        Components.interfaces.nsIDragService.DRAGDROP_ACTION_MOVE |
        Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    }
}

var homeButtonObserver = {
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
      setTimeout(openHomeDialog, 0, url);
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = gNavigatorBundle.getString("droponhomebutton");
      aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    },

  onDragExit: function (aEvent, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = "";
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
}

function openHomeDialog(aURL)
{
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  var promptTitle = gNavigatorBundle.getString("droponhometitle");
  var promptMsg   = gNavigatorBundle.getString("droponhomemsg");
  var pressedVal  = promptService.confirmEx(window, promptTitle, promptMsg,
                          promptService.STD_YES_NO_BUTTONS,
                          null, null, null, null, {value:0});

  if (pressedVal == 0) {
    try {
      var str = Components.classes["@mozilla.org/supports-string;1"]
                          .createInstance(Components.interfaces.nsISupportsString);
      str.data = aURL;
      gPrefService.setComplexValue("browser.startup.homepage",
                                   Components.interfaces.nsISupportsString, str);
      var homeButton = document.getElementById("home-button");
      homeButton.setAttribute("tooltiptext", aURL);
    } catch (ex) {
      dump("Failed to set the home page.\n"+ex+"\n");
    }
  }
}

var bookmarksButtonObserver = {
  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var split = aXferData.data.split("\n");
    var url = split[0];
    if (url != aXferData.data)  // do nothing if it's not a valid URL
      PlacesUIUtils.showMinimalAddBookmarkUI(makeURI(url), split[1]);
  },

  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    var statusTextFld = document.getElementById("statusbar-display");
    statusTextFld.label = gNavigatorBundle.getString("droponbookmarksbutton");
    aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
  },

  onDragExit: function (aEvent, aDragSession)
  {
    var statusTextFld = document.getElementById("statusbar-display");
    statusTextFld.label = "";
  },

  getSupportedFlavours: function ()
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
    flavourSet.appendFlavour("text/x-moz-url");
    flavourSet.appendFlavour("text/unicode");
    return flavourSet;
  }
}

var newWindowButtonObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = gNavigatorBundle.getString("droponnewwindowbutton");
      aEvent.target.setAttribute("dragover", "true");
      return true;
    },
  onDragExit: function (aEvent, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = "";
      aEvent.target.removeAttribute("dragover");
    },
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var xferData = aXferData.data.split("\n");
      var draggedText = xferData[0] || xferData[1];
      var postData = {};
      var url = getShortcutOrURI(draggedText, postData);
      if (url) {
        nsDragAndDrop.dragDropSecurityCheck(aEvent, aDragSession, url);
        // allow third-party services to fixup this URL
        openNewWindowWith(url, null, postData.value, true);
      }
    },
  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("text/unicode");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    }
}

var DownloadsButtonDNDObserver = {
  /////////////////////////////////////////////////////////////////////////////
  // nsDragAndDrop
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    var statusTextFld = document.getElementById("statusbar-display");
    statusTextFld.label = gNavigatorBundle.getString("dropondownloadsbutton");
    aDragSession.canDrop = (aFlavour.contentType == "text/x-moz-url" ||
                            aFlavour.contentType == "text/unicode");
  },

  onDragExit: function (aEvent, aDragSession)
  {
    var statusTextFld = document.getElementById("statusbar-display");
    statusTextFld.label = "";
  },

  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var split = aXferData.data.split("\n");
    var url = split[0];
    if (url != aXferData.data) {  //do nothing, not a valid URL
      nsDragAndDrop.dragDropSecurityCheck(aEvent, aDragSession, url);

      var name = split[1];
      saveURL(url, name, null, true, true);
    }
  },
  getSupportedFlavours: function ()
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("text/x-moz-url");
    flavourSet.appendFlavour("text/unicode");
    return flavourSet;
  }
}

const DOMLinkHandler = {
  handleEvent: function (event) {
    switch (event.type) {
      case "DOMLinkAdded":
        this.onLinkAdded(event);
        break;
    }
  },
  onLinkAdded: function (event) {
    var link = event.originalTarget;
    var rel = link.rel && link.rel.toLowerCase();
    if (!link || !link.ownerDocument || !rel || !link.href)
      return;

    var feedAdded = false;
    var iconAdded = false;
    var searchAdded = false;
    var relStrings = rel.split(/\s+/);
    var rels = {};
    for (let i = 0; i < relStrings.length; i++)
      rels[relStrings[i]] = true;

    for (let relVal in rels) {
      switch (relVal) {
        case "feed":
        case "alternate":
          if (!feedAdded) {
            if (!rels.feed && rels.alternate && rels.stylesheet)
              break;

            if (isValidFeed(link, link.ownerDocument.nodePrincipal, rels.feed)) {
              FeedHandler.addFeed(link, link.ownerDocument);
              feedAdded = true;
            }
          }
          break;
        case "icon":
          if (!iconAdded) {
            if (!gPrefService.getBoolPref("browser.chrome.site_icons"))
              break;

            var targetDoc = link.ownerDocument;
            var ios = Cc["@mozilla.org/network/io-service;1"].
                      getService(Ci.nsIIOService);
            var uri = ios.newURI(link.href, targetDoc.characterSet, null);

            if (gBrowser.isFailedIcon(uri))
              break;

            // Verify that the load of this icon is legal.
            // error pages can load their favicon, to be on the safe side,
            // only allow chrome:// favicons
            const aboutNeterr = /^about:neterror\?/;
            const aboutBlocked = /^about:blocked\?/;
            const aboutCert = /^about:certerror\?/;
            if (!(aboutNeterr.test(targetDoc.documentURI) ||
                  aboutBlocked.test(targetDoc.documentURI) ||
                  aboutCert.test(targetDoc.documentURI)) ||
                !uri.schemeIs("chrome")) {
              var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].
                        getService(Ci.nsIScriptSecurityManager);
              try {
                ssm.checkLoadURIWithPrincipal(targetDoc.nodePrincipal, uri,
                                              Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
              } catch(e) {
                break;
              }
            }

            try {
              var contentPolicy = Cc["@mozilla.org/layout/content-policy;1"].
                                  getService(Ci.nsIContentPolicy);
            } catch(e) {
              break; // Refuse to load if we can't do a security check.
            }

            // Security says okay, now ask content policy
            if (contentPolicy.shouldLoad(Ci.nsIContentPolicy.TYPE_IMAGE,
                                         uri, targetDoc.documentURIObject,
                                         link, link.type, null)
                                         != Ci.nsIContentPolicy.ACCEPT)
              break;

            var browserIndex = gBrowser.getBrowserIndexForDocument(targetDoc);
            // no browser? no favicon.
            if (browserIndex == -1)
              break;

            var tab = gBrowser.mTabContainer.childNodes[browserIndex];
            gBrowser.setIcon(tab, link.href);
            iconAdded = true;
          }
          break;
        case "search":
          if (!searchAdded) {
            var type = link.type && link.type.toLowerCase();
            type = type.replace(/^\s+|\s*(?:;.*)?$/g, "");

            if (type == "application/opensearchdescription+xml" && link.title &&
                /^(?:https?|ftp):/i.test(link.href)) {
              var engine = { title: link.title, href: link.href };
              BrowserSearch.addEngine(engine, link.ownerDocument);
              searchAdded = true;
            }
          }
          break;
      }
    }
  }
}

const BrowserSearch = {
  addEngine: function(engine, targetDoc) {
    if (!this.searchBar)
      return;

    var browser = gBrowser.getBrowserForDocument(targetDoc);

    // Check to see whether we've already added an engine with this title
    if (browser.engines) {
      if (browser.engines.some(function (e) e.title == engine.title))
        return;
    }

    // Append the URI and an appropriate title to the browser data.
    // Use documentURIObject in the check for shouldLoadFavIcon so that we
    // do the right thing with about:-style error pages.  Bug 453442
    var iconURL = null;
    if (gBrowser.shouldLoadFavIcon(browser.contentDocument.documentURIObject))
      iconURL = browser.currentURI.prePath + "/favicon.ico";

    var hidden = false;
    // If this engine (identified by title) is already in the list, add it
    // to the list of hidden engines rather than to the main list.
    // XXX This will need to be changed when engines are identified by URL;
    // see bug 335102.
    var searchService = Cc["@mozilla.org/browser/search-service;1"].
                        getService(Ci.nsIBrowserSearchService);
    if (searchService.getEngineByName(engine.title))
      hidden = true;

    var engines = (hidden ? browser.hiddenEngines : browser.engines) || [];

    engines.push({ uri: engine.href,
                   title: engine.title,
                   icon: iconURL });

    if (hidden)
      browser.hiddenEngines = engines;
    else {
      browser.engines = engines;
      if (browser == gBrowser.mCurrentBrowser)
        this.updateSearchButton();
    }
  },

  /**
   * Update the browser UI to show whether or not additional engines are 
   * available when a page is loaded or the user switches tabs to a page that 
   * has search engines.
   */
  updateSearchButton: function() {
    var searchBar = this.searchBar;
    
    // The search bar binding might not be applied even though the element is
    // in the document (e.g. when the navigation toolbar is hidden), so check
    // for .searchButton specifically.
    if (!searchBar || !searchBar.searchButton)
      return;

    var engines = gBrowser.mCurrentBrowser.engines;
    if (engines && engines.length > 0)
      searchBar.searchButton.setAttribute("addengines", "true");
    else
      searchBar.searchButton.removeAttribute("addengines");
  },

  /**
   * Gives focus to the search bar, if it is present on the toolbar, or loads
   * the default engine's search form otherwise. For Mac, opens a new window
   * or focuses an existing window, if necessary.
   */
  webSearch: function BrowserSearch_webSearch() {
#ifdef XP_MACOSX
    if (window.location.href != getBrowserURL()) {
      var win = getTopWin();
      if (win) {
        // If there's an open browser window, it should handle this command
        win.focus()
        win.BrowserSearch.webSearch();
      } else {
        // If there are no open browser windows, open a new one

        // This needs to be in a timeout so that we don't end up refocused
        // in the url bar
        function webSearchCallback() {
          setTimeout(BrowserSearch.webSearch, 0);
        }

        win = window.openDialog("chrome://browser/content/", "_blank",
                                "chrome,all,dialog=no", "about:blank");
        win.addEventListener("load", webSearchCallback, false);
      }
      return;
    }
#endif
    if (window.fullScreen)
      FullScreen.mouseoverToggle(true);

    var searchBar = this.searchBar;
    if (isElementVisible(searchBar)) {
      searchBar.select();
      searchBar.focus();
    } else {
      var ss = Cc["@mozilla.org/browser/search-service;1"].
               getService(Ci.nsIBrowserSearchService);
      var searchForm = ss.defaultEngine.searchForm;
      loadURI(searchForm, null, null, false);
    }
  },

  /**
   * Loads a search results page, given a set of search terms. Uses the current
   * engine if the search bar is visible, or the default engine otherwise.
   *
   * @param searchText
   *        The search terms to use for the search.
   *
   * @param useNewTab
   *        Boolean indicating whether or not the search should load in a new
   *        tab.
   */
  loadSearch: function BrowserSearch_search(searchText, useNewTab) {
    var ss = Cc["@mozilla.org/browser/search-service;1"].
             getService(Ci.nsIBrowserSearchService);
    var engine;
  
    // If the search bar is visible, use the current engine, otherwise, fall
    // back to the default engine.
    if (isElementVisible(this.searchBar))
      engine = ss.currentEngine;
    else
      engine = ss.defaultEngine;
  
    var submission = engine.getSubmission(searchText, null); // HTML response

    // getSubmission can return null if the engine doesn't have a URL
    // with a text/html response type.  This is unlikely (since
    // SearchService._addEngineToStore() should fail for such an engine),
    // but let's be on the safe side.
    if (!submission)
      return;
  
    if (useNewTab) {
      gBrowser.loadOneTab(submission.uri.spec, null, null,
                          submission.postData, null, false);
    } else
      loadURI(submission.uri.spec, null, submission.postData, false);
  },

  /**
   * Returns the search bar element if it is present in the toolbar, null otherwise.
   */
  get searchBar() {
    return document.getElementById("searchbar");
  },

  loadAddEngines: function BrowserSearch_loadAddEngines() {
    var newWindowPref = gPrefService.getIntPref("browser.link.open_newwindow");
    var where = newWindowPref == 3 ? "tab" : "window";
    var regionBundle = document.getElementById("bundle_browser_region");
    var searchEnginesURL = formatURL("browser.search.searchEnginesURL", true);
    openUILinkIn(searchEnginesURL, where);
  }
}

function FillHistoryMenu(aParent) {
  // Remove old entries if any
  var children = aParent.childNodes;
  for (var i = children.length - 1; i >= 0; --i) {
    if (children[i].hasAttribute("index"))
      aParent.removeChild(children[i]);
  }

  var webNav = getWebNavigation();
  var sessionHistory = webNav.sessionHistory;
  var bundle_browser = document.getElementById("bundle_browser");

  var count = sessionHistory.count;
  var index = sessionHistory.index;
  var end;

  if (count <= 1) // don't display the popup for a single item
    return false;

  var half_length = Math.floor(MAX_HISTORY_MENU_ITEMS / 2);
  var start = Math.max(index - half_length, 0);
  end = Math.min(start == 0 ? MAX_HISTORY_MENU_ITEMS : index + half_length + 1, count);
  if (end == count)
    start = Math.max(count - MAX_HISTORY_MENU_ITEMS, 0);

  var tooltipBack = bundle_browser.getString("tabHistory.goBack");
  var tooltipCurrent = bundle_browser.getString("tabHistory.current");
  var tooltipForward = bundle_browser.getString("tabHistory.goForward");

  for (var j = end - 1; j >= start; j--) {
    let item = document.createElement("menuitem");
    let entry = sessionHistory.getEntryAtIndex(j, false);

    item.setAttribute("label", entry.title || entry.URI.spec);
    item.setAttribute("index", j);

    if (j != index) {
      try {
        let iconURL = Cc["@mozilla.org/browser/favicon-service;1"]
                         .getService(Ci.nsIFaviconService)
                         .getFaviconForPage(entry.URI).spec;
        item.style.listStyleImage = "url(" + iconURL + ")";
      } catch (ex) {}
    }

    if (j < index) {
      item.className = "unified-nav-back menuitem-iconic";
      item.setAttribute("tooltiptext", tooltipBack);
    } else if (j == index) {
      item.setAttribute("type", "radio");
      item.setAttribute("checked", "true");
      item.className = "unified-nav-current";
      item.setAttribute("tooltiptext", tooltipCurrent);
    } else {
      item.className = "unified-nav-forward menuitem-iconic";
      item.setAttribute("tooltiptext", tooltipForward);
    }

    aParent.appendChild(item);
  }
  return true;
}

function addToUrlbarHistory(aUrlToAdd) {
  if (aUrlToAdd &&
      aUrlToAdd.indexOf(" ") == -1 &&
      !/[\x00-\x1F]/.test(aUrlToAdd))
    PlacesUIUtils.markPageAsTyped(aUrlToAdd);
}

function toJavaScriptConsole()
{
  toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

function BrowserDownloadsUI()
{
  Cc["@mozilla.org/download-manager-ui;1"].
  getService(Ci.nsIDownloadManagerUI).show(window);
}

function toOpenWindowByType(inType, uri, features)
{
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
  var topWindow = windowManagerInterface.getMostRecentWindow(inType);

  if (topWindow)
    topWindow.focus();
  else if (features)
    window.open(uri, "_blank", features);
  else
    window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}

function toOpenDialogByTypeAndUrl(inType, relatedUrl, windowUri, features, extraArgument)
{
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
  var windows = windowManagerInterface.getEnumerator(inType);

  // Check for windows matching the url
  while (windows.hasMoreElements()) {
    var currentWindow = windows.getNext();
    if (currentWindow.document.documentElement.getAttribute("relatedUrl") == relatedUrl) {
    	currentWindow.focus();
    	return;
    }
  }

  // We didn't find a matching window, so open a new one.
  if (features)
    return window.openDialog(windowUri, "_blank", features, extraArgument);

  return window.openDialog(windowUri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar", extraArgument);
}

function OpenBrowserWindow()
{
  var charsetArg = new String();
  var handler = Components.classes["@mozilla.org/browser/clh;1"]
                          .getService(Components.interfaces.nsIBrowserHandler);
  var defaultArgs = handler.defaultArgs;
  var wintype = document.documentElement.getAttribute('windowtype');

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  var win;
  if (window && (wintype == "navigator:browser") && window.content && window.content.document)
  {
    var DocCharset = window.content.document.characterSet;
    charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no", defaultArgs, charsetArg);
  }
  else // forget about the charset information.
  {
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no", defaultArgs);
  }

  return win;
}

function BrowserCustomizeToolbar()
{
  // Disable the toolbar context menu items
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", true);

  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.setAttribute("disabled", "true");

  var splitter = document.getElementById("urlbar-search-splitter");
  if (splitter)
    splitter.parentNode.removeChild(splitter);

  var customizeURL = "chrome://global/content/customizeToolbar.xul";
#ifdef TOOLBAR_CUSTOMIZATION_SHEET
  var sheetFrame = document.getElementById("customizeToolbarSheetIFrame");
  sheetFrame.hidden = false;

  // The document might not have been loaded yet, if this is the first time.
  // If it is already loaded, reload it so that the onload initialization code
  // re-runs.
  if (sheetFrame.getAttribute("src") == customizeURL)
    sheetFrame.contentWindow.location.reload()
  else
    sheetFrame.setAttribute("src", customizeURL);

  // XXXmano: there's apparently no better way to get this when the iframe is
  // hidden
  var sheetWidth = sheetFrame.style.width.match(/([0-9]+)px/)[1];
  document.getElementById("customizeToolbarSheetPopup")
          .openPopup(gNavToolbox, "after_start", (window.innerWidth - sheetWidth) / 2, 0);
#else
  window.openDialog(customizeURL,
                    "CustomizeToolbar",
                    "chrome,titlebar,toolbar,resizable,dependent",
                    gNavToolbox);
#endif
}

function BrowserToolboxCustomizeDone(aToolboxChanged) {
#ifdef TOOLBAR_CUSTOMIZATION_SHEET
  document.getElementById("customizeToolbarSheetIFrame").hidden = true;
  document.getElementById("customizeToolbarSheetPopup").hidePopup();
#endif

  // Update global UI elements that may have been added or removed
  if (aToolboxChanged) {
    gURLBar = document.getElementById("urlbar");
    if (gURLBar)
      gURLBar.emptyText = gURLBar.getAttribute("delayedemptytext");

    gProxyFavIcon = document.getElementById("page-proxy-favicon");
    gHomeButton.updateTooltip();
    gIdentityHandler._cacheElements();
    window.XULBrowserWindow.init();

    var backForwardDropmarker = document.getElementById("back-forward-dropmarker");
    if (backForwardDropmarker)
      backForwardDropmarker.disabled =
        document.getElementById('Browser:Back').hasAttribute('disabled') &&
        document.getElementById('Browser:Forward').hasAttribute('disabled');

    // support downgrading to Firefox 2.0
    var navBar = document.getElementById("nav-bar");
    navBar.setAttribute("currentset",
                        navBar.getAttribute("currentset")
                              .replace("unified-back-forward-button",
                                "unified-back-forward-button,back-button,forward-button"));
    document.persist(navBar.id, "currentset");

#ifndef XP_MACOSX
    updateEditUIVisibility();
#endif
  }

  UpdateUrlbarSearchSplitterState();

  gHomeButton.updatePersonalToolbarStyle();

  // Update the urlbar
  if (gURLBar) {
    URLBarSetURI();
    XULBrowserWindow.asyncUpdateUI();
    PlacesStarButton.updateState();
  }

  // Re-enable parts of the UI we disabled during the dialog
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", false);
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.removeAttribute("disabled");

  // XXXmano bug 287105: wallpaper to bug 309953,
  // the reload button isn't in sync with the reload command.
  var reloadButton = document.getElementById("reload-button");
  if (reloadButton) {
    reloadButton.disabled =
      document.getElementById("Browser:Reload").getAttribute("disabled") == "true";
  }
  //bug 440702: the back and forward buttons also suffer from bug 309953.
  var backButton = document.getElementById("back-button");
  if (backButton) {
    backButton.disabled =
      document.getElementById("Browser:Back").getAttribute("disabled") == "true";
  }
  var forwardButton = document.getElementById("forward-button");
  if (forwardButton) {
    forwardButton.disabled =
      document.getElementById("Browser:Forward").getAttribute("disabled") == "true";
  }

#ifdef XP_MACOSX
  // make sure to re-enable click-and-hold
  if (!getBoolPref("ui.click_hold_context_menus", false))
    SetClickAndHoldHandlers();
#endif

#ifndef TOOLBAR_CUSTOMIZATION_SHEET
  // XXX Shouldn't have to do this, but I do
  window.focus();
#endif
}

function BrowserToolboxCustomizeChange() {
  gHomeButton.updatePersonalToolbarStyle();
}

/**
 * Update the global flag that tracks whether or not any edit UI (the Edit menu,
 * edit-related items in the context menu, and edit-related toolbar buttons
 * is visible, then update the edit commands' enabled state accordingly.  We use
 * this flag to skip updating the edit commands on focus or selection changes
 * when no UI is visible to improve performance (including pageload performance,
 * since focus changes when you load a new page).
 *
 * If UI is visible, we use goUpdateGlobalEditMenuItems to set the commands'
 * enabled state so the UI will reflect it appropriately.
 * 
 * If the UI isn't visible, we enable all edit commands so keyboard shortcuts
 * still work and just lazily disable them as needed when the user presses a
 * shortcut.
 *
 * This doesn't work on Mac, since Mac menus flash when users press their
 * keyboard shortcuts, so edit UI is essentially always visible on the Mac,
 * and we need to always update the edit commands.  Thus on Mac this function
 * is a no op.
 */
function updateEditUIVisibility()
{
#ifndef XP_MACOSX
  let editMenuPopupState = document.getElementById("menu_EditPopup").state;
  let contextMenuPopupState = document.getElementById("contentAreaContextMenu").state;
  let placesContextMenuPopupState = document.getElementById("placesContext").state;

  // The UI is visible if the Edit menu is opening or open, if the context menu
  // is open, or if the toolbar has been customized to include the Cut, Copy,
  // or Paste toolbar buttons.
  gEditUIVisible = editMenuPopupState == "showing" ||
                   editMenuPopupState == "open" ||
                   contextMenuPopupState == "showing" ||
                   contextMenuPopupState == "open" ||
                   placesContextMenuPopupState == "showing" ||
                   placesContextMenuPopupState == "open" ||
                   document.getElementById("cut-button") ||
                   document.getElementById("copy-button") ||
                   document.getElementById("paste-button") ? true : false;

  // If UI is visible, update the edit commands' enabled state to reflect
  // whether or not they are actually enabled for the current focus/selection.
  if (gEditUIVisible)
    goUpdateGlobalEditMenuItems();

  // Otherwise, enable all commands, so that keyboard shortcuts still work,
  // then lazily determine their actual enabled state when the user presses
  // a keyboard shortcut.
  else {
    goSetCommandEnabled("cmd_undo", true);
    goSetCommandEnabled("cmd_redo", true);
    goSetCommandEnabled("cmd_cut", true);
    goSetCommandEnabled("cmd_copy", true);
    goSetCommandEnabled("cmd_paste", true);
    goSetCommandEnabled("cmd_selectAll", true);
    goSetCommandEnabled("cmd_delete", true);
    goSetCommandEnabled("cmd_switchTextDirection", true);
  }
#endif
}

var FullScreen =
{
  _XULNS: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
  toggle: function()
  {
    // show/hide all menubars, toolbars, and statusbars (except the full screen toolbar)
    this.showXULChrome("toolbar", window.fullScreen);
    this.showXULChrome("statusbar", window.fullScreen);
    document.getElementById("fullScreenItem").setAttribute("checked", !window.fullScreen);

    var fullScrToggler = document.getElementById("fullscr-toggler");
    if (!window.fullScreen) {
      // Add a tiny toolbar to receive mouseover and dragenter events, and provide affordance.
      // This will help simulate the "collapse" metaphor while also requiring less code and
      // events than raw listening of mouse coords.
      if (!fullScrToggler) {
        fullScrToggler = document.createElement("toolbar");
        fullScrToggler.id = "fullscr-toggler";
        fullScrToggler.setAttribute("customizable", "false");
        fullScrToggler.setAttribute("moz-collapsed", "true");
        var navBar = document.getElementById("nav-bar");
        navBar.parentNode.insertBefore(fullScrToggler, navBar);
      }
      fullScrToggler.addEventListener("mouseover", this._expandCallback, false);
      fullScrToggler.addEventListener("dragenter", this._expandCallback, false);

      if (gPrefService.getBoolPref("browser.fullscreen.autohide"))
        gBrowser.mPanelContainer.addEventListener("mousemove",
                                                  this._collapseCallback, false);

      document.addEventListener("keypress", this._keyToggleCallback, false);
      document.addEventListener("popupshown", this._setPopupOpen, false);
      document.addEventListener("popuphidden", this._setPopupOpen, false);
      this._shouldAnimate = true;
      this.mouseoverToggle(false);

      // Autohide prefs
      gPrefService.addObserver("browser.fullscreen", this, false);
    }
    else {
      document.removeEventListener("keypress", this._keyToggleCallback, false);
      document.removeEventListener("popupshown", this._setPopupOpen, false);
      document.removeEventListener("popuphidden", this._setPopupOpen, false);
      gPrefService.removeObserver("browser.fullscreen", this);

      if (fullScrToggler) {
        fullScrToggler.removeEventListener("mouseover", this._expandCallback, false);
        fullScrToggler.removeEventListener("dragenter", this._expandCallback, false);
      }

      // The user may quit fullscreen during an animation
      clearInterval(this._animationInterval);
      clearTimeout(this._animationTimeout);
      gNavToolbox.style.marginTop = "0px";
      if (this._isChromeCollapsed)
        this.mouseoverToggle(true);
      this._isAnimating = false;
      // This is needed if they use the context menu to quit fullscreen
      this._isPopupOpen = false;

      gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                   this._collapseCallback, false);
    }
  },

  observe: function(aSubject, aTopic, aData)
  {
    if (aData == "browser.fullscreen.autohide") {
      if (gPrefService.getBoolPref("browser.fullscreen.autohide")) {
        gBrowser.mPanelContainer.addEventListener("mousemove",
                                                  this._collapseCallback, false);
      }
      else {
        gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                     this._collapseCallback, false);
      }
    }
  },

  // Event callbacks
  _expandCallback: function()
  {
    FullScreen.mouseoverToggle(true);
  },
  _collapseCallback: function()
  {
    FullScreen.mouseoverToggle(false);
  },
  _keyToggleCallback: function(aEvent)
  {
    // if we can use the keyboard (eg Ctrl+L or Ctrl+E) to open the toolbars, we
    // should provide a way to collapse them too.
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      FullScreen._shouldAnimate = false;
      FullScreen.mouseoverToggle(false, true);
    }
    // F6 is another shortcut to the address bar, but its not covered in OpenLocation()
    else if (aEvent.keyCode == aEvent.DOM_VK_F6)
      FullScreen.mouseoverToggle(true);
  },

  // Checks whether we are allowed to collapse the chrome
  _isPopupOpen: false,
  _isChromeCollapsed: false,
  _safeToCollapse: function(forceHide)
  {
    if (!gPrefService.getBoolPref("browser.fullscreen.autohide"))
      return false;

    // a popup menu is open in chrome: don't collapse chrome
    if (!forceHide && this._isPopupOpen)
      return false;

    // a textbox in chrome is focused (location bar anyone?): don't collapse chrome
    if (document.commandDispatcher.focusedElement &&
        document.commandDispatcher.focusedElement.ownerDocument == document &&
        document.commandDispatcher.focusedElement.localName == "input") {
      if (forceHide)
        // hidden textboxes that still have focus are bad bad bad
        document.commandDispatcher.focusedElement.blur();
      else
        return false;
    }
    return true;
  },

  _setPopupOpen: function(aEvent)
  {
    // Popups should only veto chrome collapsing if they were opened when the chrome was not collapsed.
    // Otherwise, they would not affect chrome and the user would expect the chrome to go away.
    // e.g. we wouldn't want the autoscroll icon firing this event, so when the user
    // toggles chrome when moving mouse to the top, it doesn't go away again.
    if (aEvent.type == "popupshown" && !FullScreen._isChromeCollapsed &&
        aEvent.target.localName != "tooltip" && aEvent.target.localName != "window")
      FullScreen._isPopupOpen = true;
    else if (aEvent.type == "popuphidden" && aEvent.target.localName != "tooltip" &&
             aEvent.target.localName != "window")
      FullScreen._isPopupOpen = false;
  },

  // Autohide helpers for the context menu item
  getAutohide: function(aItem)
  {
    aItem.setAttribute("checked", gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },
  setAutohide: function()
  {
    gPrefService.setBoolPref("browser.fullscreen.autohide", !gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },

  // Animate the toolbars disappearing
  _shouldAnimate: true,
  _isAnimating: false,
  _animationTimeout: null,
  _animationInterval: null,
  _animateUp: function()
  {
    // check again, the user may have done something before the animation was due to start
    if (!window.fullScreen || !FullScreen._safeToCollapse(false)) {
      FullScreen._isAnimating = false;
      FullScreen._shouldAnimate = true;
      return;
    }

    var animateFrameAmount = 2;
    function animateUpFrame() {
      animateFrameAmount *= 2;
      if (animateFrameAmount >=
          (gNavToolbox.boxObject.height + gBrowser.mStrip.boxObject.height)) {
        // We've animated enough
        clearInterval(FullScreen._animationInterval);
        gNavToolbox.style.marginTop = "0px";
        FullScreen._isAnimating = false;
        FullScreen._shouldAnimate = false; // Just to make sure
        FullScreen.mouseoverToggle(false);
        return;
      }
      gNavToolbox.style.marginTop = (animateFrameAmount * -1) + "px";
    }

    FullScreen._animationInterval = setInterval(animateUpFrame, 70);
  },

  mouseoverToggle: function(aShow, forceHide)
  {
    // Don't do anything if:
    // a) we're already in the state we want,
    // b) we're animating and will become collapsed soon, or
    // c) we can't collapse because it would be undesirable right now
    if (aShow != this._isChromeCollapsed || (!aShow && this._isAnimating) ||
        (!aShow && !this._safeToCollapse(forceHide)))
      return;

    // browser.fullscreen.animateUp
    // 0 - never animate up
    // 1 - animate only for first collapse after entering fullscreen (default for perf's sake)
    // 2 - animate every time it collapses
    if (gPrefService.getIntPref("browser.fullscreen.animateUp") == 0)
      this._shouldAnimate = false;

    if (!aShow && this._shouldAnimate) {
      this._isAnimating = true;
      this._shouldAnimate = false;
      this._animationTimeout = setTimeout(this._animateUp, 800);
      return;
    }

    // The chrome is collapsed so don't spam needless mousemove events
    if (aShow) {
      gBrowser.mPanelContainer.addEventListener("mousemove",
                                                this._collapseCallback, false);
    }
    else {
      gBrowser.mPanelContainer.removeEventListener("mousemove",
                                                   this._collapseCallback, false);
    }

    gBrowser.mStrip.setAttribute("moz-collapsed", !aShow);
    var allFSToolbars = document.getElementsByTagNameNS(this._XULNS, "toolbar");
    for (var i = 0; i < allFSToolbars.length; i++) {
      if (allFSToolbars[i].getAttribute("fullscreentoolbar") == "true")
        allFSToolbars[i].setAttribute("moz-collapsed", !aShow);
    }
    document.getElementById("fullscr-toggler").setAttribute("moz-collapsed", aShow);
    this._isChromeCollapsed = !aShow;
    if (gPrefService.getIntPref("browser.fullscreen.animateUp") == 2)
      this._shouldAnimate = true;
  },

  showXULChrome: function(aTag, aShow)
  {
    var els = document.getElementsByTagNameNS(this._XULNS, aTag);

    for (var i = 0; i < els.length; ++i) {
      // XXX don't interfere with previously collapsed toolbars
      if (els[i].getAttribute("fullscreentoolbar") == "true") {
        if (!aShow) {

          var toolbarMode = els[i].getAttribute("mode");
          if (toolbarMode != "text") {
            els[i].setAttribute("saved-mode", toolbarMode);
            els[i].setAttribute("saved-iconsize",
                                els[i].getAttribute("iconsize"));
            els[i].setAttribute("mode", "icons");
            els[i].setAttribute("iconsize", "small");
          }

          // Give the main nav bar the fullscreen context menu, otherwise remove it
          // to prevent breakage
          els[i].setAttribute("saved-context",
                              els[i].getAttribute("context"));
          if (els[i].id == "nav-bar")
            els[i].setAttribute("context", "autohide-context");
          else
            els[i].removeAttribute("context");

          // Set the inFullscreen attribute to allow specific styling
          // in fullscreen mode
          els[i].setAttribute("inFullscreen", true);
        }
        else {
          function restoreAttr(attrName) {
            var savedAttr = "saved-" + attrName;
            if (els[i].hasAttribute(savedAttr)) {
              els[i].setAttribute(attrName, els[i].getAttribute(savedAttr));
              els[i].removeAttribute(savedAttr);
            }
          }

          restoreAttr("mode");
          restoreAttr("iconsize");
          restoreAttr("context");

          els[i].removeAttribute("inFullscreen");
        }
      } else {
        // use moz-collapsed so it doesn't persist hidden/collapsed,
        // so that new windows don't have missing toolbars
        if (aShow)
          els[i].removeAttribute("moz-collapsed");
        else
          els[i].setAttribute("moz-collapsed", "true");
      }
    }

    if (aShow)
      gNavToolbox.removeAttribute("inFullscreen");
    else
      gNavToolbox.setAttribute("inFullscreen", true);

#ifndef XP_MACOSX
    var controls = document.getElementsByAttribute("fullscreencontrol", "true");
    for (var i = 0; i < controls.length; ++i)
      controls[i].hidden = aShow;
#endif
  }
};

/**
 * Returns true if |aMimeType| is text-based, false otherwise.
 *
 * @param aMimeType
 *        The MIME type to check.
 *
 * If adding types to this function, please also check the similar 
 * function in findbar.xml
 */
function mimeTypeIsTextBased(aMimeType)
{
  return /^text\/|\+xml$/.test(aMimeType) ||
         aMimeType == "application/x-javascript" ||
         aMimeType == "application/javascript" ||
         aMimeType == "application/xml" ||
         aMimeType == "mozilla.application/cached-xul";
}

var XULBrowserWindow = {
  // Stored Status, Link and Loading values
  status: "",
  defaultStatus: "",
  jsStatus: "",
  jsDefaultStatus: "",
  overLink: "",
  startTime: 0,
  statusText: "",
  lastURI: null,
  isBusy: false,

  statusTimeoutInEffect: false,

  QueryInterface: function (aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsIXULBrowserWindow) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  },

  get statusMeter () {
    delete this.statusMeter;
    return this.statusMeter = document.getElementById("statusbar-icon");
  },
  get stopCommand () {
    delete this.stopCommand;
    return this.stopCommand = document.getElementById("Browser:Stop");
  },
  get reloadCommand () {
    delete this.reloadCommand;
    return this.reloadCommand = document.getElementById("Browser:Reload");
  },
  get statusTextField () {
    delete this.statusTextField;
    return this.statusTextField = document.getElementById("statusbar-display");
  },
  get securityButton () {
    delete this.securityButton;
    return this.securityButton = document.getElementById("security-button");
  },
  get isImage () {
    delete this.isImage;
    return this.isImage = document.getElementById("isImage");
  },
  get _uriFixup () {
    delete this._uriFixup;
    return this._uriFixup = Cc["@mozilla.org/docshell/urifixup;1"]
                              .getService(Ci.nsIURIFixup);
  },

  init: function () {
    this.throbberElement = document.getElementById("navigator-throbber");

    // Initialize the security button's state and tooltip text.  Remember to reset
    // _hostChanged, otherwise onSecurityChange will short circuit.
    var securityUI = gBrowser.securityUI;
    this._hostChanged = true;
    this.onSecurityChange(null, null, securityUI.state);
  },

  destroy: function () {
    // XXXjag to avoid leaks :-/, see bug 60729
    delete this.throbberElement;
    delete this.statusMeter;
    delete this.stopCommand;
    delete this.reloadCommand;
    delete this.statusTextField;
    delete this.securityButton;
    delete this.statusText;
    delete this.lastURI;
  },

  setJSStatus: function (status) {
    this.jsStatus = status;
    this.updateStatusField();
  },

  setJSDefaultStatus: function (status) {
    this.jsDefaultStatus = status;
    this.updateStatusField();
  },

  setDefaultStatus: function (status) {
    this.defaultStatus = status;
    this.updateStatusField();
  },

  setOverLink: function (link, b) {
    // Encode bidirectional formatting characters.
    // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
    this.overLink = link.replace(/[\u200e\u200f\u202a\u202b\u202c\u202d\u202e]/g,
                                 encodeURIComponent);
    this.updateStatusField();
  },

  updateStatusField: function () {
    var text = this.overLink || this.status || this.jsStatus || this.jsDefaultStatus || this.defaultStatus;

    // check the current value so we don't trigger an attribute change
    // and cause needless (slow!) UI updates
    if (this.statusText != text) {
      this.statusTextField.label = text;
      this.statusText = text;
    }
  },
  
  onLinkIconAvailable: function (aBrowser) {
    if (gProxyFavIcon && gBrowser.userTypedValue === null)
      PageProxySetIcon(aBrowser.mIconURL); // update the favicon in the URL bar
  },

  onProgressChange: function (aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress) {
    if (aMaxTotalProgress > 0) {
      // This is highly optimized.  Don't touch this code unless
      // you are intimately familiar with the cost of setting
      // attrs on XUL elements. -- hyatt
      var percentage = (aCurTotalProgress * 100) / aMaxTotalProgress;
      this.statusMeter.value = percentage;
    }
  },

  onProgressChange64: function (aWebProgress, aRequest,
                                aCurSelfProgress, aMaxSelfProgress,
                                aCurTotalProgress, aMaxTotalProgress) {
    return this.onProgressChange(aWebProgress, aRequest,
      aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress,
      aMaxTotalProgress);
  },

  onStateChange: function (aWebProgress, aRequest, aStateFlags, aStatus) {
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    const nsIChannel = Components.interfaces.nsIChannel;
    if (aStateFlags & nsIWebProgressListener.STATE_START) {
      // This (thanks to the filter) is a network start or the first
      // stray request (the first request outside of the document load),
      // initialize the throbber and his friends.

      // Call start document load listeners (only if this is a network load)
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK &&
          aRequest && aWebProgress.DOMWindow == content)
        this.startDocumentLoad(aRequest);

      this.isBusy = true;

      if (this.throbberElement) {
        // Turn the throbber on.
        this.throbberElement.setAttribute("busy", "true");
      }

      // Turn the status meter on.
      this.statusMeter.value = 0;  // be sure to clear the progress bar
      if (gProgressCollapseTimer) {
        window.clearTimeout(gProgressCollapseTimer);
        gProgressCollapseTimer = null;
      }
      else
        this.statusMeter.parentNode.collapsed = false;

      // XXX: This needs to be based on window activity...
      this.stopCommand.removeAttribute("disabled");
    }
    else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
      if (aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
        if (aWebProgress.DOMWindow == content) {
          if (aRequest)
            this.endDocumentLoad(aRequest, aStatus);

          if (Components.isSuccessCode(aStatus) &&
              content.document.documentElement.getAttribute("manifest"))
            OfflineApps.offlineAppRequested(content);
        }
      }

      // This (thanks to the filter) is a network stop or the last
      // request stop outside of loading the document, stop throbbers
      // and progress bars and such
      if (aRequest) {
        let msg = "";
        let location;
        // Get the URI either from a channel or a pseudo-object
        if (aRequest instanceof nsIChannel || "URI" in aRequest) {
          location = aRequest.URI;

          // For keyword URIs clear the user typed value since they will be changed into real URIs
          if (location.scheme == "keyword" && aWebProgress.DOMWindow == content)
            gBrowser.userTypedValue = null;

          if (location.spec != "about:blank") {
            switch (aStatus) {
              case Components.results.NS_BINDING_ABORTED:
                msg = gNavigatorBundle.getString("nv_stopped");
                break;
              case Components.results.NS_ERROR_NET_TIMEOUT:
                msg = gNavigatorBundle.getString("nv_timeout");
                break;
            }
          }
        }
        // If msg is false then we did not have an error (channel may have
        // been null, in the case of a stray image load).
        if (!msg && (!location || location.spec != "about:blank"))
          msg = gNavigatorBundle.getString("nv_done");

        this.status = "";
        this.setDefaultStatus(msg);

        // Disable menu entries for images, enable otherwise
        if (content.document && mimeTypeIsTextBased(content.document.contentType))
          this.isImage.removeAttribute('disabled');
        else
          this.isImage.setAttribute('disabled', 'true');
      }

      this.isBusy = false;

      // Turn the progress meter and throbber off.
      gProgressCollapseTimer = window.setTimeout(function () {
        gProgressMeterPanel.collapsed = true;
        gProgressCollapseTimer = null;
      }, 100);

      if (this.throbberElement)
        this.throbberElement.removeAttribute("busy");

      this.stopCommand.setAttribute("disabled", "true");
    }
  },

  onLocationChange: function (aWebProgress, aRequest, aLocationURI) {
    var location = aLocationURI ? aLocationURI.spec : "";
    this._hostChanged = true;

    if (document.tooltipNode) {
      // Optimise for the common case
      if (aWebProgress.DOMWindow == content) {
        document.getElementById("aHTMLTooltip").hidePopup();
        document.tooltipNode = null;
      }
      else {
        for (let tooltipWindow =
               document.tooltipNode.ownerDocument.defaultView;
             tooltipWindow != tooltipWindow.parent;
             tooltipWindow = tooltipWindow.parent) {
          if (tooltipWindow == aWebProgress.DOMWindow) {
            document.getElementById("aHTMLTooltip").hidePopup();
            document.tooltipNode = null;
            break;
          }
        }
      }
    }

    // This code here does not compare uris exactly when determining
    // whether or not the message should be hidden since the message
    // may be prematurely hidden when an install is invoked by a click
    // on a link that looks like this:
    //
    // <a href="#" onclick="return install();">Install Foo</a>
    //
    // - which fires a onLocationChange message to uri + '#'...
    var selectedBrowser = gBrowser.selectedBrowser;
    if (selectedBrowser.lastURI) {
      let oldSpec = selectedBrowser.lastURI.spec;
      let oldIndexOfHash = oldSpec.indexOf("#");
      if (oldIndexOfHash != -1)
        oldSpec = oldSpec.substr(0, oldIndexOfHash);
      let newSpec = location;
      let newIndexOfHash = newSpec.indexOf("#");
      if (newIndexOfHash != -1)
        newSpec = newSpec.substr(0, newSpec.indexOf("#"));
      if (newSpec != oldSpec) {
        // Remove all the notifications, except for those which want to
        // persist across the first location change.
        let nBox = gBrowser.getNotificationBox(selectedBrowser);
        nBox.removeTransientNotifications();
      }
    }
    selectedBrowser.lastURI = aLocationURI;

    // Disable menu entries for images, enable otherwise
    if (content.document && mimeTypeIsTextBased(content.document.contentType))
      this.isImage.removeAttribute('disabled');
    else
      this.isImage.setAttribute('disabled', 'true');

    this.setOverLink("", null);

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation

    var browser = gBrowser.selectedBrowser;
    if (aWebProgress.DOMWindow == content) {
      if ((location == "about:blank" && !content.opener) ||
          location == "") {  // Second condition is for new tabs, otherwise
                             // reload function is enabled until tab is refreshed.
        this.reloadCommand.setAttribute("disabled", "true");
      } else {
        this.reloadCommand.removeAttribute("disabled");
      }

      if (gURLBar) {
        // Strip off "wyciwyg://" and passwords for the location bar
        let uri = aLocationURI;
        try {
          uri = this._uriFixup.createExposableURI(uri);
        } catch (e) {}
        URLBarSetURI(uri, true);

        // Update starring UI
        PlacesStarButton.updateState();
      }

      FullZoom.onLocationChange(aLocationURI);
    }
    UpdateBackForwardCommands(gBrowser.webNavigation);

    if (gFindBar.findMode != gFindBar.FIND_NORMAL) {
      // Close the Find toolbar if we're in old-style TAF mode
      gFindBar.close();
    }

    // XXXmano new-findbar, do something useful once it lands.
    // Of course, this is especially wrong with bfcache on...

    // fix bug 253793 - turn off highlight when page changes
    gFindBar.getElement("highlight").checked = false;

    // See bug 358202, when tabs are switched during a drag operation,
    // timers don't fire on windows (bug 203573)
    if (aRequest)
      setTimeout(function () { XULBrowserWindow.asyncUpdateUI(); }, 0);
    else
      this.asyncUpdateUI();
  },
  
  asyncUpdateUI: function () {
    FeedHandler.updateFeeds();
    BrowserSearch.updateSearchButton();
  },

  onStatusChange: function (aWebProgress, aRequest, aStatus, aMessage) {
    this.status = aMessage;
    this.updateStatusField();
  },

  onSecurityChange: function (aBrowser, aWebProgress, aRequest, aState) {
  }
}

var TabsProgressListener = {
  onProgressChange: function (aBrowser, aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress) {
  },

  onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
  },

  onLocationChange: function (aBrowser, aWebProgress, aRequest, aLocationURI) {
  },
  
  onStatusChange: function (aBrowser, aWebProgress, aRequest, aStatus, aMessage) {
  },

  onRefreshAttempted: function (aBrowser, aWebProgress, aURI, aDelay, aSameURI) {
    if (gPrefService.getBoolPref("accessibility.blockautorefresh")) {
      let brandBundle = document.getElementById("bundle_brand");
      let brandShortName = brandBundle.getString("brandShortName");
      let refreshButtonText =
        gNavigatorBundle.getString("refreshBlocked.goButton");
      let refreshButtonAccesskey =
        gNavigatorBundle.getString("refreshBlocked.goButton.accesskey");
      let message =
        gNavigatorBundle.getFormattedString(aSameURI ? "refreshBlocked.refreshLabel"
                                                     : "refreshBlocked.redirectLabel",
                                            [brandShortName]);
      let docShell = aWebProgress.DOMWindow
                                 .QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell);
      let notificationBox = gBrowser.getNotificationBox(aBrowser);
      let notification = notificationBox.getNotificationWithValue("refresh-blocked");
      if (notification) {
        notification.label = message;
        notification.refreshURI = aURI;
        notification.delay = aDelay;
        notification.docShell = docShell;
      } else {
        let buttons = [{
          label: refreshButtonText,
          accessKey: refreshButtonAccesskey,
          callback: function (aNotification, aButton) {
            var refreshURI = aNotification.docShell
                                          .QueryInterface(Ci.nsIRefreshURI);
            refreshURI.forceRefreshURI(aNotification.refreshURI,
                                       aNotification.delay, true);
          }
        }];
        notification =
          notificationBox.appendNotification(message, "refresh-blocked",
                                             "chrome://browser/skin/Info.png",
                                             notificationBox.PRIORITY_INFO_MEDIUM,
                                             buttons);
        notification.refreshURI = aURI;
        notification.delay = aDelay;
        notification.docShell = docShell;
      }
      return false;
    }
    return true;
  },

  // Properties used to cache security state used to update the UI
  _state: null,
  _host: undefined,
  _tooltipText: null,
  _hostChanged: false, // onLocationChange will flip this bit

  onSecurityChange: function (aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    if (this._state == aState &&
        this._tooltipText == gBrowser.securityUI.tooltipText &&
        !this._hostChanged) {
#ifdef DEBUG
      try {
        var contentHost = gBrowser.contentWindow.location.host;
        if (this._host !== undefined && this._host != contentHost) {
            Components.utils.reportError(
              "ASSERTION: browser.js host is inconsistent. Content window has " + 
              "<" + contentHost + "> but cached host is <" + this._host + ">.\n"
            );
        }
      } catch (ex) {}
#endif
      return;
    }
    this._state = aState;

    try {
      this._host = gBrowser.contentWindow.location.host;
    } catch(ex) {
      this._host = null;
    }

    this._hostChanged = false;
    this._tooltipText = gBrowser.securityUI.tooltipText

    // aState is defined as a bitmask that may be extended in the future.
    // We filter out any unknown bits before testing for known values.
    const wpl = Components.interfaces.nsIWebProgressListener;
    const wpl_security_bits = wpl.STATE_IS_SECURE |
                              wpl.STATE_IS_BROKEN |
                              wpl.STATE_IS_INSECURE |
                              wpl.STATE_SECURE_HIGH |
                              wpl.STATE_SECURE_MED |
                              wpl.STATE_SECURE_LOW;
    var level;
    var setHost = false;

    switch (this._state & wpl_security_bits) {
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_HIGH:
        level = "high";
        setHost = true;
        break;
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_MED:
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_LOW:
        level = "low";
        setHost = true;
        break;
      case wpl.STATE_IS_BROKEN:
        level = "broken";
        break;
    }

    if (level) {
      this.securityButton.setAttribute("level", level);
      this.securityButton.hidden = false;
      // We don't style the Location Bar based on the the 'level' attribute
      // anymore, but still set it for third-party themes.
      if (gURLBar)
        gURLBar.setAttribute("level", level);
    } else {
      this.securityButton.hidden = true;
      this.securityButton.removeAttribute("level");
      if (gURLBar)
        gURLBar.removeAttribute("level");
    }

    if (setHost && this._host)
      this.securityButton.setAttribute("label", this._host);
    else
      this.securityButton.removeAttribute("label");

    this.securityButton.setAttribute("tooltiptext", this._tooltipText);

    // Don't pass in the actual location object, since it can cause us to 
    // hold on to the window object too long.  Just pass in the fields we
    // care about. (bug 424829)
    var location = gBrowser.contentWindow.location;
    var locationObj = {};
    try {
      locationObj.host = location.host;
      locationObj.hostname = location.hostname;
      locationObj.port = location.port;
    } catch (ex) {
      // Can sometimes throw if the URL being visited has no host/hostname,
      // e.g. about:blank. The _state for these pages means we won't need these
      // properties anyways, though.
    }
    gIdentityHandler.checkIdentity(this._state, locationObj);
  },

  // simulate all change notifications after switching tabs
  onUpdateCurrentBrowser: function (aStateFlags, aStatus, aMessage, aTotalProgress) {
    var nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    var loadingDone = aStateFlags & nsIWebProgressListener.STATE_STOP;
    // use a pseudo-object instead of a (potentially non-existing) channel for getting
    // a correct error message - and make sure that the UI is always either in
    // loading (STATE_START) or done (STATE_STOP) mode
    this.onStateChange(
      gBrowser.webProgress,
      { URI: gBrowser.currentURI },
      loadingDone ? nsIWebProgressListener.STATE_STOP : nsIWebProgressListener.STATE_START,
      aStatus
    );
    // status message and progress value are undefined if we're done with loading
    if (loadingDone)
      return;
    this.onStatusChange(gBrowser.webProgress, null, 0, aMessage);
    this.onProgressChange(gBrowser.webProgress, 0, 0, aTotalProgress, 1);
  },

  startDocumentLoad: function (aRequest) {
    // clear out feed data
    gBrowser.mCurrentBrowser.feeds = null;

    // clear out search-engine data
    gBrowser.mCurrentBrowser.engines = null;    

    var uri = aRequest.QueryInterface(Ci.nsIChannel).URI;
    var observerService = Cc["@mozilla.org/observer-service;1"]
                            .getService(Ci.nsIObserverService);

    if (gURLBar &&
        gURLBar.value == "" &&
        getWebNavigation().currentURI.spec == "about:blank")
      URLBarSetURI(uri);

    try {
      observerService.notifyObservers(content, "StartDocumentLoad", uri.spec);
    } catch (e) {
    }
  },

  endDocumentLoad: function (aRequest, aStatus) {
    var urlStr = aRequest.QueryInterface(Ci.nsIChannel).originalURI.spec;

    var observerService = Cc["@mozilla.org/observer-service;1"]
                            .getService(Ci.nsIObserverService);

    var notification = Components.isSuccessCode(aStatus) ? "EndDocumentLoad" : "FailDocumentLoad";
    try {
      observerService.notifyObservers(content, notification, urlStr);
    } catch (e) {
    }
  }
}

function nsBrowserAccess()
{
}

nsBrowserAccess.prototype =
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(Ci.nsIBrowserDOMWindow) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  openURI : function(aURI, aOpener, aWhere, aContext)
  {
    var newWindow = null;
    var referrer = null;
    var isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      dump("use -chrome command-line option to load external chrome urls\n");
      return null;
    }

    var loadflags = isExternal ?
                       Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                       Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    var location;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW)
      aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
    switch (aWhere) {
      case Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW :
        // FIXME: Bug 408379. So how come this doesn't send the
        // referrer like the other loads do?
        var url = aURI ? aURI.spec : "about:blank";
        // Pass all params to openDialog to ensure that "url" isn't passed through
        // loadOneOrMoreURIs, which splits based on "|"
        newWindow = openDialog(getBrowserURL(), "_blank", "all,dialog=no", url, null, null, null);
        break;
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB :
        var win = this._getMostRecentBrowserWindow();
        if (!win) {
          // we couldn't find a suitable window, a new one needs to be opened.
          return null;
        }
        var loadInBackground = gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground");
        var newTab = win.gBrowser.loadOneTab("about:blank", null, null, null, loadInBackground, false);
        newWindow = win.gBrowser.getBrowserForTab(newTab).docShell
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
        try {
          if (aURI) {
            if (aOpener) {
              location = aOpener.location;
              referrer =
                      Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService)
                                .newURI(location, null, null);
            }
            newWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .loadURI(aURI.spec, loadflags, referrer, null, null);
          }
          if (!loadInBackground && isExternal)
            newWindow.focus();
        } catch(e) {
        }
        break;
      default : // OPEN_CURRENTWINDOW or an illegal value
        try {
          if (aOpener) {
            newWindow = aOpener.top;
            if (aURI) {
              location = aOpener.location;
              referrer =
                      Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService)
                                .newURI(location, null, null);

              newWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(nsIWebNavigation)
                       .loadURI(aURI.spec, loadflags, referrer, null, null);
            }
          } else {
            newWindow = gBrowser.selectedBrowser.docShell
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
            if (aURI) {
              gBrowser.loadURIWithFlags(aURI.spec, loadflags, null, 
                                        null, null);
            }
          }
          if(!gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground"))
            content.focus();
        } catch(e) {
        }
    }
    return newWindow;
  },

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define BROKEN_WM_Z_ORDER
#endif
#endif
#ifdef XP_OS2
#define BROKEN_WM_Z_ORDER
#endif

  // this returns the most recent non-popup browser window
  _getMostRecentBrowserWindow : function ()
  {
    if (!window.document.documentElement.getAttribute("chromehidden"))
      return window;
 
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
 
#ifdef BROKEN_WM_Z_ORDER
    var win = wm.getMostRecentWindow("navigator:browser", true);
 
    // if we're lucky, this isn't a popup, and we can just return this
    if (win && win.document.documentElement.getAttribute("chromehidden")) {
      win = null;
      var windowList = wm.getEnumerator("navigator:browser", true);
      // this is oldest to newest, so this gets a bit ugly
      while (windowList.hasMoreElements()) {
        var nextWin = windowList.getNext();
        if (!nextWin.document.documentElement.getAttribute("chromehidden"))
          win = nextWin;
      }
    }
#else
    var windowList = wm.getZOrderDOMWindowEnumerator("navigator:browser", true);
    if (!windowList.hasMoreElements())
      return null;
 
    var win = windowList.getNext();
    while (win.document.documentElement.getAttribute("chromehidden")) {
      if (!windowList.hasMoreElements()) 
        return null;
 
      win = windowList.getNext();
    }
#endif

    return win;
  },

  isTabContentWindow : function(aWindow)
  {
    return gBrowser.browsers.some(function (browser) browser.contentWindow == aWindow);
  }
}

function onViewToolbarsPopupShowing(aEvent)
{
  var popup = aEvent.target;
  var i;

  // Empty the menu
  for (i = popup.childNodes.length-1; i >= 0; --i) {
    var deadItem = popup.childNodes[i];
    if (deadItem.hasAttribute("toolbarindex"))
      popup.removeChild(deadItem);
  }

  var firstMenuItem = popup.firstChild;

  for (i = 0; i < gNavToolbox.childNodes.length; ++i) {
    var toolbar = gNavToolbox.childNodes[i];
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

      menuItem.addEventListener("command", onViewToolbarCommand, false);
    }
    toolbar = toolbar.nextSibling;
  }
}

function onViewToolbarCommand(aEvent)
{
  var index = aEvent.originalTarget.getAttribute("toolbarindex");
  var toolbar = gNavToolbox.childNodes[index];

  toolbar.collapsed = aEvent.originalTarget.getAttribute("checked") != "true";
  document.persist(toolbar.id, "collapsed");
}

function displaySecurityInfo()
{
  BrowserPageInfo(null, "securityTab");
}

/**
 * Opens or closes the sidebar identified by commandID.
 *
 * @param commandID a string identifying the sidebar to toggle; see the
 *                  note below. (Optional if a sidebar is already open.)
 * @param forceOpen boolean indicating whether the sidebar should be
 *                  opened regardless of its current state (optional).
 * @note
 * We expect to find a xul:broadcaster element with the specified ID.
 * The following attributes on that element may be used and/or modified:
 *  - id           (required) the string to match commandID. The convention
 *                 is to use this naming scheme: 'view<sidebar-name>Sidebar'.
 *  - sidebarurl   (required) specifies the URL to load in this sidebar.
 *  - sidebartitle or label (in that order) specify the title to 
 *                 display on the sidebar.
 *  - checked      indicates whether the sidebar is currently displayed.
 *                 Note that toggleSidebar updates this attribute when
 *                 it changes the sidebar's visibility.
 *  - group        this attribute must be set to "sidebar".
 */
function toggleSidebar(commandID, forceOpen) {

  var sidebarBox = document.getElementById("sidebar-box");
  if (!commandID)
    commandID = sidebarBox.getAttribute("sidebarcommand");

  var sidebarBroadcaster = document.getElementById(commandID);
  var sidebar = document.getElementById("sidebar"); // xul:browser
  var sidebarTitle = document.getElementById("sidebar-title");
  var sidebarSplitter = document.getElementById("sidebar-splitter");

  if (sidebarBroadcaster.getAttribute("checked") == "true") {
    if (!forceOpen) {
      sidebarBroadcaster.removeAttribute("checked");
      sidebarBox.setAttribute("sidebarcommand", "");
      sidebarTitle.value = "";
      sidebar.setAttribute("src", "about:blank");
      sidebarBox.hidden = true;
      sidebarSplitter.hidden = true;
      content.focus();
    } else {
      fireSidebarFocusedEvent();
    }
    return;
  }

  // now we need to show the specified sidebar

  // ..but first update the 'checked' state of all sidebar broadcasters
  var broadcasters = document.getElementsByAttribute("group", "sidebar");
  for (var i = 0; i < broadcasters.length; ++i) {
    // skip elements that observe sidebar broadcasters and random
    // other elements
    if (broadcasters[i].localName != "broadcaster")
      continue;

    if (broadcasters[i] != sidebarBroadcaster)
      broadcasters[i].removeAttribute("checked");
    else
      sidebarBroadcaster.setAttribute("checked", "true");
  }

  sidebarBox.hidden = false;
  sidebarSplitter.hidden = false;

  var url = sidebarBroadcaster.getAttribute("sidebarurl");
  var title = sidebarBroadcaster.getAttribute("sidebartitle");
  if (!title)
    title = sidebarBroadcaster.getAttribute("label");
  sidebar.setAttribute("src", url); // kick off async load
  sidebarBox.setAttribute("sidebarcommand", sidebarBroadcaster.id);
  sidebarTitle.value = title;

  // We set this attribute here in addition to setting it on the <browser>
  // element itself, because the code in BrowserShutdown persists this
  // attribute, not the "src" of the <browser id="sidebar">. The reason it
  // does that is that we want to delay sidebar load a bit when a browser
  // window opens. See delayedStartup().
  sidebarBox.setAttribute("src", url);

  if (sidebar.contentDocument.location.href != url)
    sidebar.addEventListener("load", sidebarOnLoad, true);
  else // older code handled this case, so we do it too
    fireSidebarFocusedEvent();
}

function sidebarOnLoad(event) {
  var sidebar = document.getElementById("sidebar");
  sidebar.removeEventListener("load", sidebarOnLoad, true);
  // We're handling the 'load' event before it bubbles up to the usual
  // (non-capturing) event handlers. Let it bubble up before firing the
  // SidebarFocused event.
  setTimeout(fireSidebarFocusedEvent, 0);
}

/**
 * Fire a "SidebarFocused" event on the sidebar's |window| to give the sidebar
 * a chance to adjust focus as needed. An additional event is needed, because
 * we don't want to focus the sidebar when it's opened on startup or in a new
 * window, only when the user opens the sidebar.
 */
function fireSidebarFocusedEvent() {
  var sidebar = document.getElementById("sidebar");
  var event = document.createEvent("Events");
  event.initEvent("SidebarFocused", true, false);
  sidebar.contentWindow.dispatchEvent(event);
}

var gHomeButton = {
  prefDomain: "browser.startup.homepage",
  observe: function (aSubject, aTopic, aPrefName)
  {
    if (aTopic != "nsPref:changed" || aPrefName != this.prefDomain)
      return;

    this.updateTooltip();
  },

  updateTooltip: function (homeButton)
  {
    if (!homeButton)
      homeButton = document.getElementById("home-button");
    if (homeButton) {
      var homePage = this.getHomePage();
      homePage = homePage.replace(/\|/g,', ');
      homeButton.setAttribute("tooltiptext", homePage);
    }
  },

  getHomePage: function ()
  {
    var url;
    try {
      url = gPrefService.getComplexValue(this.prefDomain,
                                Components.interfaces.nsIPrefLocalizedString).data;
    } catch (e) {
    }

    // use this if we can't find the pref
    if (!url) {
      var SBS = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
      var configBundle = SBS.createBundle("resource:/browserconfig.properties");
      url = configBundle.GetStringFromName(this.prefDomain);
    }

    return url;
  },

  updatePersonalToolbarStyle: function (homeButton)
  {
    if (!homeButton)
      homeButton = document.getElementById("home-button");
    if (homeButton)
      homeButton.className = homeButton.parentNode.id == "PersonalToolbar"
                               || homeButton.parentNode.parentNode.id == "PersonalToolbar" ?
                             homeButton.className.replace("toolbarbutton-1", "bookmark-item") :
                             homeButton.className.replace("bookmark-item", "toolbarbutton-1");
  }
};

/**
 * Gets the selected text in the active browser. Leading and trailing
 * whitespace is removed, and consecutive whitespace is replaced by a single
 * space. A maximum of 150 characters will be returned, regardless of the value
 * of aCharLen.
 *
 * @param aCharLen
 *        The maximum number of characters to return.
 */
function getBrowserSelection(aCharLen) {
  // selections of more than 150 characters aren't useful
  const kMaxSelectionLen = 150;
  const charLen = Math.min(aCharLen || kMaxSelectionLen, kMaxSelectionLen);

  var focusedWindow = document.commandDispatcher.focusedWindow;
  var selection = focusedWindow.getSelection().toString();

  if (selection) {
    if (selection.length > charLen) {
      // only use the first charLen important chars. see bug 221361
      var pattern = new RegExp("^(?:\\s*.){0," + charLen + "}");
      pattern.test(selection);
      selection = RegExp.lastMatch;
    }

    selection = selection.replace(/^\s+/, "")
                         .replace(/\s+$/, "")
                         .replace(/\s+/g, " ");

    if (selection.length > charLen)
      selection = selection.substr(0, charLen);
  }
  return selection;
}

var gWebPanelURI;
function openWebPanel(aTitle, aURI)
{
    // Ensure that the web panels sidebar is open.
    toggleSidebar('viewWebPanelsSidebar', true);

    // Set the title of the panel.
    document.getElementById("sidebar-title").value = aTitle;

    // Tell the Web Panels sidebar to load the bookmark.
    var sidebar = document.getElementById("sidebar");
    if (sidebar.docShell && sidebar.contentDocument && sidebar.contentDocument.getElementById('web-panels-browser')) {
        sidebar.contentWindow.loadWebPanel(aURI);
        if (gWebPanelURI) {
            gWebPanelURI = "";
            sidebar.removeEventListener("load", asyncOpenWebPanel, true);
        }
    }
    else {
        // The panel is still being constructed.  Attach an onload handler.
        if (!gWebPanelURI)
            sidebar.addEventListener("load", asyncOpenWebPanel, true);
        gWebPanelURI = aURI;
    }
}

function asyncOpenWebPanel(event)
{
    var sidebar = document.getElementById("sidebar");
    if (gWebPanelURI && sidebar.contentDocument && sidebar.contentDocument.getElementById('web-panels-browser'))
        sidebar.contentWindow.loadWebPanel(gWebPanelURI);
    gWebPanelURI = "";
    sidebar.removeEventListener("load", asyncOpenWebPanel, true);
}

/*
 * - [ Dependencies ] ---------------------------------------------------------
 *  utilityOverlay.js:
 *    - gatherTextUnder
 */

 // Called whenever the user clicks in the content area,
 // except when left-clicking on links (special case)
 // should always return true for click to go through
 function contentAreaClick(event, fieldNormalClicks)
 {
   if (!event.isTrusted || event.getPreventDefault()) {
     return true;
   }

   var target = event.target;
   var linkNode;

   if (target instanceof HTMLAnchorElement ||
       target instanceof HTMLAreaElement ||
       target instanceof HTMLLinkElement) {
     if (target.hasAttribute("href"))
       linkNode = target;

     // xxxmpc: this is kind of a hack to work around a Gecko bug (see bug 266932)
     // we're going to walk up the DOM looking for a parent link node,
     // this shouldn't be necessary, but we're matching the existing behaviour for left click
     var parent = target.parentNode;
     while (parent) {
       if (parent instanceof HTMLAnchorElement ||
           parent instanceof HTMLAreaElement ||
           parent instanceof HTMLLinkElement) {
           if (parent.hasAttribute("href"))
             linkNode = parent;
       }
       parent = parent.parentNode;
     }
   }
   else {
     linkNode = event.originalTarget;
     while (linkNode && !(linkNode instanceof HTMLAnchorElement))
       linkNode = linkNode.parentNode;
     // <a> cannot be nested.  So if we find an anchor without an
     // href, there is no useful <a> around the target
     if (linkNode && !linkNode.hasAttribute("href"))
       linkNode = null;
   }
   var wrapper = null;
   if (linkNode) {
     wrapper = linkNode;
     if (event.button == 0 && !event.ctrlKey && !event.shiftKey &&
         !event.altKey && !event.metaKey) {
       // A Web panel's links should target the main content area.  Do this
       // if no modifier keys are down and if there's no target or the target equals
       // _main (the IE convention) or _content (the Mozilla convention).
       // XXX Now that markLinkVisited is gone, we may not need to field _main and
       // _content here.
       target = wrapper.getAttribute("target");
       if (fieldNormalClicks &&
           (!target || target == "_content" || target  == "_main"))
         // IE uses _main, SeaMonkey uses _content, we support both
       {
         if (!wrapper.href)
           return true;
         if (wrapper.getAttribute("onclick"))
           return true;
         // javascript links should be executed in the current browser
         if (wrapper.href.substr(0, 11) === "javascript:")
           return true;
         // data links should be executed in the current browser
         if (wrapper.href.substr(0, 5) === "data:")
           return true;

         try {
           urlSecurityCheck(wrapper.href, wrapper.ownerDocument.nodePrincipal);
         }
         catch(ex) {
           return false;
         } 

         var postData = { };
         var url = getShortcutOrURI(wrapper.href, postData);
         if (!url)
           return true;
         loadURI(url, null, postData.value, false);
         event.preventDefault();
         return false;
       }
       else if (linkNode.getAttribute("rel") == "sidebar") {
         // This is the Opera convention for a special link that - when clicked - allows
         // you to add a sidebar panel.  We support the Opera convention here.  The link's
         // title attribute contains the title that should be used for the sidebar panel.
         PlacesUIUtils.showMinimalAddBookmarkUI(makeURI(wrapper.href),
                                                wrapper.getAttribute("title"),
                                                null, null, true, true);
         event.preventDefault();
         return false;
       }
     }
     else {
       handleLinkClick(event, wrapper.href, linkNode);
     }

     return true;
   } else {
     // Try simple XLink
     var href, realHref, baseURI;
     linkNode = target;
     while (linkNode) {
       if (linkNode.nodeType == Node.ELEMENT_NODE) {
         wrapper = linkNode;

         realHref = wrapper.getAttributeNS("http://www.w3.org/1999/xlink", "href");
         if (realHref) {
           href = realHref;
           baseURI = wrapper.baseURI
         }
       }
       linkNode = linkNode.parentNode;
     }
     if (href) {
       href = makeURLAbsolute(baseURI, href);
       handleLinkClick(event, href, null);
       return true;
     }
   }
   if (event.button == 1 &&
       gPrefService.getBoolPref("middlemouse.contentLoadURL") &&
       !gPrefService.getBoolPref("general.autoScroll")) {
     middleMousePaste(event);
   }
   return true;
 }

function handleLinkClick(event, href, linkNode)
{
  var doc = event.target.ownerDocument;

  switch (event.button) {
    case 0:    // if left button clicked
#ifdef XP_MACOSX
      if (event.metaKey) { // Cmd
#else
      if (event.ctrlKey) {
#endif
        openNewTabWith(href, doc, null, event, false);
        event.stopPropagation();
        return true;
      }

      if (event.shiftKey && event.altKey) {
        var feedService = 
            Cc["@mozilla.org/browser/feeds/result-service;1"].
            getService(Ci.nsIFeedResultService);
        feedService.forcePreviewPage = true;
        loadURI(href, null, null, false);
        return false;
      }
                                                       
      if (event.shiftKey) {
        openNewWindowWith(href, doc, null, false);
        event.stopPropagation();
        return true;
      }

      if (event.altKey) {
        saveURL(href, linkNode ? gatherTextUnder(linkNode) : "", null, true,
                true, doc.documentURIObject);
        return true;
      }

      return false;
    case 1:    // if middle button clicked
      var tab;
      try {
        tab = gPrefService.getBoolPref("browser.tabs.opentabfor.middleclick")
      }
      catch(ex) {
        tab = true;
      }
      if (tab)
        openNewTabWith(href, doc, null, event, false);
      else
        openNewWindowWith(href, doc, null, false);
      event.stopPropagation();
      return true;
  }
  return false;
}

function middleMousePaste(event)
{
  var url = readFromClipboard();
  if (!url)
    return;

  var postData = { };
  url = getShortcutOrURI(url, postData);
  if (!url)
    return;

  try {
    addToUrlbarHistory(url);
  } catch (ex) {
    // Things may go wrong when adding url to session history,
    // but don't let that interfere with the loading of the url.
    Cu.reportError(ex);
  }

  openUILink(url,
             event,
             true /* ignore the fact this is a middle click */);

  event.stopPropagation();
}

/*
 * Note that most of this routine has been moved into C++ in order to
 * be available for all <browser> tags as well as gecko embedding. See
 * mozilla/content/base/src/nsContentAreaDragDrop.cpp.
 *
 * Do not add any new fuctionality here other than what is needed for
 * a standalone product.
 */

var contentAreaDNDObserver = {
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);

      // valid urls don't contain spaces ' '; if we have a space it
      // isn't a valid url, or if it's a javascript: or data: url,
      // bail out
      if (!url || !url.length || url.indexOf(" ", 0) != -1 ||
          /^\s*(javascript|data):/.test(url))
        return;

      nsDragAndDrop.dragDropSecurityCheck(aEvent, aDragSession, url);

      switch (document.documentElement.getAttribute('windowtype')) {
        case "navigator:browser":
          var postData = { };
          var uri = getShortcutOrURI(url, postData);
          loadURI(uri, null, postData.value, false);
          break;
        case "navigator:view-source":
          viewSource(url);
          break;
      }

      // keep the event from being handled by the dragDrop listeners
      // built-in to gecko if they happen to be above us.
      aEvent.preventDefault();
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    }

};

function MultiplexHandler(event)
{ try {
    var node = event.target;
    var name = node.getAttribute('name');

    if (name == 'detectorGroup') {
        SetForcedDetector(true);
        SelectDetector(event, false);
    } else if (name == 'charsetGroup') {
        var charset = node.getAttribute('id');
        charset = charset.substring('charset.'.length, charset.length)
        SetForcedCharset(charset);
    } else if (name == 'charsetCustomize') {
        //do nothing - please remove this else statement, once the charset prefs moves to the pref window
    } else {
        SetForcedCharset(node.getAttribute('id'));
    }
    } catch(ex) { alert(ex); }
}

function SelectDetector(event, doReload)
{
    var uri =  event.target.getAttribute("id");
    var prefvalue = uri.substring('chardet.'.length, uri.length);
    if ("off" == prefvalue) { // "off" is special value to turn off the detectors
        prefvalue = "";
    }

    try {
        var pref = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
        var str =  Components.classes["@mozilla.org/supports-string;1"]
                             .createInstance(Components.interfaces.nsISupportsString);

        str.data = prefvalue;
        pref.setComplexValue("intl.charset.detector",
                             Components.interfaces.nsISupportsString, str);
        if (doReload) window.content.location.reload();
    }
    catch (ex) {
        dump("Failed to set the intl.charset.detector preference.\n");
    }
}

function SetForcedDetector(doReload)
{
    BrowserSetForcedDetector(doReload);
}

function SetForcedCharset(charset)
{
    BrowserSetForcedCharacterSet(charset);
}

function BrowserSetForcedCharacterSet(aCharset)
{
  var docCharset = gBrowser.docShell.QueryInterface(Ci.nsIDocCharset);
  docCharset.charset = aCharset;
  // Save the forced character-set
  PlacesUtils.history.setCharsetForURI(getWebNavigation().currentURI, aCharset);
  BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function BrowserSetForcedDetector(doReload)
{
  gBrowser.documentCharsetInfo.forcedDetector = true;
  if (doReload)
    BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function UpdateCurrentCharset()
{
    // extract the charset from DOM
    var wnd = document.commandDispatcher.focusedWindow;
    if ((window == wnd) || (wnd == null)) wnd = window.content;

    // Uncheck previous item
    if (gPrevCharset) {
        var pref_item = document.getElementById('charset.' + gPrevCharset);
        if (pref_item)
          pref_item.setAttribute('checked', 'false');
    }

    var menuitem = document.getElementById('charset.' + wnd.document.characterSet);
    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateCharsetDetector()
{
    var prefvalue;

    try {
        var pref = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch);
        prefvalue = pref.getComplexValue("intl.charset.detector",
                                         Components.interfaces.nsIPrefLocalizedString).data;
    }
    catch (ex) {
        prefvalue = "";
    }

    if (prefvalue == "") prefvalue = "off";
    dump("intl.charset.detector = "+ prefvalue + "\n");

    prefvalue = 'chardet.' + prefvalue;
    var menuitem = document.getElementById(prefvalue);

    if (menuitem) {
        menuitem.setAttribute('checked', 'true');
    }
}

function UpdateMenus(event)
{
    // use setTimeout workaround to delay checkmark the menu
    // when onmenucomplete is ready then use it instead of oncreate
    // see bug 78290 for the detail
    UpdateCurrentCharset();
    setTimeout(UpdateCurrentCharset, 0);
    UpdateCharsetDetector();
    setTimeout(UpdateCharsetDetector, 0);
}

function CreateMenu(node)
{
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(null, "charsetmenu-selected", node);
}

function charsetLoadListener (event)
{
    var charset = window.content.document.characterSet;

    if (charset.length > 0 && (charset != gLastBrowserCharset)) {
        if (!gCharsetMenu)
          gCharsetMenu = Components.classes['@mozilla.org/rdf/datasource;1?name=charset-menu'].getService().QueryInterface(Components.interfaces.nsICurrentCharsetListener);
        gCharsetMenu.SetCurrentCharset(charset);
        gPrevCharset = gLastBrowserCharset;
        gLastBrowserCharset = charset;
    }
}

/* Begin Page Style Functions */
function getAllStyleSheets(frameset) {
  var styleSheetsArray = Array.slice(frameset.document.styleSheets);
  for (let i = 0; i < frameset.frames.length; i++) {
    let frameSheets = getAllStyleSheets(frameset.frames[i]);
    styleSheetsArray = styleSheetsArray.concat(frameSheets);
  }
  return styleSheetsArray;
}

function stylesheetFillPopup(menuPopup) {
  var noStyle = menuPopup.firstChild;
  var persistentOnly = noStyle.nextSibling;
  var sep = persistentOnly.nextSibling;
  while (sep.nextSibling)
    menuPopup.removeChild(sep.nextSibling);

  var styleSheets = getAllStyleSheets(window.content);
  var currentStyleSheets = {};
  var styleDisabled = getMarkupDocumentViewer().authorStyleDisabled;
  var haveAltSheets = false;
  var altStyleSelected = false;

  for (let i = 0; i < styleSheets.length; ++i) {
    let currentStyleSheet = styleSheets[i];

    if (!currentStyleSheet.title)
      continue;

    // Skip any stylesheets that don't match the screen media type.
    let (media = currentStyleSheet.media.mediaText.toLowerCase()) {
      if (media && (media.indexOf("screen") == -1) && (media.indexOf("all") == -1))
        continue;
    }

    if (!currentStyleSheet.disabled)
      altStyleSelected = true;

    haveAltSheets = true;

    let lastWithSameTitle = null;
    if (currentStyleSheet.title in currentStyleSheets)
      lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];

    if (!lastWithSameTitle) {
      let menuItem = document.createElement("menuitem");
      menuItem.setAttribute("type", "radio");
      menuItem.setAttribute("label", currentStyleSheet.title);
      menuItem.setAttribute("data", currentStyleSheet.title);
      menuItem.setAttribute("checked", !currentStyleSheet.disabled && !styleDisabled);
      menuPopup.appendChild(menuItem);
      currentStyleSheets[currentStyleSheet.title] = menuItem;
    } else if (currentStyleSheet.disabled) {
      lastWithSameTitle.removeAttribute("checked");
    }
  }

  noStyle.setAttribute("checked", styleDisabled);
  persistentOnly.setAttribute("checked", !altStyleSelected && !styleDisabled);
  persistentOnly.hidden = (window.content.document.preferredStyleSheetSet) ? haveAltSheets : false;
  sep.hidden = (noStyle.hidden && persistentOnly.hidden) || !haveAltSheets;
  return true;
}

function stylesheetInFrame(frame, title) {
  return Array.some(frame.document.styleSheets,
                    function (stylesheet) stylesheet.title == title);
}

function stylesheetSwitchFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (let i = 0; i < docStyleSheets.length; ++i) {
    let docStyleSheet = docStyleSheets[i];

    if (title == "_nostyle")
      docStyleSheet.disabled = true;
    else if (docStyleSheet.title)
      docStyleSheet.disabled = (docStyleSheet.title != title);
    else if (docStyleSheet.disabled)
      docStyleSheet.disabled = false;
  }
}

function stylesheetSwitchAll(frameset, title) {
  if (!title || title == "_nostyle" || stylesheetInFrame(frameset, title))
    stylesheetSwitchFrame(frameset, title);

  for (let i = 0; i < frameset.frames.length; i++)
    stylesheetSwitchAll(frameset.frames[i], title);
}

function setStyleDisabled(disabled) {
  getMarkupDocumentViewer().authorStyleDisabled = disabled;
}
/* End of the Page Style functions */

var BrowserOffline = {
  /////////////////////////////////////////////////////////////////////////////
  // BrowserOffline Public Methods
  init: function ()
  {
    if (!this._uiElement)
      this._uiElement = document.getElementById("goOfflineMenuitem");

    var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "network:offline-status-changed", false);

    var ioService = Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService2);

    // if ioService is managing the offline status, then ioservice.offline
    // is already set correctly. We will continue to allow the ioService
    // to manage its offline state until the user uses the "Work Offline" UI.
    
    if (!ioService.manageOfflineStatus) {
      // set the initial state
      var isOffline = false;
      try {
        isOffline = gPrefService.getBoolPref("browser.offline");
      }
      catch (e) { }
      ioService.offline = isOffline;
    }
    
    this._updateOfflineUI(ioService.offline);
  },

  uninit: function ()
  {
    try {
      var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
      os.removeObserver(this, "network:offline-status-changed");
    } catch (ex) {
    }
  },

  toggleOfflineStatus: function ()
  {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService2);

    // Stop automatic management of the offline status
    try {
      ioService.manageOfflineStatus = false;
    } catch (ex) {
    }
  
    if (!ioService.offline && !this._canGoOffline()) {
      this._updateOfflineUI(false);
      return;
    }

    ioService.offline = !ioService.offline;

    // Save the current state for later use as the initial state
    // (if there is no netLinkService)
    gPrefService.setBoolPref("browser.offline", ioService.offline);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIObserver
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic != "network:offline-status-changed")
      return;

    this._updateOfflineUI(aState == "offline");
  },

  /////////////////////////////////////////////////////////////////////////////
  // BrowserOffline Implementation Methods
  _canGoOffline: function ()
  {
    var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
    if (os) {
      try {
        var cancelGoOffline = Components.classes["@mozilla.org/supports-PRBool;1"].createInstance(Components.interfaces.nsISupportsPRBool);
        os.notifyObservers(cancelGoOffline, "offline-requested", null);

        // Something aborted the quit process.
        if (cancelGoOffline.data)
          return false;
      }
      catch (ex) {
      }
    }
    return true;
  },

  _uiElement: null,
  _updateOfflineUI: function (aOffline)
  {
    var offlineLocked = gPrefService.prefIsLocked("network.online");
    if (offlineLocked)
      this._uiElement.setAttribute("disabled", "true");

    this._uiElement.setAttribute("checked", aOffline);
  }
};

var OfflineApps = {
  /////////////////////////////////////////////////////////////////////////////
  // OfflineApps Public Methods
  init: function ()
  {
    var obs = Cc["@mozilla.org/observer-service;1"].
              getService(Ci.nsIObserverService);
    obs.addObserver(this, "dom-storage-warn-quota-exceeded", false);
    obs.addObserver(this, "offline-cache-update-completed", false);
  },

  uninit: function ()
  {
    var obs = Cc["@mozilla.org/observer-service;1"].
              getService(Ci.nsIObserverService);
    obs.removeObserver(this, "dom-storage-warn-quota-exceeded");
    obs.removeObserver(this, "offline-cache-update-completed");
  },

  /////////////////////////////////////////////////////////////////////////////
  // OfflineApps Implementation Methods

  // XXX: _getBrowserWindowForContentWindow and _getBrowserForContentWindow
  // were taken from browser/components/feeds/src/WebContentConverter.
  _getBrowserWindowForContentWindow: function(aContentWindow) {
    return aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShellTreeItem)
                         .rootTreeItem
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow)
                         .wrappedJSObject;
  },

  _getBrowserForContentWindow: function(aBrowserWindow, aContentWindow) {
    // This depends on pseudo APIs of browser.js and tabbrowser.xml
    aContentWindow = aContentWindow.top;
    var browsers = aBrowserWindow.getBrowser().browsers;
    for (var i = 0; i < browsers.length; ++i) {
      if (browsers[i].contentWindow == aContentWindow)
        return browsers[i];
    }
  },

  _getManifestURI: function(aWindow) {
    var attr = aWindow.document.documentElement.getAttribute("manifest");
    if (!attr) return null;

    try {
      var ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);

      var contentURI = ios.newURI(aWindow.location.href, null, null);
      return ios.newURI(attr, aWindow.document.characterSet, contentURI);
    } catch (e) {
      return null;
    }
  },

  // A cache update isn't tied to a specific window.  Try to find
  // the best browser in which to warn the user about space usage
  _getBrowserForCacheUpdate: function(aCacheUpdate) {
    // Prefer the current browser
    var uri = this._getManifestURI(gBrowser.mCurrentBrowser.contentWindow);
    if (uri && uri.equals(aCacheUpdate.manifestURI)) {
      return gBrowser.mCurrentBrowser;
    }

    var browsers = gBrowser.browsers;
    for (var i = 0; i < browsers.length; ++i) {
      uri = this._getManifestURI(browsers[i].contentWindow);
      if (uri && uri.equals(aCacheUpdate.manifestURI)) {
        return browsers[i];
      }
    }

    return null;
  },

  _warnUsage: function(aBrowser, aURI) {
    if (!aBrowser)
      return;

    var notificationBox = gBrowser.getNotificationBox(aBrowser);
    var notification = notificationBox.getNotificationWithValue("offline-app-usage");
    if (!notification) {
      var bundle_browser = document.getElementById("bundle_browser");

      var buttons = [{
          label: bundle_browser.getString("offlineApps.manageUsage"),
          accessKey: bundle_browser.getString("offlineApps.manageUsageAccessKey"),
          callback: OfflineApps.manage
        }];

      var warnQuota = gPrefService.getIntPref("offline-apps.quota.warn");
      const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
      var message = bundle_browser.getFormattedString("offlineApps.usage",
                                                      [ aURI.host,
                                                        warnQuota / 1024 ]);

      notificationBox.appendNotification(message, "offline-app-usage",
                                         "chrome://browser/skin/Info.png",
                                         priority, buttons);
    }

    // Now that we've warned once, prevent the warning from showing up
    // again.
    var pm = Cc["@mozilla.org/permissionmanager;1"].
             getService(Ci.nsIPermissionManager);
    pm.add(aURI, "offline-app",
           Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);
  },

  // XXX: duplicated in preferences/advanced.js
  _getOfflineAppUsage: function (host)
  {
    // XXX Bug 442810: include offline cache usage.
    var usage = 0;
    var storageManager = Components.classes["@mozilla.org/dom/storagemanager;1"].
                         getService(Components.interfaces.nsIDOMStorageManager);
    usage += storageManager.getUsage(host);

    return usage;
  },

  _checkUsage: function(aURI) {
    var pm = Cc["@mozilla.org/permissionmanager;1"].
             getService(Ci.nsIPermissionManager);

    // if the user has already allowed excessive usage, don't bother checking
    if (pm.testExactPermission(aURI, "offline-app") !=
        Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN) {
      var usage = this._getOfflineAppUsage(aURI.asciiHost);
      var warnQuota = gPrefService.getIntPref("offline-apps.quota.warn");
      if (usage >= warnQuota * 1024) {
        return true;
      }
    }

    return false;
  },

  offlineAppRequested: function(aContentWindow) {
    if (!gPrefService.getBoolPref("browser.offline-apps.notify")) {
      return;
    }

    var browserWindow = this._getBrowserWindowForContentWindow(aContentWindow);
    var browser = this._getBrowserForContentWindow(browserWindow,
                                                   aContentWindow);

    var currentURI = browser.webNavigation.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].
             getService(Ci.nsIPermissionManager);

    // don't bother showing UI if the user has already made a decision
    if (pm.testExactPermission(currentURI, "offline-app") !=
        Ci.nsIPermissionManager.UNKNOWN_ACTION)
      return;

    try {
      if (gPrefService.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch(e) {
      // this pref isn't set by default, ignore failures
    }

    var notificationBox = gBrowser.getNotificationBox(browser);
    var notification = notificationBox.getNotificationWithValue("offline-app-requested");
    if (!notification) {
      var bundle_browser = document.getElementById("bundle_browser");

      var buttons = [{
        label: bundle_browser.getString("offlineApps.allow"),
        accessKey: bundle_browser.getString("offlineApps.allowAccessKey"),
        callback: function() { OfflineApps.allowSite(); }
      },{
        label: bundle_browser.getString("offlineApps.never"),
        accessKey: bundle_browser.getString("offlineApps.neverAccessKey"),
        callback: function() { OfflineApps.disallowSite(); }
      },{
        label: bundle_browser.getString("offlineApps.notNow"),
        accessKey: bundle_browser.getString("offlineApps.notNowAccessKey"),
        callback: function() { /* noop */ }
      }];

      const priority = notificationBox.PRIORITY_INFO_LOW;
      var message = bundle_browser.getFormattedString("offlineApps.available",
                                                      [ currentURI.host ]);
      notificationBox.appendNotification(message, "offline-app-requested",
                                         "chrome://browser/skin/Info.png",
                                         priority, buttons);
    }
  },

  allowSite: function() {
    var currentURI = gBrowser.selectedBrowser.webNavigation.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].
             getService(Ci.nsIPermissionManager);
    pm.add(currentURI, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);

    // When a site is enabled while loading, <link rel="offline-resource">
    // resources will start fetching immediately.  This one time we need to
    // do it ourselves.
    this._startFetching();
  },

  disallowSite: function() {
    var currentURI = gBrowser.selectedBrowser.webNavigation.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].
             getService(Ci.nsIPermissionManager);
    pm.add(currentURI, "offline-app", Ci.nsIPermissionManager.DENY_ACTION);
  },

  manage: function() {
    openAdvancedPreferences("networkTab");
  },

  _startFetching: function() {
    var manifest = content.document.documentElement.getAttribute("manifest");
    if (!manifest)
      return;

    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);

    var contentURI = ios.newURI(content.location.href, null, null);
    var manifestURI = ios.newURI(manifest, content.document.characterSet,
                                 contentURI);

    var updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].
                        getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, contentURI);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIObserver
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic == "dom-storage-warn-quota-exceeded") {
      if (aSubject) {
        var uri = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService).
                  newURI(aSubject.location.href, null, null);

        if (OfflineApps._checkUsage(uri)) {
          var browserWindow =
            this._getBrowserWindowForContentWindow(aSubject);
          var browser = this._getBrowserForContentWindow(browserWindow,
                                                         aSubject);
          OfflineApps._warnUsage(browser, uri);
        }
      }
    } else if (aTopic == "offline-cache-update-completed") {
      var cacheUpdate = aSubject.QueryInterface(Ci.nsIOfflineCacheUpdate);

      var uri = cacheUpdate.manifestURI;
      if (OfflineApps._checkUsage(uri)) {
        var browser = this._getBrowserForCacheUpdate(cacheUpdate);
        if (browser) {
          OfflineApps._warnUsage(browser, cacheUpdate.manifestURI);
        }
      }
    }
  }
};

function WindowIsClosing()
{
  var cn = gBrowser.tabContainer.childNodes;
  var numtabs = cn.length;
  var reallyClose = true;

  for (var i = 0; reallyClose && i < numtabs; ++i) {
    var ds = gBrowser.getBrowserForTab(cn[i]).docShell;

    if (ds.contentViewer && !ds.contentViewer.permitUnload())
      reallyClose = false;
  }

  if (!reallyClose)
    return false;

  // closeWindow takes a second optional function argument to open up a
  // window closing warning dialog if we're not quitting. (Quitting opens
  // up another dialog so we don't need to.)
  return closeWindow(false,
    function () {
      return gBrowser.warnAboutClosingTabs(true);
    });
}

var MailIntegration = {
  sendLinkForWindow: function (aWindow) {
    this.sendMessage(aWindow.location.href,
                     aWindow.document.title);
  },

  sendMessage: function (aBody, aSubject) {
    // generate a mailto url based on the url and the url's title
    var mailtoUrl = "mailto:";
    if (aBody) {
      mailtoUrl += "?body=" + encodeURIComponent(aBody);
      mailtoUrl += "&subject=" + encodeURIComponent(aSubject);
    }

    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    var uri = ioService.newURI(mailtoUrl, null, null);

    // now pass this uri to the operating system
    this._launchExternalUrl(uri);
  },

  // a generic method which can be used to pass arbitrary urls to the operating
  // system.
  // aURL --> a nsIURI which represents the url to launch
  _launchExternalUrl: function (aURL) {
    var extProtocolSvc =
       Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                 .getService(Components.interfaces.nsIExternalProtocolService);
    if (extProtocolSvc)
      extProtocolSvc.loadUrl(aURL);
  }
};

function BrowserOpenAddonsMgr(aPane)
{
  const EMTYPE = "Extension:Manager";
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var theEM = wm.getMostRecentWindow(EMTYPE);
  if (theEM) {
    theEM.focus();
    if (aPane)
      theEM.showView(aPane);
    return;
  }

  const EMURL = "chrome://mozapps/content/extensions/extensions.xul";
  const EMFEATURES = "chrome,menubar,extra-chrome,toolbar,dialog=no,resizable";
  if (aPane)
    window.openDialog(EMURL, "", EMFEATURES, aPane);
  else
    window.openDialog(EMURL, "", EMFEATURES);
}

function escapeNameValuePair(aName, aValue, aIsFormUrlEncoded)
{
  if (aIsFormUrlEncoded)
    return escape(aName + "=" + aValue);
  else
    return escape(aName) + "=" + escape(aValue);
}

function AddKeywordForSearchField()
{
  var node = document.popupNode;

  var charset = node.ownerDocument.characterSet;

  var docURI = makeURI(node.ownerDocument.URL,
                       charset);

  var formURI = makeURI(node.form.getAttribute("action"),
                        charset,
                        docURI);

  var spec = formURI.spec;

  var isURLEncoded = 
               (node.form.method.toUpperCase() == "POST"
                && (node.form.enctype == "application/x-www-form-urlencoded" ||
                    node.form.enctype == ""));

  var el, type;
  var formData = [];

  for (var i=0; i < node.form.elements.length; i++) {
    el = node.form.elements[i];

    if (!el.type) // happens with fieldsets
      continue;

    if (el == node) {
      formData.push((isURLEncoded) ? escapeNameValuePair(el.name, "%s", true) :
                                     // Don't escape "%s", just append
                                     escapeNameValuePair(el.name, "", false) + "%s");
      continue;
    }

    type = el.type.toLowerCase();
    
    if ((type == "text" || type == "hidden" || type == "textarea") ||
        ((type == "checkbox" || type == "radio") && el.checked)) {
      formData.push(escapeNameValuePair(el.name, el.value, isURLEncoded));
    } else if (el instanceof HTMLSelectElement && el.selectedIndex >= 0) {
      for (var j=0; j < el.options.length; j++) {
        if (el.options[j].selected)
          formData.push(escapeNameValuePair(el.name, el.options[j].value,
                                            isURLEncoded));
      }
    }
  }

  var postData;

  if (isURLEncoded)
    postData = formData.join("&");
  else
    spec += "?" + formData.join("&");

  var description = PlacesUIUtils.getDescriptionFromDocument(node.ownerDocument);
  PlacesUIUtils.showMinimalAddBookmarkUI(makeURI(spec), "", description, null,
                                         null, null, "", postData, charset);
}

function SwitchDocumentDirection(aWindow) {
  aWindow.document.dir = (aWindow.document.dir == "ltr" ? "rtl" : "ltr");
  for (var run = 0; run < aWindow.frames.length; run++)
    SwitchDocumentDirection(aWindow.frames[run]);
}

function getPluginInfo(pluginElement)
{
  var tagMimetype;
  var pluginsPage;
  if (pluginElement instanceof HTMLAppletElement) {
    tagMimetype = "application/x-java-vm";
  } else {
    if (pluginElement instanceof HTMLObjectElement) {
      pluginsPage = pluginElement.getAttribute("codebase");
    } else {
      pluginsPage = pluginElement.getAttribute("pluginspage");
    }

    // only attempt if a pluginsPage is defined.
    if (pluginsPage) {
      var doc = pluginElement.ownerDocument;
      var docShell = findChildShell(doc, gBrowser.selectedBrowser.docShell, null);
      try {
        pluginsPage = makeURI(pluginsPage, doc.characterSet, docShell.currentURI).spec;
      } catch (ex) { 
        pluginsPage = "";
      }
    }

    tagMimetype = pluginElement.QueryInterface(Components.interfaces.nsIObjectLoadingContent)
                               .actualType;

    if (tagMimetype == "") {
      tagMimetype = pluginElement.type;
    }
  }

  return {mimetype: tagMimetype, pluginsPage: pluginsPage};
}

function missingPluginInstaller(){
}

missingPluginInstaller.prototype.installSinglePlugin = function(aEvent){
  var missingPluginsArray = {};

  var pluginInfo = getPluginInfo(aEvent.target);
  missingPluginsArray[pluginInfo.mimetype] = pluginInfo;

  if (missingPluginsArray) {
    window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                      "PFSWindow", "chrome,centerscreen,resizable=yes",
                      {plugins: missingPluginsArray, browser: gBrowser.selectedBrowser});
  }

  aEvent.stopPropagation();
}

missingPluginInstaller.prototype.managePlugins = function(aEvent){
  BrowserOpenAddonsMgr("plugins");
  aEvent.stopPropagation();
}

missingPluginInstaller.prototype.newMissingPlugin = function(aEvent){
  // Since we are expecting also untrusted events, make sure
  // that the target is a plugin
  if (!(aEvent.target instanceof Components.interfaces.nsIObjectLoadingContent))
    return;

  // For broken non-object plugin tags, register a click handler so
  // that the user can click the plugin replacement to get the new
  // plugin. Object tags can, and often do, deal with that themselves,
  // so don't stomp on the page developers toes.

  if (aEvent.type != "PluginBlocklisted" &&
      !(aEvent.target instanceof HTMLObjectElement)) {
    aEvent.target.addEventListener("click",
                                   gMissingPluginInstaller.installSinglePlugin,
                                   true);
  }

  try {
    if (gPrefService.getBoolPref("plugins.hide_infobar_for_missing_plugin"))
      return;
  } catch (ex) {} // if the pref is missing, treat it as false, which shows the infobar

  var browser = gBrowser.getBrowserForDocument(aEvent.target.ownerDocument
                                                     .defaultView.top.document);
  if (!browser.missingPlugins)
    browser.missingPlugins = {};

  var pluginInfo = getPluginInfo(aEvent.target);

  browser.missingPlugins[pluginInfo.mimetype] = pluginInfo;

  var notificationBox = gBrowser.getNotificationBox(browser);

  // If there is already a missing plugin notification then do nothing
  if (notificationBox.getNotificationWithValue("missing-plugins"))
    return;
  var blockedNotification = notificationBox.getNotificationWithValue("blocked-plugins");
  var priority = notificationBox.PRIORITY_WARNING_MEDIUM;

  if (aEvent.type == "PluginBlocklisted") {
    if (blockedNotification)
      return;

    let iconURL = "chrome://mozapps/skin/plugins/pluginBlocked-16.png";
    let messageString = gNavigatorBundle.getString("blockedpluginsMessage.title");
    let buttons = [{
      label: gNavigatorBundle.getString("blockedpluginsMessage.infoButton.label"),
      accessKey: gNavigatorBundle.getString("blockedpluginsMessage.infoButton.accesskey"),
      popup: null,
      callback: blocklistInfo
    }, {
      label: gNavigatorBundle.getString("blockedpluginsMessage.searchButton.label"),
      accessKey: gNavigatorBundle.getString("blockedpluginsMessage.searchButton.accesskey"),
      popup: null,
      callback: pluginsMissing
    }];

    notificationBox.appendNotification(messageString, "blocked-plugins",
                                       iconURL, priority, buttons);
  }
  else if (aEvent.type == "PluginNotFound") {
    // Cancel any notification about blocklisting
    if (blockedNotification)
      blockedNotification.close();

    let iconURL = "chrome://mozapps/skin/plugins/pluginGeneric-16.png";
    let messageString = gNavigatorBundle.getString("missingpluginsMessage.title");
    let buttons = [{
      label: gNavigatorBundle.getString("missingpluginsMessage.button.label"),
      accessKey: gNavigatorBundle.getString("missingpluginsMessage.button.accesskey"),
      popup: null,
      callback: pluginsMissing
    }];
  
    notificationBox.appendNotification(messageString, "missing-plugins",
                                       iconURL, priority, buttons);
  }
}

missingPluginInstaller.prototype.newDisabledPlugin = function(aEvent){
  // Since we are expecting also untrusted events, make sure
  // that the target is a plugin
  if (!(aEvent.target instanceof Components.interfaces.nsIObjectLoadingContent))
    return;

  aEvent.target.addEventListener("click",
                                 gMissingPluginInstaller.managePlugins,
                                 true);
}

missingPluginInstaller.prototype.refreshBrowser = function(aEvent) {
  var browser = aEvent.target;
  var notificationBox = gBrowser.getNotificationBox(browser);
  var notification = notificationBox.getNotificationWithValue("missing-plugins");

  // clear the plugin list, now that at least one plugin has been installed
  browser.missingPlugins = null;
  if (notification) {
    // reset UI
    notificationBox.removeNotification(notification);
  }
  // reload the browser to make the new plugin show.
  browser.reload();
}

function blocklistInfo()
{
  var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                            .getService(Components.interfaces.nsIURLFormatter);
  var url = formatter.formatURLPref("extensions.blocklist.detailsURL");
  gBrowser.loadOneTab(url, null, null, null, false, false);
  return true;
}

function pluginsMissing()
{
  // get the urls of missing plugins
  var missingPluginsArray = gBrowser.selectedBrowser.missingPlugins;
  if (missingPluginsArray) {
    window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                      "PFSWindow", "chrome,centerscreen,resizable=yes",
                      {plugins: missingPluginsArray, browser: gBrowser.selectedBrowser});
  }
}

var gMissingPluginInstaller = new missingPluginInstaller();

function convertFromUnicode(charset, str)
{
  try {
    var unicodeConverter = Components
       .classes["@mozilla.org/intl/scriptableunicodeconverter"]
       .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
    unicodeConverter.charset = charset;
    str = unicodeConverter.ConvertFromUnicode(str);
    return str + unicodeConverter.Finish();
  } catch(ex) {
    return null; 
  }
}

/**
 * The Feed Handler object manages discovery of RSS/ATOM feeds in web pages
 * and shows UI when they are discovered. 
 */
var FeedHandler = {
  /**
   * The click handler for the Feed icon in the location bar. Opens the
   * subscription page if user is not given a choice of feeds.
   * (Otherwise the list of available feeds will be presented to the 
   * user in a popup menu.)
   */
  onFeedButtonClick: function(event) {
    event.stopPropagation();

    if (event.target.hasAttribute("feed") &&
        event.eventPhase == Event.AT_TARGET &&
        (event.button == 0 || event.button == 1)) {
        this.subscribeToFeed(null, event);
    }
  },
  
  /**
   * Called when the user clicks on the Feed icon in the location bar. 
   * Builds a menu of unique feeds associated with the page, and if there
   * is only one, shows the feed inline in the browser window. 
   * @param   menuPopup
   *          The feed list menupopup to be populated.
   * @returns true if the menu should be shown, false if there was only
   *          one feed and the feed should be shown inline in the browser
   *          window (do not show the menupopup).
   */
  buildFeedList: function(menuPopup) {
    var feeds = gBrowser.selectedBrowser.feeds;
    if (feeds == null) {
      // XXX hack -- menu opening depends on setting of an "open"
      // attribute, and the menu refuses to open if that attribute is
      // set (because it thinks it's already open).  onpopupshowing gets
      // called after the attribute is unset, and it doesn't get unset
      // if we return false.  so we unset it here; otherwise, the menu
      // refuses to work past this point.
      menuPopup.parentNode.removeAttribute("open");
      return false;
    }

    while (menuPopup.firstChild)
      menuPopup.removeChild(menuPopup.firstChild);

    if (feeds.length == 1) {
      var feedButton = document.getElementById("feed-button");
      if (feedButton)
        feedButton.setAttribute("feed", feeds[0].href);
      return false;
    }

    // Build the menu showing the available feed choices for viewing. 
    for (var i = 0; i < feeds.length; ++i) {
      var feedInfo = feeds[i];
      var menuItem = document.createElement("menuitem");
      var baseTitle = feedInfo.title || feedInfo.href;
      var labelStr = gNavigatorBundle.getFormattedString("feedShowFeedNew", [baseTitle]);
      menuItem.setAttribute("label", labelStr);
      menuItem.setAttribute("feed", feedInfo.href);
      menuItem.setAttribute("tooltiptext", feedInfo.href);
      menuItem.setAttribute("crop", "center");
      menuPopup.appendChild(menuItem);
    }
    return true;
  },
  
  /**
   * Subscribe to a given feed.  Called when
   *   1. Page has a single feed and user clicks feed icon in location bar
   *   2. Page has a single feed and user selects Subscribe menu item
   *   3. Page has multiple feeds and user selects from feed icon popup
   *   4. Page has multiple feeds and user selects from Subscribe submenu
   * @param   href
   *          The feed to subscribe to. May be null, in which case the
   *          event target's feed attribute is examined.
   * @param   event
   *          The event this method is handling. Used to decide where 
   *          to open the preview UI. (Optional, unless href is null)
   */
  subscribeToFeed: function(href, event) {
    // Just load the feed in the content area to either subscribe or show the
    // preview UI
    if (!href)
      href = event.target.getAttribute("feed");
    urlSecurityCheck(href, gBrowser.contentPrincipal,
                     Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
    var feedURI = makeURI(href, document.characterSet);
    // Use the feed scheme so X-Moz-Is-Feed will be set
    // The value doesn't matter
    if (/^https?/.test(feedURI.scheme))
      href = "feed:" + href;
    this.loadFeed(href, event);
  },

  loadFeed: function(href, event) {
    var feeds = gBrowser.selectedBrowser.feeds;
    try {
      openUILink(href, event, false, true, false, null);
    }
    finally {
      // We might default to a livebookmarks modal dialog, 
      // so reset that if the user happens to click it again
      gBrowser.selectedBrowser.feeds = feeds;
    }
  },

  /**
   * Update the browser UI to show whether or not feeds are available when
   * a page is loaded or the user switches tabs to a page that has feeds. 
   */
  updateFeeds: function() {
    var feedButton = document.getElementById("feed-button");
    if (!this._feedMenuitem)
      this._feedMenuitem = document.getElementById("subscribeToPageMenuitem");
    if (!this._feedMenupopup)
      this._feedMenupopup = document.getElementById("subscribeToPageMenupopup");

    var feeds = gBrowser.mCurrentBrowser.feeds;
    if (!feeds || feeds.length == 0) {
      if (feedButton) {
        feedButton.removeAttribute("feeds");
        feedButton.removeAttribute("feed");
        feedButton.setAttribute("tooltiptext", 
                                gNavigatorBundle.getString("feedNoFeeds"));
      }
      this._feedMenuitem.setAttribute("disabled", "true");
      this._feedMenupopup.setAttribute("hidden", "true");
      this._feedMenuitem.removeAttribute("hidden");
    } else {
      if (feedButton) {
        feedButton.setAttribute("feeds", "true");
        feedButton.setAttribute("tooltiptext", 
                                gNavigatorBundle.getString("feedHasFeedsNew"));
      }
      
      if (feeds.length > 1) {
        this._feedMenuitem.setAttribute("hidden", "true");
        this._feedMenupopup.removeAttribute("hidden");
        if (feedButton)
          feedButton.removeAttribute("feed");
      } else {
        if (feedButton)
          feedButton.setAttribute("feed", feeds[0].href);

        this._feedMenuitem.setAttribute("feed", feeds[0].href);
        this._feedMenuitem.removeAttribute("disabled");
        this._feedMenuitem.removeAttribute("hidden");
        this._feedMenupopup.setAttribute("hidden", "true");
      }
    }
  }, 

  addFeed: function(feed, targetDoc) {
    if (feed) {
      // find which tab this is for, and set the attribute on the browser
      var browserForLink = gBrowser.getBrowserForDocument(targetDoc);
      if (!browserForLink) {
        // ??? this really shouldn't happen..
        return;
      }

      var feeds = [];
      if (browserForLink.feeds != null)
        feeds = browserForLink.feeds;

      feeds.push(feed);
      browserForLink.feeds = feeds;
      if (browserForLink == gBrowser || browserForLink == gBrowser.mCurrentBrowser) {
        var feedButton = document.getElementById("feed-button");
        if (feedButton) {
          feedButton.setAttribute("feeds", "true");
          feedButton.setAttribute("tooltiptext", 
                                  gNavigatorBundle.getString("feedHasFeedsNew"));
        }
      }
    }
  }
};

#include browser-places.js

#include browser-textZoom.js

#include browser-tabPreviews.js

HistoryMenu.toggleRecentlyClosedTabs = function PHM_toggleRecentlyClosedTabs() {
  // enable/disable the Recently Closed Tabs sub menu
  var undoPopup = document.getElementById("historyUndoPopup");

  // get closed-tabs from nsSessionStore
  var ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  // no restorable tabs, so disable menu
  if (ss.getClosedTabCount(window) == 0)
    undoPopup.parentNode.setAttribute("disabled", true);
  else
    undoPopup.parentNode.removeAttribute("disabled");
}

/**
 * Populate when the history menu is opened
 */
HistoryMenu.populateUndoSubmenu = function PHM_populateUndoSubmenu() {
  var undoPopup = document.getElementById("historyUndoPopup");

  // remove existing menu items
  while (undoPopup.hasChildNodes())
    undoPopup.removeChild(undoPopup.firstChild);

  // get closed-tabs from nsSessionStore
  var ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  // no restorable tabs, so make sure menu is disabled, and return
  if (ss.getClosedTabCount(window) == 0) {
    undoPopup.parentNode.setAttribute("disabled", true);
    return;
  }

  // enable menu
  undoPopup.parentNode.removeAttribute("disabled");

  // populate menu
  var undoItems = eval("(" + ss.getClosedTabData(window) + ")");
  for (var i = 0; i < undoItems.length; i++) {
    var m = document.createElement("menuitem");
    m.setAttribute("label", undoItems[i].title);
    if (undoItems[i].image)
      m.setAttribute("image", undoItems[i].image);
    m.setAttribute("class", "menuitem-iconic bookmark-item");
    m.setAttribute("value", i);
    m.setAttribute("oncommand", "undoCloseTab(" + i + ");");
    m.addEventListener("click", undoCloseMiddleClick, false);
    if (i == 0)
      m.setAttribute("key", "key_undoCloseTab");
    undoPopup.appendChild(m);
  }

  // "Open All in Tabs"
  var strings = gNavigatorBundle;
  undoPopup.appendChild(document.createElement("menuseparator"));
  m = undoPopup.appendChild(document.createElement("menuitem"));
  m.setAttribute("label", strings.getString("menuOpenAllInTabs.label"));
  m.setAttribute("accesskey", strings.getString("menuOpenAllInTabs.accesskey"));
  m.addEventListener("command", function() {
    for (var i = 0; i < undoItems.length; i++)
      undoCloseTab();
  }, false);
}

/**
  * Re-open a closed tab and put it to the end of the tab strip. 
  * Used for a middle click.
  * @param aEvent
  *        The event when the user clicks the menu item
  */
function undoCloseMiddleClick(aEvent) {
  if (aEvent.button != 1)
    return;

  undoCloseTab(aEvent.originalTarget.value);
  gBrowser.moveTabToEnd();
}

/**
 * Re-open a closed tab.
 * @param aIndex
 *        The index of the tab (via nsSessionStore.getClosedTabData)
 * @returns a reference to the reopened tab.
 */
function undoCloseTab(aIndex) {
  // wallpaper patch to prevent an unnecessary blank tab (bug 343895)
  var blankTabToRemove = null;
  if (gBrowser.tabContainer.childNodes.length == 1 &&
      !gPrefService.getBoolPref("browser.tabs.autoHide") &&
      gBrowser.selectedBrowser.sessionHistory.count < 2 &&
      gBrowser.selectedBrowser.currentURI.spec == "about:blank" &&
      !gBrowser.selectedBrowser.contentDocument.body.hasChildNodes() &&
      !gBrowser.selectedTab.hasAttribute("busy"))
    blankTabToRemove = gBrowser.selectedTab;

  var tab = null;
  var ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  if (ss.getClosedTabCount(window) > (aIndex || 0)) {
    tab = ss.undoCloseTab(window, aIndex || 0);
    
    if (blankTabToRemove)
      gBrowser.removeTab(blankTabToRemove);
  }
  
  return tab;
}

/**
 * Format a URL
 * eg:
 * echo formatURL("http://%LOCALE%.amo.mozilla.org/%LOCALE%/%APP%/%VERSION%/");
 * > http://en-US.amo.mozilla.org/en-US/firefox/3.0a1/
 *
 * Currently supported built-ins are LOCALE, APP, and any value from nsIXULAppInfo, uppercased.
 */
function formatURL(aFormat, aIsPref) {
  var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
  return aIsPref ? formatter.formatURLPref(aFormat) : formatter.formatURL(aFormat);
}

/**
 * This also takes care of updating the command enabled-state when tabs are
 * created or removed.
 */
function BookmarkAllTabsHandler() {
  this._command = document.getElementById("Browser:BookmarkAllTabs");
  gBrowser.addEventListener("TabOpen", this, true);
  gBrowser.addEventListener("TabClose", this, true);
  this._updateCommandState();
}

BookmarkAllTabsHandler.prototype = {
  QueryInterface: function BATH_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIDOMEventListener) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_NOINTERFACE;
  },

  _updateCommandState: function BATH__updateCommandState(aTabClose) {
    var numTabs = gBrowser.tabContainer.childNodes.length;

    // The TabClose event is fired before the tab is removed from the DOM
    if (aTabClose)
      numTabs--;

    if (numTabs > 1)
      this._command.removeAttribute("disabled");
    else
      this._command.setAttribute("disabled", "true");
  },

  doCommand: function BATH_doCommand() {
    PlacesCommandHook.bookmarkCurrentPages();
  },

  // nsIDOMEventListener
  handleEvent: function(aEvent) {
    this._updateCommandState(aEvent.type == "TabClose");
  }
};

/**
 * Utility object to handle manipulations of the identity indicators in the UI
 */
var gIdentityHandler = {
  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED       : "verifiedIdentity", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED  : "verifiedDomain",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN          : "unknownIdentity",  // No trusted identity information

  // Cache the most recent SSLStatus and Location seen in checkIdentity
  _lastStatus : null,
  _lastLocation : null,

  // smart getters
  get _stringBundle () {
    delete this._stringBundle;
    return this._stringBundle = document.getElementById("bundle_browser");
  },
  get _staticStrings () {
    delete this._staticStrings;
    this._staticStrings = {};
    this._staticStrings[this.IDENTITY_MODE_DOMAIN_VERIFIED] = {
      encryption_label: this._stringBundle.getString("identity.encrypted")
    };
    this._staticStrings[this.IDENTITY_MODE_IDENTIFIED] = {
      encryption_label: this._stringBundle.getString("identity.encrypted")
    };
    this._staticStrings[this.IDENTITY_MODE_UNKNOWN] = {
      encryption_label: this._stringBundle.getString("identity.unencrypted")
    };
    return this._staticStrings;
  },
  get _identityPopup () {
    delete this._identityPopup;
    return this._identityPopup = document.getElementById("identity-popup");
  },
  get _identityBox () {
    delete this._identityBox;
    return this._identityBox = document.getElementById("identity-box");
  },
  get _identityPopupContentBox () {
    delete this._identityPopupContentBox;
    return this._identityPopupContentBox =
      document.getElementById("identity-popup-content-box");
  },
  get _identityPopupContentHost () {
    delete this._identityPopupContentHost;
    return this._identityPopupContentHost =
      document.getElementById("identity-popup-content-host");
  },
  get _identityPopupContentOwner () {
    delete this._identityPopupContentOwner;
    return this._identityPopupContentOwner =
      document.getElementById("identity-popup-content-owner");
  },
  get _identityPopupContentSupp () {
    delete this._identityPopupContentSupp;
    return this._identityPopupContentSupp =
      document.getElementById("identity-popup-content-supplemental");
  },
  get _identityPopupContentVerif () {
    delete this._identityPopupContentVerif;
    return this._identityPopupContentVerif =
      document.getElementById("identity-popup-content-verifier");
  },
  get _identityPopupEncLabel () {
    delete this._identityPopupEncLabel;
    return this._identityPopupEncLabel =
      document.getElementById("identity-popup-encryption-label");
  },
  get _identityIconLabel () {
    delete this._identityIconLabel;
    return this._identityIconLabel = document.getElementById("identity-icon-label");
  },

  /**
   * Rebuild cache of the elements that may or may not exist depending
   * on whether there's a location bar.
   */
  _cacheElements : function() {
    delete this._identityBox;
    delete this._identityIconLabel;
    this._identityBox = document.getElementById("identity-box");
    this._identityIconLabel = document.getElementById("identity-icon-label");
  },

  /**
   * Handler for mouseclicks on the "More Information" button in the
   * "identity-popup" panel.
   */
  handleMoreInfoClick : function(event) {
    displaySecurityInfo();
    event.stopPropagation();
  },
  
  /**
   * Helper to parse out the important parts of _lastStatus (of the SSL cert in
   * particular) for use in constructing identity UI strings
  */
  getIdentityData : function() {
    var result = {};
    var status = this._lastStatus.QueryInterface(Components.interfaces.nsISSLStatus);
    var cert = status.serverCert;
    
    // Human readable name of Subject
    result.subjectOrg = cert.organization;
    
    // SubjectName fields, broken up for individual access
    if (cert.subjectName) {
      result.subjectNameFields = {};
      cert.subjectName.split(",").forEach(function(v) {
        var field = v.split("=");
        this[field[0]] = field[1];
      }, result.subjectNameFields);
      
      // Call out city, state, and country specifically
      result.city = result.subjectNameFields.L;
      result.state = result.subjectNameFields.ST;
      result.country = result.subjectNameFields.C;
    }
    
    // Human readable name of Certificate Authority
    result.caOrg =  cert.issuerOrganization || cert.issuerCommonName;
    result.cert = cert;
    
    return result;
  },
  
  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available) and, if necessary, update the UI to reflect this.  Intended to
   * be called by onSecurityChange
   * 
   * @param PRUint32 state
   * @param JS Object location that mirrors an nsLocation (i.e. has .host and
   *                           .hostname and .port)
   */
  checkIdentity : function(state, location) {
    var currentStatus = gBrowser.securityUI
                                .QueryInterface(Components.interfaces.nsISSLStatusProvider)
                                .SSLStatus;
    this._lastStatus = currentStatus;
    this._lastLocation = location;
    
    if (state & Components.interfaces.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      this.setMode(this.IDENTITY_MODE_IDENTIFIED);
    else if (state & Components.interfaces.nsIWebProgressListener.STATE_SECURE_HIGH)
      this.setMode(this.IDENTITY_MODE_DOMAIN_VERIFIED);
    else
      this.setMode(this.IDENTITY_MODE_UNKNOWN);
  },
  
  /**
   * Return the eTLD+1 version of the current hostname
   */
  getEffectiveHost : function() {
    // Cache the eTLDService if this is our first time through
    if (!this._eTLDService)
      this._eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"]
                         .getService(Ci.nsIEffectiveTLDService);
    try {
      return this._eTLDService.getBaseDomainFromHost(this._lastLocation.hostname);
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return this._lastLocation.hostname;
    }
  },
  
  /**
   * Update the UI to reflect the specified mode, which should be one of the
   * IDENTITY_MODE_* constants.
   */
  setMode : function(newMode) {
    if (!this._identityBox) {
      // No identity box means the identity box is not visible, in which
      // case there's nothing to do.
      return;
    }

    this._identityBox.className = newMode;
    this.setIdentityMessages(newMode);
    
    // Update the popup too, if it's open
    if (this._identityPopup.state == "open")
      this.setPopupMessages(newMode);
  },
  
  /**
   * Set up the messages for the primary identity UI based on the specified mode,
   * and the details of the SSL cert, where applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setIdentityMessages : function(newMode) {
    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();     
      
      // It would be sort of nice to use the CN= field in the cert, since that's
      // typically what we want here, but thanks to x509 certs being extensible,
      // it's not the only place you have to check, there can be more than one domain,
      // et cetera, ad nauseum.  We know the cert is valid for location.host, so
      // let's just use that. Check the pref to determine how much of the verified
      // hostname to show
      var icon_label = "";
      switch (gPrefService.getIntPref("browser.identity.ssl_domain_display")) {
        case 2 : // Show full domain
          icon_label = this._lastLocation.hostname;
          break;
        case 1 : // Show eTLD.
          icon_label = this.getEffectiveHost();
      }
      
      // We need a port number for all lookups.  If one hasn't been specified, use
      // the https default
      var lookupHost = this._lastLocation.host;
      if (lookupHost.indexOf(':') < 0)
        lookupHost += ":443";

      // Cache the override service the first time we need to check it
      if (!this._overrideService)
        this._overrideService = Components.classes["@mozilla.org/security/certoverride;1"]
                                          .getService(Components.interfaces.nsICertOverrideService);

      // Verifier is either the CA Org, for a normal cert, or a special string
      // for certs that are trusted because of a security exception.
      var tooltip = this._stringBundle.getFormattedString("identity.identified.verifier",
                                                          [iData.caOrg]);
      
      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      if (this._overrideService.hasMatchingOverride(this._lastLocation.hostname, 
                                                    (this._lastLocation.port || 443),
                                                    iData.cert, {}, {}))
        tooltip = this._stringBundle.getString("identity.identified.verified_by_you");
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();  
      tooltip = this._stringBundle.getFormattedString("identity.identified.verifier",
                                                      [iData.caOrg]);
      if (iData.country)
        icon_label = this._stringBundle.getFormattedString("identity.identified.title_with_country",
                                                           [iData.subjectOrg, iData.country]);
      else
        icon_label = iData.subjectOrg;
    }
    else {
      tooltip = this._stringBundle.getString("identity.unknown.tooltip");
      icon_label = "";
    }
    
    // Push the appropriate strings out to the UI
    this._identityBox.tooltipText = tooltip;
    this._identityIconLabel.value = icon_label;
  },
  
  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setPopupMessages : function(newMode) {
      
    this._identityPopup.className = newMode;
    this._identityPopupContentBox.className = newMode;
    
    // Set the static strings up front
    this._identityPopupEncLabel.textContent = this._staticStrings[newMode].encryption_label;
    
    // Initialize the optional strings to empty values
    var supplemental = "";
    var verifier = "";
    
    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();
      var host = this.getEffectiveHost();
      var owner = this._stringBundle.getString("identity.ownerUnknown2");
      verifier = this._identityBox.tooltipText;
      supplemental = "";
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      host = this.getEffectiveHost();
      owner = iData.subjectOrg; 
      verifier = this._identityBox.tooltipText;

      // Build an appropriate supplemental block out of whatever location data we have
      if (iData.city)
        supplemental += iData.city + "\n";        
      if (iData.state && iData.country)
        supplemental += this._stringBundle.getFormattedString("identity.identified.state_and_country",
                                                              [iData.state, iData.country]);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
    }
    else {
      // These strings will be hidden in CSS anyhow
      host = "";
      owner = "";
    }
    
    // Push the appropriate strings out to the UI
    this._identityPopupContentHost.textContent = host;
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;
  },

  hideIdentityPopup : function() {
    this._identityPopup.hidePopup();
  },

  /**
   * Click handler for the identity-box element in primary chrome.  
   */
  handleIdentityButtonEvent : function(event) {
  
    event.stopPropagation();
 
    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN))
      return; // Left click, space or enter only

    // Revert the contents of the location bar, see bug 406779
    gURLBar.handleRevert();

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._identityPopup.hidden = false;
    
    // Tell the popup to consume dismiss clicks, to avoid bug 395314
    this._identityPopup.popupBoxObject
        .setConsumeRollupEvent(Ci.nsIPopupBoxObject.ROLLUP_CONSUME);
    
    // Update the popup strings
    this.setPopupMessages(this._identityBox.className);
    
    // Make sure the identity popup hangs toward the middle of the location bar
    // in RTL builds
    var position = 'after_start';
    if (gURLBar.getAttribute("chromedir") == "rtl")
      position = 'after_end';

    // Now open the popup, anchored off the primary chrome element
    this._identityPopup.openPopup(this._identityBox, position);
  }
};

let DownloadMonitorPanel = {
  //////////////////////////////////////////////////////////////////////////////
  //// DownloadMonitorPanel Member Variables

  _panel: null,
  _activeStr: null,
  _pausedStr: null,
  _lastTime: Infinity,
  _listening: false,

  //////////////////////////////////////////////////////////////////////////////
  //// DownloadMonitorPanel Public Methods

  /**
   * Initialize the status panel and member variables
   */
  init: function DMP_init() {
    // Load the modules to help display strings
    Cu.import("resource://gre/modules/DownloadUtils.jsm");
    Cu.import("resource://gre/modules/PluralForm.jsm");

    // Initialize "private" member variables
    this._panel = document.getElementById("download-monitor");

    // Cache the status strings
    let (bundle = document.getElementById("bundle_browser")) {
      this._activeStr = bundle.getString("activeDownloads");
      this._pausedStr = bundle.getString("pausedDownloads");
    }

    gDownloadMgr.addListener(this);
    this._listening = true;

    this.updateStatus();
  },

  uninit: function DMP_uninit() {
    if (this._listening)
      gDownloadMgr.removeListener(this);
  },

  /**
   * Update status based on the number of active and paused downloads
   */
  updateStatus: function DMP_updateStatus() {
    let numActive = gDownloadMgr.activeDownloadCount;

    // Hide the panel and reset the "last time" if there's no downloads
    if (numActive == 0) {
      this._panel.hidden = true;
      this._lastTime = Infinity;

      return;
    }
  
    // Find the download with the longest remaining time
    let numPaused = 0;
    let maxTime = -Infinity;
    let dls = gDownloadMgr.activeDownloads;
    while (dls.hasMoreElements()) {
      let dl = dls.getNext().QueryInterface(Ci.nsIDownload);
      if (dl.state == gDownloadMgr.DOWNLOAD_DOWNLOADING) {
        // Figure out if this download takes longer
        if (dl.speed > 0 && dl.size > 0)
          maxTime = Math.max(maxTime, (dl.size - dl.amountTransferred) / dl.speed);
        else
          maxTime = -1;
      }
      else if (dl.state == gDownloadMgr.DOWNLOAD_PAUSED)
        numPaused++;
    }

    // Get the remaining time string and last sec for time estimation
    let timeLeft;
    [timeLeft, this._lastTime] = DownloadUtils.getTimeLeft(maxTime, this._lastTime);

    // Figure out how many downloads are currently downloading
    let numDls = numActive - numPaused;
    let status = this._activeStr;

    // If all downloads are paused, show the paused message instead
    if (numDls == 0) {
      numDls = numPaused;
      status = this._pausedStr;
    }

    // Get the correct plural form and insert the number of downloads and time
    // left message if necessary
    status = PluralForm.get(numDls, status);
    status = status.replace("#1", numDls);
    status = status.replace("#2", timeLeft);

    // Update the panel and show it
    this._panel.label = status;
    this._panel.hidden = false;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener

  /**
   * Update status for download progress changes
   */
  onProgressChange: function() {
    this.updateStatus();
  },

  /**
   * Update status for download state changes
   */
  onDownloadStateChange: function() {
    this.updateStatus();
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus, aDownload) {
  },

  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) {
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadProgressListener]),
};

function getNotificationBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getNotificationBox(foundBrowser)
  return null;
};

/* DEPRECATED */
function getBrowser() gBrowser;
function getNavToolbox() gNavToolbox;

let gPrivateBrowsingUI = {
  _observerService: null,
  _privateBrowsingService: null,
  _privateBrowsingAutoStarted: false,

  init: function PBUI_init() {
    this._observerService = Cc["@mozilla.org/observer-service;1"].
                            getService(Ci.nsIObserverService);
    this._observerService.addObserver(this, "private-browsing", false);

    this._privateBrowsingService = Cc["@mozilla.org/privatebrowsing;1"].
                                   getService(Ci.nsIPrivateBrowsingService);

    if (this.privateBrowsingEnabled)
      this.onEnterPrivateBrowsing();
  },

  uninit: function PBUI_unint() {
    this._observerService.removeObserver(this, "private-browsing");
  },

  observe: function PBUI_observe(aSubject, aTopic, aData) {
    if (aTopic == "private-browsing") {
      if (aData == "enter")
        this.onEnterPrivateBrowsing();
      else if (aData == "exit")
        this.onExitPrivateBrowsing();
    }
  },

  _shouldEnter: function PBUI__shouldEnter() {
    try {
      // Never prompt if the session is not going to be closed, or if user has
      // already requested not to be prompted.
      if (gPrefService.getBoolPref("browser.privatebrowsing.dont_prompt_on_enter") ||
          gPrefService.getBoolPref("browser.privatebrowsing.keep_current_session"))
        return true;
    }
    catch (ex) { }

    var bundleService = Cc["@mozilla.org/intl/stringbundle;1"].
                        getService(Ci.nsIStringBundleService);
    var pbBundle = bundleService.createBundle("chrome://browser/locale/browser.properties");
    var brandBundle = bundleService.createBundle("chrome://branding/locale/brand.properties");

    var appName = brandBundle.GetStringFromName("brandShortName");
    var dialogTitle = pbBundle.GetStringFromName("privateBrowsingDialogTitle");
    var message = pbBundle.formatStringFromName("privateBrowsingMessage", [appName], 1);

    var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                        getService(Ci.nsIPromptService);

    var flags = promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0 +
                promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_1 +
                promptService.BUTTON_POS_0_DEFAULT;

    var neverAsk = {value:false};
    var button0Title = pbBundle.GetStringFromName("privateBrowsingYesTitle");
    var button1Title = pbBundle.GetStringFromName("privateBrowsingNoTitle");
    var neverAskText = pbBundle.GetStringFromName("privateBrowsingNeverAsk");

    var result;
    var choice = promptService.confirmEx(null, dialogTitle, message,
                               flags, button0Title, button1Title, null,
                               neverAskText, neverAsk);

    switch (choice) {
    case 0: // Start Private Browsing
      result = true;
      if (neverAsk.value)
        gPrefService.setBoolPref("browser.privatebrowsing.dont_prompt_on_enter", true);
      break;
    case 1: // Keep
      result = false;
      break;
    }

    return result;
  },

  onEnterPrivateBrowsing: function PBUI_onEnterPrivateBrowsing() {
    let pbMenuItem = document.getElementById("privateBrowsingItem");
    if (pbMenuItem)
      pbMenuItem.setAttribute("checked", "true");

    this._privateBrowsingAutoStarted = this._privateBrowsingService.autoStarted;

    if (!this._privateBrowsingAutoStarted) {
      // Adjust the window's title
      let docElement = document.documentElement;
#ifdef XP_MACOSX // see bug 411929 comment 38 for the reason behind this
      docElement.setAttribute("titlemodifier",
        docElement.getAttribute("titlemodifier_privatebrowsing"));
      docElement.setAttribute("titledefault", "");
#else
      docElement.setAttribute("title",
        docElement.getAttribute("title_privatebrowsing"));
      docElement.setAttribute("titlemodifier",
        docElement.getAttribute("titlemodifier_privatebrowsing"));
#endif
      docElement.setAttribute("browsingmode", "private");
    }
    else {
      // Disable the menu item in auto-start mode
      if (pbMenuItem)
        pbMenuItem.setAttribute("disabled", "true");
      document.getElementById("Tools:PrivateBrowsing")
              .setAttribute("disabled", "true");
    }
  },

  onExitPrivateBrowsing: function PBUI_onExitPrivateBrowsing() {
    if (BrowserSearch.searchBar)
      BrowserSearch.searchBar.textbox.reset();

    gFindBar.getElement("findbar-textbox").reset();

    let pbMenuItem = document.getElementById("privateBrowsingItem");
    if (pbMenuItem)
      pbMenuItem.removeAttribute("checked");

    if (!this._privateBrowsingAutoStarted) {
      // Adjust the window's title
      let docElement = document.documentElement;
#ifdef XP_MACOSX // see bug 411929 comment 38 for the reason behind this
      docElement.setAttribute("titlemodifier", "");
      docElement.setAttribute("titledefault",
        docElement.getAttribute("titlemodifier_normal"));
#else
      docElement.setAttribute("title",
        docElement.getAttribute("title_normal"));
      docElement.setAttribute("titlemodifier",
        docElement.getAttribute("titlemodifier_normal"));
#endif
      docElement.setAttribute("browsingmode", "normal");
    }
    else
      this._privateBrowsingAutoStarted = false;
  },

  toggleMode: function PBUI_toggleMode() {
    // prompt the users on entering the private mode, if needed
    if (!this.privateBrowsingEnabled)
      if (!this._shouldEnter())
        return;

    this._privateBrowsingService.privateBrowsingEnabled =
      !this.privateBrowsingEnabled;
  },

  get privateBrowsingEnabled PBUI_get_privateBrowsingEnabled() {
    return this._privateBrowsingService.privateBrowsingEnabled;
  }
};
