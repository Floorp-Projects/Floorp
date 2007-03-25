# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

const kXULNS =
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

#ifndef MOZ_PLACES
# For Places-enabled builds, this is in
# chrome://browser/content/places/controller.js
var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
#endif

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;

const MAX_HISTORY_MENU_ITEMS = 15;

// bookmark dialog features
#ifdef XP_MACOSX
const BROWSER_ADD_BM_FEATURES = "centerscreen,chrome,dialog,resizable,modal";
#else
const BROWSER_ADD_BM_FEATURES = "centerscreen,chrome,dialog,resizable,dependent";
#endif

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_XUL = "application/vnd.mozilla.xul+xml";

var gBrowserGlue = Components.classes["@mozilla.org/browser/browserglue;1"]
                             .getService(Components.interfaces.nsIBrowserGlue);
var gRDF = null;
var gGlobalHistory = null;
var gURIFixup = null;
var gPageStyleButton = null;
var gCharsetMenu = null;
var gLastBrowserCharset = null;
var gPrevCharset = null;
var gURLBar = null;
var gFindBar = null;
var gProxyButton = null;
var gProxyFavIcon = null;
var gProxyDeck = null;
var gNavigatorBundle = null;
var gIsLoadingBlank = false;
var gLastValidURLStr = "";
var gLastValidURL = null;
var gClickSelectsAll = false;
var gMustLoadSidebar = false;
var gProgressMeterPanel = null;
var gProgressCollapseTimer = null;
var gPrefService = null;
var appCore = null;
var gBrowser = null;
var gSidebarCommand = "";

// Global variable that holds the nsContextMenu instance.
var gContextMenu = null;

var gChromeState = null; // chrome state before we went into print preview

var gSanitizeListener = null;

var gURLBarAutoFillPrefListener = null;
var gAutoHideTabbarPrefListener = null;
var gBookmarkAllTabsHandler = null;

#ifdef XP_MACOSX
var gClickAndHoldTimer = null;
#endif

/**
* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/

function pageShowEventHandlers(event)
{
  // Filter out events that are not about the document load we are interested in
  if (event.originalTarget == content.document) {
    checkForDirectoryListing();
    charsetLoadListener(event);
    
    XULBrowserWindow.asyncUpdateUI();
  }

  // some event handlers want to be told what the original browser/listener is
  var targetBrowser = null;
  if (gBrowser.mTabbedMode) {
    var targetBrowserIndex = gBrowser.getBrowserIndexForDocument(event.originalTarget);
    if (targetBrowserIndex == -1)
      return;
    targetBrowser = gBrowser.getBrowserAtIndex(targetBrowserIndex);
  } else {
    targetBrowser = gBrowser.mCurrentBrowser;
  }

#ifndef MOZ_PLACES_BOOKMARKS
  // update the last visited date
  if (targetBrowser.currentURI.spec)
    BMSVC.updateLastVisitedDate(targetBrowser.currentURI.spec,
                                targetBrowser.contentDocument.characterSet);
#endif
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

function UpdateBackForwardCommands(aWebNavigation)
{
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

  var backButton = document.getElementById("back-button");
  if (backButton)
    _addClickAndHoldListenersOnElement(backButton);
  var forwardButton = document.getElementById("forward-button");
  if (forwardButton)
    _addClickAndHoldListenersOnElement(forwardButton);
}
#endif

function addBookmarkMenuitems()
{
  var tabbrowser = getBrowser();
  var tabMenu = document.getAnonymousElementByAttribute(tabbrowser,"anonid","tabContextMenu");
  var bookmarkAllTabsItem = document.createElement("menuitem");
  bookmarkAllTabsItem.setAttribute("label", gNavigatorBundle.getString("bookmarkAllTabs_label"));
  bookmarkAllTabsItem.setAttribute("accesskey", gNavigatorBundle.getString("bookmarkAllTabs_accesskey"));
  bookmarkAllTabsItem.setAttribute("command", "Browser:BookmarkAllTabs");
  var bookmarkCurTabItem = document.createElement("menuitem");
  bookmarkCurTabItem.setAttribute("label", gNavigatorBundle.getString("bookmarkCurTab_label"));
  bookmarkCurTabItem.setAttribute("accesskey", gNavigatorBundle.getString("bookmarkCurTab_accesskey"));
  bookmarkCurTabItem.setAttribute("oncommand", "BookmarkThisTab();");
  var menuseparator = document.createElement("menuseparator");
  var insertPos = tabMenu.lastChild.previousSibling;
  tabMenu.insertBefore(bookmarkAllTabsItem, insertPos);
  tabMenu.insertBefore(bookmarkCurTabItem, bookmarkAllTabsItem);
  tabMenu.insertBefore(menuseparator, bookmarkCurTabItem);
}

function BookmarkThisTab()
{
  var tab = getBrowser().mContextTab;
  if (tab.localName != "tab")
    tab = getBrowser().mCurrentTab;

#ifdef MOZ_PLACES_BOOKMARKS
  PlacesCommandHook.bookmarkPage(tab.linkedBrowser);
#else
  addBookmarkAs(tab.linkedBrowser, false);
#endif
}

#ifdef MOZ_PLACES_BOOKMARKS
/**
 * Initialize the bookmarks toolbar
 */
function initBookmarksToolbar() {
  var bt = document.getElementById("bookmarksBarContent");
  if (!bt)
    return;
  bt.place =
    PlacesUtils.getQueryStringForFolder(PlacesUtils.bookmarks.toolbarFolder);
}
#endif

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

  onUpdatePageReport: function ()
  {
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
      if (!gPrefService)
        gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefBranch2);
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
    var tabbrowser = getBrowser();
    for (var i = 0; i < tabbrowser.browsers.length; ++i) {
      var browser = tabbrowser.getBrowserAtIndex(i);
      if (this._findChildShell(browser.docShell, aDocShell))
        return browser;
    }
    return null;
  },

  observe: function (aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    var browserBundle = document.getElementById("bundle_browser");
    var browser, webNav, wm;
    switch (aTopic) {
    case "xpinstall-install-blocked":
      var shell = aSubject.QueryInterface(Components.interfaces.nsIDocShell);
      browser = this._getBrowser(shell);
      if (browser) {
        var host = browser.docShell.QueryInterface(Components.interfaces.nsIWebNavigation).currentURI.host;
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
          // XXXben - use regular software install warnings for now until we can
          // properly differentiate themes. It's likely in fact that themes won't
          // be blocked so this code path will only be reached for extensions.
          notificationName = "xpinstall"
          messageString = browserBundle.getFormattedString("xpinstallPromptWarning",
                                                           [brandShortName, host]);

          buttons = [{
            label: browserBundle.getString("xpinstallPromptWarningButton"),
            accessKey: browserBundle.getString("xpinstallPromptWarningButton.accesskey"),
            popup: null,
            callback: function() { return xpinstallEditPermissions(shell); }
          }];
        }

        var notificationBox = gBrowser.getNotificationBox(browser);
        if (!notificationBox.getNotificationWithValue(notificationName)) {
          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          const iconURL = "chrome://mozapps/skin/xpinstall/xpinstallItemGeneric.png";
          notificationBox.appendNotification(messageString, notificationName,
                                             iconURL, priority, buttons);
        }
      }
      break;
    }
  }
};

function xpinstallEditPermissions(aDocShell)
{
  var browser = gXPInstallObserver._getBrowser(aDocShell);
  if (browser) {
    var bundlePreferences = document.getElementById("bundle_preferences");
    var webNav = aDocShell.QueryInterface(Components.interfaces.nsIWebNavigation);
    var params = { blockVisible   : false,
                   sessionVisible : false,
                   allowVisible   : true,
                   prefilledHost  : webNav.currentURI.host,
                   permissionType : "install",
                   windowTitle    : bundlePreferences.getString("addons_permissions_title"),
                   introText      : bundlePreferences.getString("addonspermissionstext") };
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
    return false;
  }

  return true;
}

function BrowserStartup()
{
  gBrowser = document.getElementById("content");

  window.tryToClose = WindowIsClosing;

  var uriToLoad = null;
  // Check for window.arguments[0]. If present, use that for uriToLoad.
  if ("arguments" in window && window.arguments[0])
    uriToLoad = window.arguments[0];

  gIsLoadingBlank = uriToLoad == "about:blank";

  prepareForStartup();

#ifdef ENABLE_PAGE_CYCLER
  appCore.startPageCycler();
#else
# only load url passed in when we're not page cycling
  if (uriToLoad && !gIsLoadingBlank) {
    if (window.arguments.length >= 3)
      loadURI(uriToLoad, window.arguments[2], window.arguments[3] || null,
              window.arguments[4] || false);
    else
      loadOneOrMoreURIs(uriToLoad);
  }
#endif

  var sidebarSplitter;
  if (window.opener && !window.opener.closed) {
    var openerFindBar = window.opener.gFindBar;
    if (openerFindBar && !openerFindBar.hidden &&
        openerFindBar.findMode == gFindBar.FIND_NORMAL)
      gFindBar.open();

    var openerSidebarBox = window.opener.document.getElementById("sidebar-box");
    // The opener can be the hidden window too, if we're coming from the state
    // where no windows are open, and the hidden window has no sidebar box.
    if (openerSidebarBox && !openerSidebarBox.hidden) {
      var sidebarBox = document.getElementById("sidebar-box");
      var sidebarTitle = document.getElementById("sidebar-title");
      sidebarTitle.setAttribute("value", window.opener.document.getElementById("sidebar-title").getAttribute("value"));
      sidebarBox.setAttribute("width", openerSidebarBox.boxObject.width);
      var sidebarCmd = openerSidebarBox.getAttribute("sidebarcommand");
      sidebarBox.setAttribute("sidebarcommand", sidebarCmd);
      sidebarBox.setAttribute("src", window.opener.document.getElementById("sidebar").getAttribute("src"));
      gMustLoadSidebar = true;
      sidebarBox.hidden = false;
      sidebarSplitter = document.getElementById("sidebar-splitter");
      sidebarSplitter.hidden = false;
      document.getElementById(sidebarCmd).setAttribute("checked", "true");
    }
  }
  else {
    var box = document.getElementById("sidebar-box");
    if (box.hasAttribute("sidebarcommand")) {
      var commandID = box.getAttribute("sidebarcommand");
      if (commandID) {
        var command = document.getElementById(commandID);
        if (command) {
          gMustLoadSidebar = true;
          box.hidden = false;
          sidebarSplitter = document.getElementById("sidebar-splitter");
          sidebarSplitter.hidden = false;
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
  var obs = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  obs.notifyObservers(null, "browser-window-before-show", "");

  // Set a sane starting width/height for all resolutions on new profiles.
  if (!document.documentElement.hasAttribute("width")) {
    var defaultWidth = 994, defaultHeight;
    if (screen.availHeight <= 600) {
      document.documentElement.setAttribute("sizemode", "maximized");
      defaultWidth = 610;
      defaultHeight = 450;
    }
    else {
      // Create a narrower window for large or wide-aspect displays, to suggest
      // side-by-side page view.
      if ((screen.availWidth / 2) >= 800)
        defaultWidth = (screen.availWidth / 2) - 20;
      defaultHeight = screen.availHeight - 10;
#ifdef MOZ_WIDGET_GTK
#define USE_HEIGHT_ADJUST
#endif
#ifdef MOZ_WIDGET_GTK2
#define USE_HEIGHT_ADJUST
#endif
#ifdef USE_HEIGHT_ADJUST
      // On X, we're not currently able to account for the size of the window
      // border.  Use 28px as a guess (titlebar + bottom window border)
      defaultHeight -= 28;
#endif
#ifdef XP_MACOSX
      // account for the Mac OS X title bar
      defaultHeight -= 22;
#endif
    }
    document.documentElement.setAttribute("width", defaultWidth);
    document.documentElement.setAttribute("height", defaultHeight);
  }

  setTimeout(delayedStartup, 0);
}

function HandleAppCommandEvent(evt)
{
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

function prepareForStartup()
{
  gURLBar = document.getElementById("urlbar");
  gNavigatorBundle = document.getElementById("bundle_browser");
  gProgressMeterPanel = document.getElementById("statusbar-progresspanel");
  gFindBar = document.getElementById("FindToolbar");
  gBrowser.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);
  // Note: we need to listen to untrusted events, because the pluginfinder XBL
  // binding can't fire trusted ones (runs with page privileges).
  gBrowser.addEventListener("PluginNotFound", gMissingPluginInstaller.newMissingPlugin, true, true);
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
  window.XULBrowserWindow = new nsBrowserStatusHandler();
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

  // Initialize the feedhandler
  FeedHandler.init();

  // Initialize the searchbar
  BrowserSearch.init();
}

function delayedStartup()
{
  var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  os.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
  os.addObserver(gXPInstallObserver, "xpinstall-install-blocked", false);

  if (!gPrefService)
    gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch2);
  BrowserOffline.init();
  
  if (gURLBar && document.documentElement.getAttribute("chromehidden").indexOf("toolbar") != -1) {
    gURLBar.setAttribute("readonly", "true");
    gURLBar.setAttribute("enablehistory", "false");
    var goButtonStack = document.getElementById("go-button-stack");
    if (goButtonStack)
      goButtonStack.setAttribute("hidden", "true");
  }

  if (gURLBar)
    gURLBar.addEventListener("dragdrop", URLBarOnDrop, true);

  gBrowser.addEventListener("pageshow", function(evt) { setTimeout(pageShowEventHandlers, 0, evt); }, true);

  window.addEventListener("keypress", ctrlNumberTabSelection, false);

  if (gMustLoadSidebar) {
    var sidebar = document.getElementById("sidebar");
    var sidebarBox = document.getElementById("sidebar-box");
    sidebar.setAttribute("src", sidebarBox.getAttribute("src"));
  }

  // add bookmark options to context menu for tabs
  addBookmarkMenuitems();

#ifndef MOZ_PLACES_BOOKMARKS
  initServices();
  initBMService();
  // now load bookmarks
  BMSVC.readBookmarks();
  var bt = document.getElementById("bookmarks-ptf");
  if (bt) {
    var btf = BMSVC.getBookmarksToolbarFolder().Value;
    bt.ref = btf;
    document.getElementById("bookmarks-chevron").ref = btf;
    bt.database.AddObserver(BookmarksToolbarRDFObserver);
  }
  window.addEventListener("resize", BookmarksToolbar.resizeFunc, false);
  document.getElementById("PersonalToolbar")
          .controllers.appendController(BookmarksMenuController);
#else
  PlacesMenuDNDController.init();

  initBookmarksToolbar();
#endif

  // called when we go into full screen, even if it is
  // initiated by a web page script
  window.addEventListener("fullscreen", onFullScreen, true);

  if (gIsLoadingBlank && gURLBar && isElementVisible(gURLBar))
    focusElement(gURLBar);
  else
    focusElement(content);

  SetPageProxyState("invalid");

  var toolbox = document.getElementById("navigator-toolbox");
  toolbox.customizeDone = BrowserToolboxCustomizeDone;

  // Set up Sanitize Item
  gSanitizeListener = new SanitizeListener();

  // Enable/Disable URL Bar Auto Fill
  gURLBarAutoFillPrefListener = new URLBarAutoFillPrefListener();
  gPrefService.addObserver(gURLBarAutoFillPrefListener.domain,
                           gURLBarAutoFillPrefListener, false);

  // Enable/Disable auto-hide tabbar
  gAutoHideTabbarPrefListener = new AutoHideTabbarPrefListener();
  gPrefService.addObserver(gAutoHideTabbarPrefListener.domain,
                           gAutoHideTabbarPrefListener, false);

  gPrefService.addObserver(gHomeButton.prefDomain, gHomeButton, false);
  gHomeButton.updateTooltip();

  gClickSelectsAll = gPrefService.getBoolPref("browser.urlbar.clickSelectsAll");
  if (gURLBar)
    gURLBar.clickSelectsAll = gClickSelectsAll;

#ifdef HAVE_SHELL_SERVICE
  // Perform default browser checking (after window opens).
  var shell = getShellService();
  if (shell) {
    var shouldCheck = shell.shouldCheckDefaultBrowser;
    var willRestoreSession = false;
    try {
      var ss = Cc["@mozilla.org/browser/sessionstartup;1"].
               getService(Ci.nsISessionStartup);
      willRestoreSession = ss.doRestore();
    }
    catch (ex) { /* never mind; suppose SessionStore is broken */ }
    if (shouldCheck && !shell.isDefaultBrowser(true) && !willRestoreSession) {
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

  // Initialize the microsummary service by retrieving it, prompting its factory
  // to create its singleton, whose constructor initializes the service.
  Cc["@mozilla.org/microsummary/service;1"].getService(Ci.nsIMicrosummaryService);

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

  // browser-specific tab augmentation
  AugmentTabs.init();

  // bookmark-all-tabs command
  gBookmarkAllTabsHandler = new BookmarkAllTabsHandler();
}

function BrowserShutdown()
{
  var os = Components.classes["@mozilla.org/observer-service;1"]
    .getService(Components.interfaces.nsIObserverService);
  os.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
  os.removeObserver(gXPInstallObserver, "xpinstall-install-blocked");

  try {
    gBrowser.removeProgressListener(window.XULBrowserWindow);
  } catch (ex) {
  }

#ifndef MOZ_PLACES_BOOKMARKS
  try {
    document.getElementById("PersonalToolbar")
            .controllers.removeController(BookmarksMenuController);
  } catch (ex) {
  }

  var bt = document.getElementById("bookmarks-ptf");
  if (bt) {
    try {
      bt.database.RemoveObserver(BookmarksToolbarRDFObserver);
    } catch (ex) {
    }
  }
#endif

  try {
    gPrefService.removeObserver(gURLBarAutoFillPrefListener.domain,
                                gURLBarAutoFillPrefListener);
    gPrefService.removeObserver(gAutoHideTabbarPrefListener.domain,
                                gAutoHideTabbarPrefListener);
    gPrefService.removeObserver(gHomeButton.prefDomain, gHomeButton);
  } catch (ex) {
    Components.utils.reportError(ex);
  }

  if (gSanitizeListener)
    gSanitizeListener.shutdown();

  BrowserOffline.uninit();

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
// nonBrowserWindowStartup() and nonBrowserWindowDelayedStartup() are used for
// non-browser windows in macBrowserOverlay
function nonBrowserWindowStartup()
{
  // Disable inappropriate commands / submenus
  var disabledItems = ['cmd_newNavigatorTab', 'Browser:SavePage', 'Browser:SendLink',
                       'cmd_pageSetup', 'cmd_print', 'cmd_find', 'cmd_findAgain', 'viewToolbarsMenu',
                       'cmd_toggleTaskbar', 'viewSidebarMenuMenu', 'Browser:Reload', 'Browser:ReloadSkipCache',
                       'viewTextZoomMenu', 'pageStyleMenu', 'charsetMenu', 'View:PageSource', 'View:FullScreen',
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

  gNavigatorBundle = document.getElementById("bundle_browser");

  setTimeout(nonBrowserWindowDelayedStartup, 0);
}

function nonBrowserWindowDelayedStartup()
{
  // loads the services
#ifndef MOZ_PLACES_BOOKMARKS
  initServices();
  initBMService();
#endif

  // init global pref service
  gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch2);

  // Set up Sanitize Item
  gSanitizeListener = new SanitizeListener();
}
#endif

function URLBarAutoFillPrefListener()
{
  this.toggleAutoFillInURLBar();
}

URLBarAutoFillPrefListener.prototype =
{
  domain: "browser.urlbar.autoFill",
  observe: function (aSubject, aTopic, aPrefName)
  {
    if (aTopic != "nsPref:changed" || aPrefName != this.domain)
      return;

    this.toggleAutoFillInURLBar();
  },

  toggleAutoFillInURLBar: function ()
  {
    if (!gURLBar)
      return;

    var prefValue = false;
    try {
      prefValue = gPrefService.getBoolPref(this.domain);
    }
    catch (e) {
    }

    if (prefValue)
      gURLBar.setAttribute("completedefaultindex", "true");
    else
      gURLBar.removeAttribute("completedefaultindex");
  }
}

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
      gPrefService.setBoolPref("browser.tabs.forceHide", false);
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
    gPrefService.savePrefFile(null);
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
        gNavigatorBundle.getString("sanitizeWithPromptLabel") : 
        this._defaultLabel;
    document.getElementById("sanitizeItem").setAttribute("label", label);
  }
}

function ctrlNumberTabSelection(event)
{
  if (event.altKey && event.keyCode == KeyEvent.DOM_VK_RETURN) {
    // XXXblake Proper fix is to just check whether focus is in the urlbar. However, focus with the autocomplete widget is all
    // hacky and broken and there's no way to do that right now. So this just patches it to ensure that alt+enter works when focus
    // is on a link.
    if (!(document.commandDispatcher.focusedElement instanceof HTMLAnchorElement)) {
      // Don't let winxp beep on ALT+ENTER, since the URL bar uses it.
      event.preventDefault();
      return;
    }
  }

#ifdef XP_MACOSX
  if (!event.metaKey)
#else
#ifdef XP_UNIX
  // don't let tab selection clash with numeric accesskeys (bug 366084)
  if (!event.altKey || event.shiftKey)
#else
  if (!event.ctrlKey)
#endif
#endif
    return;

  // \d in a RegExp will find any Unicode character with the "decimal digit"
  // property (Nd)
  var regExp = /\d/;
  if (!regExp.test(String.fromCharCode(event.charCode)))
    return;

  // Some Unicode decimal digits are in the range U+xxx0 - U+xxx9 and some are
  // in the range U+xxx6 - U+xxxF. Find the digit 1 corresponding to our
  // character.
  var digit1 = (event.charCode & 0xFFF0) | 1;
  if (!regExp.test(String.fromCharCode(digit1)))
    digit1 += 6;

  var index = event.charCode - digit1;
  if (index < 0)
    return;

  // [Ctrl]+[9] always selects the last tab
  if (index == 8)
    index = gBrowser.tabContainer.childNodes.length - 1;
  else if (index >= gBrowser.tabContainer.childNodes.length)
    return;

  var oldTab = gBrowser.selectedTab;
  var newTab = gBrowser.tabContainer.childNodes[index];
  if (newTab != oldTab)
    gBrowser.selectedTab = newTab;

  event.preventDefault();
  event.stopPropagation();
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
      getWebNavigation().gotoIndex(index);
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

function BrowserForward(aEvent, aIgnoreAlt)
{
  var where = whereToOpenLink(aEvent, false, aIgnoreAlt);

  if (where == "current") {
    try {
      getWebNavigation().goForward();
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

function BrowserBack(aEvent, aIgnoreAlt)
{
  var where = whereToOpenLink(aEvent, false, aIgnoreAlt);

  if (where == "current") {
    try {
      getWebNavigation().goBack();
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

function BrowserBackMenu(event)
{
  return FillHistoryMenu(event.target, "back");
}

function BrowserForwardMenu(event)
{
  return FillHistoryMenu(event.target, "forward");
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

function BrowserReload()
{
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_NONE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadSkipCache()
{
  // Bypass proxy and cache.
  const reloadFlags = nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserHome()
{
  var homePage = gHomeButton.getHomePage();
  loadOneOrMoreURIs(homePage);
}

function BrowserHomeClick(aEvent)
{
  if (aEvent.button == 2) // right-click: do nothing
    return;

  var homePage = gHomeButton.getHomePage();
  var where = whereToOpenLink(aEvent);
  var urls;

  // openUILinkIn in utilityOverlay.js doesn't handle loading multiple pages
  switch (where) {
  case "save":
    urls = homePage.split("|");
    saveURL(urls[0], null, null, true);  // only save the first page
    break;
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
    newWindow = openDialog(getBrowserURL(), "_blank", "all,dialog=no", aURIString);
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

#ifndef MOZ_PLACES
function constructGoMenuItem(goMenu, beforeItem, url, title)
{
  var menuitem = document.createElementNS(kXULNS, "menuitem");
  menuitem.setAttribute("statustext", url);
  menuitem.setAttribute("label", title);
  goMenu.insertBefore(menuitem, beforeItem);
  return menuitem;
}

function onGoMenuHidden(aEvent)
{
  if (aEvent.target == aEvent.currentTarget)
    setTimeout(destroyGoMenuItems, 0, document.getElementById('goPopup'));
}

function destroyGoMenuItems(goMenu) {
  var startSeparator = document.getElementById("startHistorySeparator");
  var endSeparator = document.getElementById("endHistorySeparator");
  endSeparator.hidden = true;

  // Destroy the items.
  var destroy = false;
  for (var i = 0; i < goMenu.childNodes.length; i++) {
    var item = goMenu.childNodes[i];
    if (item == endSeparator)
      break;

    if (destroy) {
      i--;
      goMenu.removeChild(item);
    }

    if (item == startSeparator)
      destroy = true;
  }
}

function updateGoMenu(aEvent, goMenu)
{
  if (aEvent.target != aEvent.currentTarget)
    return;

  // In case the timer didn't fire.
  destroyGoMenuItems(goMenu);

  // enable/disable RCT sub menu
  // do this here, before the early return
  HistoryMenu.toggleRecentlyClosedTabs();

  var history = document.getElementById("hiddenHistoryTree");

  if (history.hidden) {
    history.hidden = false;
    var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                                  .getService(Components.interfaces.nsIRDFDataSource);
    history.database.AddDataSource(globalHistory);
  }

  if (!history.ref)
    history.ref = "NC:HistoryRoot";

  var count = history.view.rowCount;
  if (count > 10)
    count = 10;

  if (count == 0)
    return;

  const NC_NS     = "http://home.netscape.com/NC-rdf#";

  if (!gRDF)
     gRDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                      .getService(Components.interfaces.nsIRDFService);

  var builder = history.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);

  var beforeItem = document.getElementById("endHistorySeparator");

  var nameResource = gRDF.GetResource(NC_NS + "Name");

  var endSep = beforeItem;
  var showSep = false;

  for (var i = count-1; i >= 0; i--) {
    var res = builder.getResourceAtIndex(i);
    var url = res.Value;
    var titleRes = history.database.GetTarget(res, nameResource, true);
    if (!titleRes)
      continue;

    showSep = true;
    var titleLiteral = titleRes.QueryInterface(Components.interfaces.nsIRDFLiteral);
    beforeItem = constructGoMenuItem(goMenu, beforeItem, url, titleLiteral.Value);
  }

  if (showSep)
    endSep.hidden = false;
}
#endif
 
#ifndef MOZ_PLACES_BOOKMARKS
function addBookmarkAs(aBrowser, aBookmarkAllTabs, aIsWebPanel)
{
  const browsers = aBrowser.browsers;

  // we only disable the menu item on onpopupshowing; if we get
  // here via keyboard shortcut, we need to pretend like
  // nothing happened if we have no tabs
  if ((!browsers || browsers.length == 1) && aBookmarkAllTabs)
    return;

  if (browsers && browsers.length > 1)
    addBookmarkForTabBrowser(aBrowser, aBookmarkAllTabs);
  else
    addBookmarkForBrowser(aBrowser.webNavigation, aIsWebPanel);
}

function addBookmarkForTabBrowser(aTabBrowser, aBookmarkAllTabs, aSelect)
{
  var tabsInfo = [];
  var currentTabInfo = { name: "", url: "", charset: null };

  const activeBrowser = aTabBrowser.selectedBrowser;
  const browsers = aTabBrowser.browsers;
  for (var i = 0; i < browsers.length; ++i) {
    var webNav = browsers[i].webNavigation;
    var url = webNav.currentURI.spec;
    var name = "";
    var charSet, description;
    try {
      var doc = webNav.document;
      name = doc.title || url;
      charSet = doc.characterSet;
      description = BookmarksUtils.getDescriptionFromDocument(doc);
    } catch (e) {
      name = url;
    }
    tabsInfo[i] = { name: name, url: url, charset: charSet, description: description };
    if (browsers[i] == activeBrowser)
      currentTabInfo = tabsInfo[i];
  }
  var dialogArgs = currentTabInfo;
  if (aBookmarkAllTabs) {
    dialogArgs = { name: gNavigatorBundle.getString("bookmarkAllTabsDefault") };
    dialogArgs.bBookmarkAllTabs = true;
  }

  dialogArgs.objGroup = tabsInfo;
  openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
             BROWSER_ADD_BM_FEATURES, dialogArgs);
}

function addBookmarkForBrowser(aDocShell, aIsWebPanel)
{
  // Bug 52536: We obtain the URL and title from the nsIWebNavigation
  // associated with a <browser/> rather than from a DOMWindow.
  // This is because when a full page plugin is loaded, there is
  // no DOMWindow (?) but information about the loaded document
  // may still be obtained from the webNavigation.
  var url = aDocShell.currentURI.spec;
  var title, charSet = null;
  var description;
  try {
    title = aDocShell.document.title || url;
    charSet = aDocShell.document.characterSet;
    description = BookmarksUtils.getDescriptionFromDocument(aDocShell.document);
  }
  catch (e) {
    title = url;
  }
  BookmarksUtils.addBookmark(url, title, charSet, aIsWebPanel, description);
}
#endif

function openLocation()
{
  if (gURLBar && isElementVisible(gURLBar)) {
    gURLBar.focus();
    gURLBar.select();
    return;
  }
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
  gBrowser.loadOneTab("about:blank", null, null, null, false, false);
  if (gURLBar)
    setTimeout(function() { gURLBar.focus(); }, 0);
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

function BrowserCloseTabOrWindow()
{
#ifdef XP_MACOSX
  // If we're not a browser window, just close the window
  if (window.location.href != getBrowserURL()) {
    closeWindow(true);
    return;
  }
#endif

  if (gBrowser.tabContainer.childNodes.length > 1) {
    // Just close up a tab.
    gBrowser.removeCurrentTab();
    return;
  }
#ifndef XP_MACOSX
  if (window.toolbar.visible &&
      !gPrefService.getBoolPref("browser.tabs.autoHide")) {
    // Replace the remaining tab with a blank one and focus the address bar
    gBrowser.removeCurrentTab();
    if (gURLBar)
      setTimeout(function() { gURLBar.focus(); }, 0);
    return;
  }
#endif

  BrowserCloseWindow();
}

function BrowserTryToCloseWindow()
{
  //give tryToClose a chance to veto if it is defined
  if (typeof(window.tryToClose) != "function" || window.tryToClose())
    BrowserCloseWindow();
}

function BrowserCloseWindow()
{
  // This code replicates stuff in BrowserShutdown().  It is here because
  // window.screenX and window.screenY have real values.  We need
  // to fix this eventually but by replicating the code here, we
  // provide a means of saving position (it just requires that the
  // user close the window via File->Close (vs. close box).

  // Get the current window position/size.
  var x = window.screenX;
  var y = window.screenY;
  var h = window.outerHeight;
  var w = window.outerWidth;

  // Store these into the window attributes (for persistence).
  var win = document.getElementById( "main-window" );
  win.setAttribute( "x", x );
  win.setAttribute( "y", y );
  win.setAttribute( "height", h );
  win.setAttribute( "width", w );

  closeWindow(true);
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
    getWebNavigation().loadURI(uri, flags, referrer, postData, null);
  } catch (e) {
  }
}

function BrowserLoadURL(aTriggeringEvent, aPostData) {
  var url = gURLBar.value;

  if (aTriggeringEvent instanceof MouseEvent) {
    if (aTriggeringEvent.button == 2)
      return; // Do nothing for right clicks

    // We have a mouse event (from the go button), so use the standard
    // UI link behaviors
    openUILink(url, aTriggeringEvent, false, false,
               true /* allow third party fixup */, aPostData);
    return;
  }

  if (aTriggeringEvent && aTriggeringEvent.altKey) {
    handleURLBarRevert();
    content.focus();
    gBrowser.loadOneTab(url, null, null, aPostData, false,
                        true /* allow third party fixup */);
    aTriggeringEvent.preventDefault();
    aTriggeringEvent.stopPropagation();
  }
  else
    loadURI(url, null, aPostData, true /* allow third party fixup */);

  focusElement(content);
}

function getShortcutOrURI(aURL, aPostDataRef)
{
  // rjc: added support for URL shortcuts (3/30/1999)
  try {
    var shortcutURL = null;
#ifdef MOZ_PLACES_BOOKMARKS
    var shortcutURI = PlacesUtils.bookmarks.getURIForKeyword(aURL);
    if (shortcutURI)
      shortcutURL = shortcutURI.spec;
#else
    shortcutURL = BMSVC.resolveKeyword(aURL, aPostDataRef);
#endif
    if (!shortcutURL) {
      // rjc: add support for string substitution with shortcuts (4/4/2000)
      //      (see bug # 29871 for details)
      var aOffset = aURL.indexOf(" ");
      if (aOffset > 0) {
        var cmd = aURL.substr(0, aOffset);
        var text = aURL.substr(aOffset+1);
#ifdef MOZ_PLACES_BOOKMARKS
        shortcutURI = bookmarkService.getURIForKeyword(cmd);
        if (shortcutURI)
          shortcutURL = shortcutURI.spec;
#else
        shortcutURL = BMSVC.resolveKeyword(cmd, aPostDataRef);
#endif
        if (shortcutURL && text) {
          var encodedText = null; 
          var charset = "";
          const re = /^(.*)\&mozcharset=([a-zA-Z][_\-a-zA-Z0-9]+)\s*$/; 
          var matches = shortcutURL.match(re);
          if (matches) {
             shortcutURL = matches[1];
             charset = matches[2];
          }
#ifndef MOZ_PLACES_BOOKMARKS
          // FIXME: Bug #317472, we don't have last charset in places yet.
          else if (/%s/.test(shortcutURL) || 
                   (aPostDataRef && /%s/.test(aPostDataRef.value))) {
            try {
              charset = BMSVC.getLastCharset(shortcutURL);
            } catch (ex) {
            }
          }
#endif

          if (charset)
            encodedText = escape(convertFromUnicode(charset, text)); 
          else  // default case: charset=UTF-8
            encodedText = encodeURIComponent(text);

          if (aPostDataRef && aPostDataRef.value) {
            // XXXben - currently we only support "application/x-www-form-urlencoded"
            //          enctypes.
            aPostDataRef.value = unescape(aPostDataRef.value);
            if (aPostDataRef.value.match(/%[sS]/)) {
              aPostDataRef.value = getPostDataStream(aPostDataRef.value,
                                                     text, encodedText,
                                                     "application/x-www-form-urlencoded");
            }
            else {
              shortcutURL = null;
              aPostDataRef.value = null;
            }
          }
          else {
            if (/%[sS]/.test(shortcutURL))
              shortcutURL = shortcutURL.replace(/%s/g, encodedText)
                                       .replace(/%S/g, text);
            else 
              shortcutURL = null;
          }
        }
      }
    }

    if (shortcutURL)
      aURL = shortcutURL;

  } catch (ex) {
  }
  return aURL;
}

#if 0
// XXXben - this is only useful if we ever support text/plain encoded forms in
// smart keywords.
function normalizePostData(aStringData)
{
  var parts = aStringData.split("&");
  var result = "";
  for (var i = 0; i < parts.length; ++i) {
    var part = unescape(parts[i]);
    if (part)
      result += part + "\r\n";
  }
  return result;
}
#endif
function getPostDataStream(aStringData, aKeyword, aEncKeyword, aType)
{
  var dataStream = Components.classes["@mozilla.org/io/string-input-stream;1"]
                            .createInstance(Components.interfaces.nsIStringInputStream);
  aStringData = aStringData.replace(/%s/g, aEncKeyword).replace(/%S/g, aKeyword);
#ifdef MOZILLA_1_8_BRANCH
# bug 318193
  dataStream.setData(aStringData, aStringData.length);
#else
  dataStream.data = aStringData;
#endif

  var mimeStream = Components.classes["@mozilla.org/network/mime-input-stream;1"]
                              .createInstance(Components.interfaces.nsIMIMEInputStream);
  mimeStream.addHeader("Content-Type", aType);
  mimeStream.addContentLength = true;
  mimeStream.setData(dataStream);
  return mimeStream.QueryInterface(Components.interfaces.nsIInputStream);
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

  ViewSourceOfURL(webNav.currentURI.spec, pageCookie, aDocument);
}

function ViewSourceOfURL(aURL, aPageDescriptor, aDocument)
{
  if (getBoolPref("view_source.editor.external", false)) {
    gViewSourceUtils.openInExternalEditor(aURL, aPageDescriptor, aDocument);
  }
  else {
    gViewSourceUtils.openInInternalViewer(aURL, aPageDescriptor, aDocument);
  }
}

// doc - document to use for source, or null for this window's document
// initialTab - name of the initial tab to display, or null for the first tab
function BrowserPageInfo(doc, initialTab)
{
  var args = {doc: doc, initialTab: initialTab};
  toOpenDialogByTypeAndUrl("Browser:page-info",
                           doc ? doc.location : window.content.document.location,
                           "chrome://browser/content/pageInfo.xul",
                           "chrome,dialog=no",
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

function checkForDirectoryListing()
{
  if ( "HTTPIndex" in content &&
       content.HTTPIndex instanceof Components.interfaces.nsIHTTPIndex ) {
    content.wrappedJSObject.defaultCharacterset =
      getMarkupDocumentViewer().defaultCharacterSet;
  }
}

// If "ESC" is pressed in the url bar, we replace the urlbar's value with the url of the page
// and highlight it, unless it is about:blank, where we reset it to "".
function handleURLBarRevert()
{
  var url = getWebNavigation().currentURI.spec;
  var throbberElement = document.getElementById("navigator-throbber");

  var isScrolling = gURLBar.popupOpen;

  // don't revert to last valid url unless page is NOT loading
  // and user is NOT key-scrolling through autocomplete list
  if ((!throbberElement || !throbberElement.hasAttribute("busy")) && !isScrolling) {
    if (url != "about:blank") {
      gURLBar.value = url;
      gURLBar.select();
      SetPageProxyState("valid");
    } else { //if about:blank, urlbar becomes ""
      gURLBar.value = "";
    }
  }

  gBrowser.userTypedValue = null;

  // tell widget to revert to last typed text only if the user
  // was scrolling when they hit escape
  return !isScrolling;
}

function handleURLBarCommand(aTriggeringEvent) {
  if (!gURLBar.value)
    return;

  var postData = { };
  canonizeUrl(aTriggeringEvent, postData);

  try {
    addToUrlbarHistory(gURLBar.value);
  } catch (ex) {
    // Things may go wrong when adding url to session history,
    // but don't let that interfere with the loading of the url.
  }

  BrowserLoadURL(aTriggeringEvent, postData.value);
}

function canonizeUrl(aTriggeringEvent, aPostDataRef) {
  if (!gURLBar || !gURLBar.value)
    return;

  var url = gURLBar.value;

  // Only add the suffix when the URL bar value isn't already "URL-like".
  // Since this function is called from handleURLBarCommand, which receives
  // both mouse (from the go button) and keyboard events, we also make sure not
  // to do the fixup unless we get a keyboard event, to match user expectations.
  if (!/^(www|http)|\/\s*$/i.test(url) &&
      (aTriggeringEvent instanceof KeyEvent)) {
#ifdef XP_MACOSX
    var accel = aTriggeringEvent.metaKey;
#else
    var accel = aTriggeringEvent.ctrlKey;
#endif
    var shift = aTriggeringEvent.shiftKey;

    var suffix = "";

    switch (true) {
      case (accel && shift):
        suffix = ".org/";
        break;
      case (shift):
        suffix = ".net/";
        break;
      case (accel):
        try {
          suffix = gPrefService.getCharPref("browser.fixup.alternate.suffix");
          if (suffix.charAt(suffix.length - 1) != "/")
            suffix += "/";
        } catch(e) {
          suffix = ".com/";
        }
        break;
    }

    if (suffix) {
      // trim leading/trailing spaces (bug 233205)
      url = url.replace(/^\s+/, "").replace(/\s+$/, "");

      // Tack www. and suffix on.  If user has appended directories, insert
      // suffix before them (bug 279035).  Be careful not to get two slashes.
      // Also, don't add the suffix if it's in the original url (bug 233853).
      
      var firstSlash = url.indexOf("/");
      var existingSuffix = url.indexOf(suffix.substring(0, suffix.length - 1));

      // * Logic for slash and existing suffix (example)
      // No slash, no suffix: Add suffix (mozilla)
      // No slash, yes suffix: Add slash (mozilla.com)
      // Yes slash, no suffix: Insert suffix (mozilla/stuff)
      // Yes slash, suffix before slash: Do nothing (mozilla.com/stuff)
      // Yes slash, suffix after slash: Insert suffix (mozilla/?stuff=.com)
      
      if (firstSlash >= 0) {
        if (existingSuffix == -1 || existingSuffix > firstSlash)
          url = url.substring(0, firstSlash) + suffix +
                url.substring(firstSlash + 1);
      } else
        url = url + (existingSuffix == -1 ? suffix : "/");

      url = "http://www." + url;
    }
  }

  gURLBar.value = getShortcutOrURI(url, aPostDataRef);

  // Also update this so the browser display keeps the new value (bug 310651)
  gBrowser.userTypedValue = gURLBar.value;
}

function UpdatePageProxyState()
{
  if (gURLBar && gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid");
}

function SetPageProxyState(aState)
{
  if (!gURLBar)
    return;

  if (!gProxyButton)
    gProxyButton = document.getElementById("page-proxy-button");
  if (!gProxyFavIcon)
    gProxyFavIcon = document.getElementById("page-proxy-favicon");
  if (!gProxyDeck)
    gProxyDeck = document.getElementById("page-proxy-deck");

  gProxyButton.setAttribute("pageproxystate", aState);

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
  else if (gProxyDeck.selectedIndex != 1)
    gProxyDeck.selectedIndex = 1;
}

function PageProxyClearIcon ()
{
  if (gProxyDeck.selectedIndex != 0)
    gProxyDeck.selectedIndex = 0;
  if (gProxyFavIcon.hasAttribute("src"))
    gProxyFavIcon.removeAttribute("src");
}

function PageProxyDragGesture(aEvent)
{
  if (gProxyButton.getAttribute("pageproxystate") == "valid") {
    nsDragAndDrop.startDrag(aEvent, proxyIconDNDObserver);
    return true;
  }
  return false;
}

function PageProxyClickHandler(aEvent)
{
  switch (aEvent.button) {
    case 0:
      gURLBar.select();
      break;
    case 1:
      if (gPrefService.getBoolPref("middlemouse.paste"))
        middleMousePaste(aEvent);
      break;
  }
  return true;
}

function URLBarOnDrop(evt)
{
  nsDragAndDrop.drop(evt, urlbarObserver);
}

var urlbarObserver = {
  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);

      // The URL bar automatically handles inputs with newline characters,
      // so we can get away with treating text/x-moz-url flavours as text/unicode.
      if (url) {
        getBrowser().dragDropSecurityCheck(aEvent, aDragSession, url);

        try {
          gURLBar.value = url;
          const nsIScriptSecMan = Components.interfaces.nsIScriptSecurityManager;
          urlSecurityCheck(gURLBar.value,
                           gBrowser.contentPrincipal,
                           nsIScriptSecMan.DISALLOW_INHERIT_PRINCIPAL);
          handleURLBarCommand();
        } catch (ex) {}
      }
    },
  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();

      // Plain text drops are often misidentified as "text/x-moz-url", so favor plain text.
      flavourSet.appendFlavour("text/unicode");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    }
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

function BrowserReloadWithFlags(reloadFlags)
{
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
  //   (*) browser messages
  //   (*) sidebar
  //   (*) find bar
  //   (*) statusbar

  var navToolbox = document.getElementById("navigator-toolbox");
  navToolbox.hidden = aHide;
  if (aHide)
  {
    gChromeState = {};
    var sidebar = document.getElementById("sidebar-box");
    gChromeState.sidebarOpen = !sidebar.hidden;
    gSidebarCommand = sidebar.getAttribute("sidebarcommand");

    var notificationBox = gBrowser.getNotificationBox();
    gChromeState.notificationsOpen = !notificationBox.notificationsHidden;
    notificationBox.notificationsHidden = aHide;

    var statusbar = document.getElementById("status-bar");
    gChromeState.statusbarOpen = !statusbar.hidden;
    statusbar.hidden = aHide;

    gChromeState.findOpen = !gFindBar.hidden;
    gFindBar.close();
  }
  else {
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
  toggleAffectedChrome(true);
}

function onExitPrintPreview()
{
  // restore chrome to original state
  toggleAffectedChrome(false);
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

  var texts = [titleText, XLinkTitleText];
  var tipNode = document.getElementById("aHTMLTooltip");
  tipNode.style.direction = direction;

  for (var i = 0; i < texts.length; ++i) {
    var t = texts[i];
    if (t && t.search(/\S/) >= 0) {
      // XXX - Short-term fix to bug 67127: collapse whitespace here
      tipNode.setAttribute("label", t.replace(/\s+/g, " ") );
      retVal = true;
    }
  }

  return retVal;
}

var proxyIconDNDObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var value = gURLBar.value;
      // XXX - do we want to allow the user to set a blank page to their homepage?
      //       if so then we want to modify this a little to set about:blank as
      //       the homepage in the event of an empty urlbar.
      if (!value) return;

      var urlString = value + "\n" + window.content.document.title;
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

#ifndef MOZ_PLACES_BOOKMARKS
var bookmarksButtonObserver = {
  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var split = aXferData.data.split("\n");
    var url = split[0];
    if (url != aXferData.data) {  //do nothing if it's not a valid URL
      var dialogArgs = {
        name: split[1],
        url: url
      }
      openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
                 BROWSER_ADD_BM_FEATURES, dialogArgs);
    }
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
#endif

var newTabButtonObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = gNavigatorBundle.getString("droponnewtabbutton");
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
        getBrowser().dragDropSecurityCheck(aEvent, aDragSession, url);
        // allow third-party services to fixup this URL
        openNewTabWith(url, null, postData.value, aEvent, true);
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
        getBrowser().dragDropSecurityCheck(aEvent, aDragSession, url);
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

var goButtonObserver = {
  onDragOver: function(aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = gNavigatorBundle.getString("dropongobutton");
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
      try {
        getBrowser().dragDropSecurityCheck(aEvent, aDragSession, url);
        urlSecurityCheck(url,
                         gBrowser.contentPrincipal,
                         Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
        loadURI(url, null, postData.value, true);
      } catch (ex) {}
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
      getBrowser().dragDropSecurityCheck(aEvent, aDragSession, url);

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

const BrowserSearch = {

  /**
   * Initialize the BrowserSearch
   */
  init: function() {
    gBrowser.addEventListener("DOMLinkAdded", 
                              function (event) { BrowserSearch.onLinkAdded(event); }, 
                              false);
  },

  /**
   * A new <link> tag has been discovered - check to see if it advertises
   * a OpenSearch engine.
   */
  onLinkAdded: function(event) {
    // XXX this event listener can/should probably be combined with the onLinkAdded
    // listener in tabbrowser.xml.  See comments in FeedHandler.onLinkAdded().
    const target = event.target;
    var etype = target.type;
    const searchRelRegex = /(^|\s)search($|\s)/i;
    const searchHrefRegex = /^(https?|ftp):\/\//i;

    if (!etype)
      return;
      
    // Bug 349431: If the engine has no suggested title, ignore it rather
    // than trying to find an alternative.
    if (!target.title)
      return;

    if (etype == "application/opensearchdescription+xml" &&
        searchRelRegex.test(target.rel) && searchHrefRegex.test(target.href))
    {
      const targetDoc = target.ownerDocument;
      // Set the attribute of the (first) search-engine button.
      var searchButton = document.getAnonymousElementByAttribute(this.getSearchBar(),
                                  "anonid", "searchbar-engine-button");
      if (searchButton) {
        var browser = gBrowser.getBrowserForDocument(targetDoc);
         // Append the URI and an appropriate title to the browser data.
        var iconURL = null;
        if (gBrowser.shouldLoadFavIcon(browser.currentURI))
          iconURL = browser.currentURI.prePath + "/favicon.ico";

        var hidden = false;
        // If this engine (identified by title) is already in the list, add it
        // to the list of hidden engines rather than to the main list.
        // XXX This will need to be changed when engines are identified by URL;
        // see bug 335102.
         var searchService =
            Components.classes["@mozilla.org/browser/search-service;1"]
                      .getService(Components.interfaces.nsIBrowserSearchService);
        if (searchService.getEngineByName(target.title))
          hidden = true;

        var engines = [];
        if (hidden) {
          if (browser.hiddenEngines)
            engines = browser.hiddenEngines;
        }
        else {
          if (browser.engines)
            engines = browser.engines;
        }

        engines.push({ uri: target.href,
                       title: target.title,
                       icon: iconURL });

         if (hidden) {
           browser.hiddenEngines = engines;
         }
         else {
           browser.engines = engines;
           if (browser == gBrowser || browser == gBrowser.mCurrentBrowser)
             this.updateSearchButton();
         }
      }
    }
  },

  /**
   * Update the browser UI to show whether or not additional engines are 
   * available when a page is loaded or the user switches tabs to a page that 
   * has search engines. 
   */
  updateSearchButton: function() {
    var searchButton = document.getAnonymousElementByAttribute(this.getSearchBar(),
                                "anonid", "searchbar-engine-button");
    if (!searchButton)
      return;
    var engines = gBrowser.mCurrentBrowser.engines;
    if (!engines || engines.length == 0) {
      if (searchButton.hasAttribute("addengines"))
        searchButton.removeAttribute("addengines");
    }
    else {
      searchButton.setAttribute("addengines", "true");
    }
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
    var searchBar = this.getSearchBar();
    if (searchBar) {
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
    if (this.getSearchBar())
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
      getBrowser().loadOneTab(submission.uri.spec, null, null,
                              submission.postData, null, false);
    } else
      loadURI(submission.uri.spec, null, submission.postData, false);
  },

  /**
   * Returns the search bar element if it is present in the toolbar and not
   * hidden, null otherwise.
   */
  getSearchBar: function BrowserSearch_getSearchBar() {
    var searchBar = document.getElementById("searchbar");
    if (searchBar && isElementVisible(searchBar))
      return searchBar;

    return null;
  },

  loadAddEngines: function BrowserSearch_loadAddEngines() {
    var newWindowPref = gPrefService.getIntPref("browser.link.open_newwindow");
    var where = newWindowPref == 3 ? "tab" : "window";
    var regionBundle = document.getElementById("bundle_browser_region");
    var searchEnginesURL = formatURL("browser.search.searchEnginesURL", true);
    openUILinkIn(searchEnginesURL, where);
  }
}

function FillHistoryMenu(aParent, aMenu)
  {
    // Remove old entries if any
    deleteHistoryItems(aParent);

    var webNav = getWebNavigation();
    var sessionHistory = webNav.sessionHistory;

    var count = sessionHistory.count;
    var index = sessionHistory.index;
    var end;
    var j;
    var entry;

    switch (aMenu)
      {
        case "back":
          end = (index > MAX_HISTORY_MENU_ITEMS) ? index - MAX_HISTORY_MENU_ITEMS : 0;
          if ((index - 1) < end) return false;
          for (j = index - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "forward":
          end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count - 1;
          if ((index + 1) > end) return false;
          for (j = index + 1; j <= end; j++)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
      }

    return true;
  }

function addToUrlbarHistory(aUrlToAdd)
{
  if (!aUrlToAdd)
     return;
  if (aUrlToAdd.search(/[\x00-\x1F]/) != -1) // don't store bad URLs
     return;

  if (!gGlobalHistory)
    gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                               .getService(Components.interfaces.nsIBrowserHistory);

  if (!gURIFixup)
    gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                          .getService(Components.interfaces.nsIURIFixup);
   try {
     if (aUrlToAdd.indexOf(" ") == -1) {
       var fixedUpURI = gURIFixup.createFixupURI(aUrlToAdd, 0);
       gGlobalHistory.markPageAsTyped(fixedUpURI);
     }
   }
   catch(ex) {
   }
}

function createMenuItem( aParent, aIndex, aLabel)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "label", aLabel );
    menuitem.setAttribute( "index", aIndex );
    aParent.appendChild( menuitem );
  }

function deleteHistoryItems(aParent)
{
  var children = aParent.childNodes;
  for (var i = children.length - 1; i >= 0; --i)
    {
      var index = children[i].getAttribute("index");
      if (index)
        aParent.removeChild(children[i]);
    }
}

function toJavaScriptConsole()
{
  toOpenWindowByType("global:console", "chrome://global/content/console.xul");
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
    window.openDialog(windowUri, "_blank", features, extraArgument);
  else
    window.openDialog(windowUri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar", extraArgument);
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

#ifdef TOOLBAR_CUSTOMIZATION_SHEET
  document.getElementById("customizeToolbarSheetBox").hidden = false;

  /**
   * XXXmano hack: when a new tab is added while the sheet is visible,
   * the new tabpanel is overlaying the sheet. We workaround this issue by
   * hiding and reopening the sheet when a new tab is added.
   */
  function TabOpenSheetHandler(aEvent) {
    getBrowser().removeEventListener("TabOpen", TabOpenSheetHandler, false);

    document.getElementById("customizeToolbarSheetIFrame")
            .contentWindow.gCustomizeToolbarSheet.done();
    document.getElementById("customizeToolbarSheetBox").hidden = true;
    BrowserCustomizeToolbar();
    
  }
  getBrowser().addEventListener("TabOpen", TabOpenSheetHandler, false);
#else
  window.openDialog("chrome://global/content/customizeToolbar.xul",
                    "CustomizeToolbar",
                    "chrome,all,dependent",
                    document.getElementById("navigator-toolbox"));
#endif
}

function BrowserToolboxCustomizeDone(aToolboxChanged)
{
#ifdef TOOLBAR_CUSTOMIZATION_SHEET
  document.getElementById("customizeToolbarSheetBox").hidden = true;
#endif

  // Update global UI elements that may have been added or removed
  if (aToolboxChanged) {
    gURLBar = document.getElementById("urlbar");
    if (gURLBar)
      gURLBar.clickSelectsAll = gClickSelectsAll;
    gProxyButton = document.getElementById("page-proxy-button");
    gProxyFavIcon = document.getElementById("page-proxy-favicon");
    gProxyDeck = document.getElementById("page-proxy-deck");
    gHomeButton.updateTooltip();
    window.XULBrowserWindow.init();
  }

  // Update the urlbar
  var url = getWebNavigation().currentURI.spec;
  if (gURLBar) {
    gURLBar.value = url == "about:blank" ? "" : url;
    SetPageProxyState("valid");
    XULBrowserWindow.asyncUpdateUI();    
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

#ifdef XP_MACOSX
  // make sure to re-enable click-and-hold
  if (!getBoolPref("ui.click_hold_context_menus", false))
    SetClickAndHoldHandlers();
#endif

#ifndef MOZ_PLACES_BOOKMARKS
  // fix up the personal toolbar folder
  var bt = document.getElementById("bookmarks-ptf");
  if (bt) {
    var btf = BMSVC.getBookmarksToolbarFolder().Value;
    var btchevron = document.getElementById("bookmarks-chevron");
    bt.ref = btf;
    btchevron.ref = btf;
    // no uniqueness is guaranteed, so we have to remove first
    try {
      bt.database.RemoveObserver(BookmarksToolbarRDFObserver);
    } catch (ex) {
      // ignore
    }
    bt.database.AddObserver(BookmarksToolbarRDFObserver);
    bt.builder.rebuild();
    btchevron.builder.rebuild();

    // fake a resize; this function takes care of flowing bookmarks
    // from the bar to the overflow item
    BookmarksToolbar.resizeFunc(null);
  }
#endif

#ifndef TOOLBAR_CUSTOMIZATION_SHEET
  // XXX Shouldn't have to do this, but I do
  window.focus();
#endif
}

var FullScreen =
{
  toggle: function()
  {
    // show/hide all menubars, toolbars, and statusbars (except the full screen toolbar)
    this.showXULChrome("toolbar", window.fullScreen);
    this.showXULChrome("statusbar", window.fullScreen);
    document.getElementById("fullScreenItem").setAttribute("checked", !window.fullScreen);
  },

  showXULChrome: function(aTag, aShow)
  {
    var XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var els = document.getElementsByTagNameNS(XULNS, aTag);

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

          // XXX See bug 202978: we disable the context menu
          // to prevent customization while in fullscreen, which
          // causes menu breakage.
          els[i].setAttribute("saved-context",
                              els[i].getAttribute("context"));
          els[i].removeAttribute("context");

          // Set the inFullscreen attribute to allow specific styling
          // in fullscreen mode
          els[i].setAttribute("inFullscreen", true);
        }
        else {
          function restoreAttr(attrName) {
            var savedAttr = "saved-" + attrName;
            if (els[i].hasAttribute(savedAttr)) {
              var savedValue = els[i].getAttribute(savedAttr);
              els[i].setAttribute(attrName, savedValue);
              els[i].removeAttribute(savedAttr);
            }
          }

          restoreAttr("mode");
          restoreAttr("iconsize");
          restoreAttr("context"); // XXX see above

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

    var toolbox = document.getElementById("navigator-toolbox");
    if (aShow)
      toolbox.removeAttribute("inFullscreen");
    else
      toolbox.setAttribute("inFullscreen", true);

#ifndef XP_MACOSX
    var controls = document.getElementsByAttribute("fullscreencontrol", "true");
    for (var i = 0; i < controls.length; ++i)
      controls[i].hidden = aShow;
#endif
  }
};

function nsBrowserStatusHandler()
{
  this.init();
}

nsBrowserStatusHandler.prototype =
{
  // Stored Status, Link and Loading values
  status : "",
  defaultStatus : "",
  jsStatus : "",
  jsDefaultStatus : "",
  overLink : "",
  startTime : 0,
  statusText: "",
  lastURI: null,

  statusTimeoutInEffect : false,

  QueryInterface : function(aIID)
  {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsIXULBrowserWindow) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  },

  init : function()
  {
    this.throbberElement        = document.getElementById("navigator-throbber");
    this.statusMeter            = document.getElementById("statusbar-icon");
    this.stopCommand            = document.getElementById("Browser:Stop");
    this.reloadCommand          = document.getElementById("Browser:Reload");
    this.reloadSkipCacheCommand = document.getElementById("Browser:ReloadSkipCache");
    this.statusTextField        = document.getElementById("statusbar-display");
    this.securityButton         = document.getElementById("security-button");
    this.urlBar                 = document.getElementById("urlbar");
    this.isImage                = document.getElementById("isImage");

    // Initialize the security button's state and tooltip text
    var securityUI = getBrowser().securityUI;
    this.onSecurityChange(null, null, securityUI.state);
  },

  destroy : function()
  {
    // XXXjag to avoid leaks :-/, see bug 60729
    this.throbberElement        = null;
    this.statusMeter            = null;
    this.stopCommand            = null;
    this.reloadCommand          = null;
    this.reloadSkipCacheCommand = null;
    this.statusTextField        = null;
    this.securityButton         = null;
    this.urlBar                 = null;
    this.statusText             = null;
    this.lastURI                = null;
  },

  setJSStatus : function(status)
  {
    this.jsStatus = status;
    this.updateStatusField();
  },

  setJSDefaultStatus : function(status)
  {
    this.jsDefaultStatus = status;
    this.updateStatusField();
  },

  setDefaultStatus : function(status)
  {
    this.defaultStatus = status;
    this.updateStatusField();
  },

  setOverLink : function(link, b)
  {
    this.overLink = link;
    this.updateStatusField();
  },

  updateStatusField : function()
  {
    var text = this.overLink || this.status || this.jsStatus || this.jsDefaultStatus || this.defaultStatus;

    // check the current value so we don't trigger an attribute change
    // and cause needless (slow!) UI updates
    if (this.statusText != text) {
      this.statusTextField.label = text;
      this.statusText = text;
    }
  },
  
  mimeTypeIsTextBased : function(contentType)
  {
    return /^text\/|\+xml$/.test(contentType) ||
           contentType == "application/x-javascript" ||
           contentType == "application/xml" ||
           contentType == "mozilla.application/cached-xul";
  },

  onLinkIconAvailable : function(aBrowser)
  {
    if (gProxyFavIcon &&
        gBrowser.mCurrentBrowser == aBrowser &&
        gBrowser.userTypedValue === null)
    {
      // update the favicon in the URL bar
      PageProxySetIcon(aBrowser.mIconURL);
    }

#ifdef MOZ_PLACES_BOOKMARKS
    // Save this favicon in the favicon service
    if (aBrowser.mIconURL) {
      var faviconService = Components.classes["@mozilla.org/browser/favicon-service;1"]
        .getService(Components.interfaces.nsIFaviconService);
      var uri = Components.classes["@mozilla.org/network/io-service;1"]
        .getService(Components.interfaces.nsIIOService).newURI(aBrowser.mIconURL, null, null);
      faviconService.setAndLoadFaviconForPage(aBrowser.currentURI, uri, false);
    }
#endif
  },

  onProgressChange : function (aWebProgress, aRequest,
                               aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress)
  {
    if (aMaxTotalProgress > 0) {
      // This is highly optimized.  Don't touch this code unless
      // you are intimately familiar with the cost of setting
      // attrs on XUL elements. -- hyatt
      var percentage = (aCurTotalProgress * 100) / aMaxTotalProgress;
      this.statusMeter.value = percentage;
    }
  },

  onProgressChange64 : function (aWebProgress, aRequest,
                                 aCurSelfProgress, aMaxSelfProgress,
                                 aCurTotalProgress, aMaxTotalProgress)
  {
    return this.onProgressChange(aWebProgress, aRequest,
      aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress,
      aMaxTotalProgress);
  },

  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
  {
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
          var browser = gBrowser.mCurrentBrowser;
          if (!gBrowser.mTabbedMode && !browser.mIconURL)
            gBrowser.useDefaultIcon(gBrowser.mCurrentTab);
#ifndef MOZ_PLACES_BOOKMARKS
          if (browser.mIconURL)
            BookmarksUtils.loadFavIcon(browser.currentURI.spec, browser.mIconURL);
#endif
        }
      }

      // This (thanks to the filter) is a network stop or the last
      // request stop outside of loading the document, stop throbbers
      // and progress bars and such
      if (aRequest) {
        var msg = "";
          // Get the URI either from a channel or a pseudo-object
          if (aRequest instanceof nsIChannel || "URI" in aRequest) {
            var location = aRequest.URI;

            // For keyword URIs clear the user typed value since they will be changed into real URIs
            if (location.scheme == "keyword" && aWebProgress.DOMWindow == content)
              getBrowser().userTypedValue = null;

            if (location.spec != "about:blank") {
              const kErrorBindingAborted = 0x804B0002;
              const kErrorNetTimeout = 0x804B000E;
              switch (aStatus) {
                case kErrorBindingAborted:
                  msg = gNavigatorBundle.getString("nv_stopped");
                  break;
                case kErrorNetTimeout:
                  msg = gNavigatorBundle.getString("nv_timeout");
                  break;
              }
            }
          }
          // If msg is false then we did not have an error (channel may have
          // been null, in the case of a stray image load).
          if (!msg && (!location || location.spec != "about:blank")) {
            msg = gNavigatorBundle.getString("nv_done");
          }
          this.status = "";
          this.setDefaultStatus(msg);

          // Disable menu entries for images, enable otherwise
          if (content.document && this.mimeTypeIsTextBased(content.document.contentType))
            this.isImage.removeAttribute('disabled');
          else
            this.isImage.setAttribute('disabled', 'true');
        }

        // Turn the progress meter and throbber off.
        gProgressCollapseTimer = window.setTimeout(
          function() {
            gProgressMeterPanel.collapsed = true;
            gProgressCollapseTimer = null;
          }, 100);

        if (this.throbberElement)
          this.throbberElement.removeAttribute("busy");

        this.stopCommand.setAttribute("disabled", "true");
    }
  },

  onLocationChange : function(aWebProgress, aRequest, aLocationURI)
  {
    var location = aLocationURI ? aLocationURI.spec : "";
 
    if (document.tooltipNode) {
      // Optimise for the common case
      if (aWebProgress.DOMWindow == content) {
        document.getElementById("aHTMLTooltip").hidePopup();
        document.tooltipNode = null;
      }
      else {
        for (var tooltipWindow =
               document.tooltipNode.target.ownerDocument.defaultView;
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
    var selectedBrowser = getBrowser().selectedBrowser;
    if (selectedBrowser.lastURI) {
      var oldSpec = selectedBrowser.lastURI.spec;
      var oldIndexOfHash = oldSpec.indexOf("#");
      if (oldIndexOfHash != -1)
        oldSpec = oldSpec.substr(0, oldIndexOfHash);
      var newSpec = location;
      var newIndexOfHash = newSpec.indexOf("#");
      if (newIndexOfHash != -1)
        newSpec = newSpec.substr(0, newSpec.indexOf("#"));
      if (newSpec != oldSpec) {
        gBrowser.getNotificationBox(selectedBrowser).removeAllNotifications(true);
      }
    }
    selectedBrowser.lastURI = aLocationURI;

    // Disable menu entries for images, enable otherwise
    if (content.document && this.mimeTypeIsTextBased(content.document.contentType))
      this.isImage.removeAttribute('disabled');
    else
      this.isImage.setAttribute('disabled', 'true');

    this.setOverLink("", null);

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation

    var browser = getBrowser().selectedBrowser;
    if (aWebProgress.DOMWindow == content) {

      if (location == "about:blank" || location == "") {   //second condition is for new tabs, otherwise
        location = "";                                     //reload function is enabled until tab is refreshed
        this.reloadCommand.setAttribute("disabled", "true");
        this.reloadSkipCacheCommand.setAttribute("disabled", "true");
      } else {
        this.reloadCommand.removeAttribute("disabled");
        this.reloadSkipCacheCommand.removeAttribute("disabled");
      }

      // The document loaded correctly, clear the value if we should
      if (browser.userTypedClear > 0 && aRequest)
        browser.userTypedValue = null;

      if (!gBrowser.mTabbedMode && aWebProgress.isLoadingDocument)
        gBrowser.setIcon(gBrowser.mCurrentTab, null);

      //XXXBlake don't we have to reinit this.urlBar, etc.
      //         when the toolbar changes?
      if (gURLBar) {
        var userTypedValue = browser.userTypedValue;
        if (!userTypedValue) {
          // If the url has "wyciwyg://" as the protocol, strip it off.
          // Nobody wants to see it on the urlbar for dynamically generated
          // pages.
          if (!gURIFixup)
            gURIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                                  .getService(Components.interfaces.nsIURIFixup);
          if (location && gURIFixup) {
            try {
              location = gURIFixup.createExposableURI(aLocationURI).spec;
            } catch (ex) {}
          }

          gURLBar.value = location;
          SetPageProxyState("valid");

          // Setting the urlBar value in some cases causes userTypedValue to
          // become set because of oninput, so reset it to its old value.
          browser.userTypedValue = userTypedValue;
        } else {
          gURLBar.value = userTypedValue;
          SetPageProxyState("invalid");
        }
      }
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
    if (aRequest) {
      var self = this;
      setTimeout(function() { self.asyncUpdateUI(); }, 0);
    } 
    else
      this.asyncUpdateUI();
  },
  
  asyncUpdateUI : function () {
    FeedHandler.updateFeeds();
    BrowserSearch.updateSearchButton();
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    this.status = aMessage;
    this.updateStatusField();
  },

  onRefreshAttempted : function(aWebProgress, aURI, aDelay, aSameURI)
  {
    if (gPrefService.getBoolPref("accessibility.blockautorefresh")) {
      var brandBundle = document.getElementById("bundle_brand");
      var brandShortName = brandBundle.getString("brandShortName");
      var refreshButtonText = 
        gNavigatorBundle.getString("refreshBlocked.goButton");
      var refreshButtonAccesskey = 
        gNavigatorBundle.getString("refreshBlocked.goButton.accesskey");
      var message;
      if (aSameURI)
        message = gNavigatorBundle.getFormattedString(
          "refreshBlocked.refreshLabel", [brandShortName]);
      else
        message = gNavigatorBundle.getFormattedString(
          "refreshBlocked.redirectLabel", [brandShortName]);
      var topBrowser = getBrowserFromContentWindow(aWebProgress.DOMWindow.top);
      var docShell = aWebProgress.DOMWindow
                                 .QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell);
      var notificationBox = gBrowser.getNotificationBox(topBrowser);
      var notification = notificationBox.getNotificationWithValue(
        "refresh-blocked");
      if (notification) {
        notification.label = message;
        notification.refreshURI = aURI;
        notification.delay = aDelay;
        notification.docShell = docShell;
      }
      else {
        var buttons = [{
          label: refreshButtonText,
          accessKey: refreshButtonAccesskey,
          callback: function(aNotification, aButton) {
            var refreshURI = aNotification.docShell
                                          .QueryInterface(Ci.nsIRefreshURI);
            refreshURI.forceRefreshURI(aNotification.refreshURI,
                                       aNotification.delay, true);
          }
        }];
        const priority = notificationBox.PRIORITY_INFO_MEDIUM;
        notification = notificationBox.appendNotification(
          message,
          "refresh-blocked",
          "chrome://browser/skin/Info.png",
          priority,
          buttons);
        notification.refreshURI = aURI;
        notification.delay = aDelay;
        notification.docShell = docShell;
      }
      return false;
    }
    return true;
  },

  onSecurityChange : function(aWebProgress, aRequest, aState)
  {
    const wpl = Components.interfaces.nsIWebProgressListener;
    this.securityButton.removeAttribute("label");

    switch (aState) {
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_HIGH:
        this.securityButton.setAttribute("level", "high");
        if (this.urlBar)
          this.urlBar.setAttribute("level", "high");
        try {
          this.securityButton.setAttribute("label",
            gBrowser.contentWindow.location.host);
        } catch(exception) {}
        break;
      case wpl.STATE_IS_SECURE | wpl.STATE_SECURE_LOW:
        this.securityButton.setAttribute("level", "low");
        if (this.urlBar)
          this.urlBar.setAttribute("level", "low");
        try {
          this.securityButton.setAttribute("label",
            gBrowser.contentWindow.location.host);
        } catch(exception) {}
        break;
      case wpl.STATE_IS_BROKEN:
        this.securityButton.setAttribute("level", "broken");
        if (this.urlBar)
          this.urlBar.setAttribute("level", "broken");
        break;
      case wpl.STATE_IS_INSECURE:
      default:
        this.securityButton.removeAttribute("level");
        if (this.urlBar)
          this.urlBar.removeAttribute("level");
        break;
    }

    var securityUI = gBrowser.securityUI;
    this.securityButton.setAttribute("tooltiptext", securityUI.tooltipText);
    var lockIcon = document.getElementById("lock-icon");
    if (lockIcon)
      lockIcon.setAttribute("tooltiptext", securityUI.tooltipText);
  },

  // simulate all change notifications after switching tabs
  onUpdateCurrentBrowser : function(aStateFlags, aStatus, aMessage, aTotalProgress)
  {
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

  startDocumentLoad : function(aRequest)
  {
    // It's okay to clear what the user typed when we start
    // loading a document. If the user types, this counter gets
    // set to zero, if the document load ends without an
    // onLocationChange, this counter gets decremented
    // (so we keep it while switching tabs after failed loads)
    getBrowser().userTypedClear++;

    // clear out feed data
    gBrowser.mCurrentBrowser.feeds = null;

    // clear out search-engine data
    gBrowser.mCurrentBrowser.engines = null;    

    const nsIChannel = Components.interfaces.nsIChannel;
    var urlStr = aRequest.QueryInterface(nsIChannel).URI.spec;
    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);
    try {
      observerService.notifyObservers(content, "StartDocumentLoad", urlStr);
    } catch (e) {
    }
  },

  endDocumentLoad : function(aRequest, aStatus)
  {
    // The document is done loading, we no longer want the
    // value cleared.
    if (getBrowser().userTypedClear > 0)
      getBrowser().userTypedClear--;

    const nsIChannel = Components.interfaces.nsIChannel;
    var urlStr = aRequest.QueryInterface(nsIChannel).originalURI.spec;

    var observerService = Components.classes["@mozilla.org/observer-service;1"]
                                    .getService(Components.interfaces.nsIObserverService);

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
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      switch (aContext) {
        case Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL :
          aWhere = gPrefService.getIntPref("browser.link.open_external");
          break;
        default : // OPEN_NEW or an illegal value
          aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
      }
    }
    var url = aURI ? aURI.spec : "about:blank";
    switch(aWhere) {
      case Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW :
        newWindow = openDialog(getBrowserURL(), "_blank", "all,dialog=no", url);
        break;
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB :
        var loadInBackground = gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground");
        var newTab = gBrowser.loadOneTab("about:blank", null, null, null, loadInBackground, false);
        newWindow = gBrowser.getBrowserForTab(newTab).docShell
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow);
        try {
          if (aOpener) {
            location = aOpener.location;
            referrer =
                    Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService)
                              .newURI(location, null, null);
          }
          newWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .loadURI(url, loadflags, referrer, null, null);
        } catch(e) {
        }
        break;
      default : // OPEN_CURRENTWINDOW or an illegal value
        try {
          if (aOpener) {
            newWindow = aOpener.top;
            location = aOpener.location;
            referrer =
                    Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService)
                              .newURI(location, null, null);

            newWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(nsIWebNavigation)
                     .loadURI(url, loadflags, referrer, null, null);
          } else {
            newWindow = gBrowser.selectedBrowser.docShell
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
            getWebNavigation().loadURI(url, loadflags, null, null, null);
          }
          if(!gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground"))
            content.focus();
        } catch(e) {
        }
    }
    return newWindow;
  },

  isTabContentWindow : function(aWindow)
  {
    var browsers = gBrowser.browsers;
    for (var ctr = 0; ctr < browsers.length; ctr++)
      if (browsers.item(ctr).contentWindow == aWindow)
        return true;
    return false;
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

  var toolbox = document.getElementById("navigator-toolbox");
  for (i = 0; i < toolbox.childNodes.length; ++i) {
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

      menuItem.addEventListener("command", onViewToolbarCommand, false);
    }
    toolbar = toolbar.nextSibling;
  }
}

function onViewToolbarCommand(aEvent)
{
  var toolbox = document.getElementById("navigator-toolbox");
  var index = aEvent.originalTarget.getAttribute("toolbarindex");
  var toolbar = toolbox.childNodes[index];

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
 *                  opened regardless of it's current state (optional).
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
      sidebarBox.hidden = true;
      sidebarSplitter.hidden = true;
      content.focus();
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
  sidebar.setAttribute("src", url);
  sidebarBox.setAttribute("sidebarcommand", sidebarBroadcaster.id);
  sidebarTitle.value = title;

  // This is used because we want to delay sidebar load a bit
  // when a browser window opens. See delayedStartup()
  sidebarBox.setAttribute("src", url);
}

var gHomeButton = {
  prefDomain: "browser.startup.homepage",
  observe: function (aSubject, aTopic, aPrefName)
  {
    if (aTopic != "nsPref:changed" || aPrefName != this.prefDomain)
      return;

    this.updateTooltip();
  },

  updateTooltip: function ()
  {
    var homeButton = document.getElementById("home-button");
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
         var dialogArgs = {
           name: wrapper.getAttribute("title"),
           url: wrapper.href,
           bWebPanel: true
         }
#ifndef MOZ_PLACES_BOOKMARKS
         openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
                    BROWSER_ADD_BM_FEATURES, dialogArgs);
         event.preventDefault();
#else
         PlacesUtils.showAddBookmarkUI(makeURI(wrapper.href),
                                       wrapper.getAttribute("title"),
                                       null, true, true);
         event.preventDefault();
#endif
         return false;
       }
       else if (target == "_search") {
         // Used in WinIE as a way of transiently loading pages in a sidebar.  We
         // mimic that WinIE functionality here and also load the page transiently.

         // DISALLOW_INHERIT_PRINCIPAL is used here in order to also
         // block javascript and data: links targeting the sidebar.
         try {
           const nsIScriptSecurityMan = Ci.nsIScriptSecurityManager;
           urlSecurityCheck(wrapper.href,
                            wrapper.ownerDocument.nodePrincipal,
                            nsIScriptSecurityMan.DISALLOW_INHERIT_PRINCIPAL);
         }
         catch(ex) {
           return false;
         } 

         openWebPanel(gNavigatorBundle.getString("webPanels"), wrapper.href);
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
  }

  openUILink(url,
             event,
             true /* ignore the fact this is a middle click */);

  event.stopPropagation();
}

function makeURLAbsolute( base, url )
{
  // Construct nsIURL.
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
  var baseURI  = ioService.newURI(base, null, null);

  return ioService.newURI(baseURI.resolve(url), null, null).spec;
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

      getBrowser().dragDropSecurityCheck(aEvent, aDragSession, url);

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

// For extensions
function getBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

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
  var docCharset = getBrowser().docShell.QueryInterface(
                            Components.interfaces.nsIDocCharset);
  docCharset.charset = aCharset;
  BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function BrowserSetForcedDetector(doReload)
{
  getBrowser().documentCharsetInfo.forcedDetector = true;
  if (doReload)
    BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function UpdateCurrentCharset()
{
    var menuitem = null;

    // exctract the charset from DOM
    var wnd = document.commandDispatcher.focusedWindow;
    if ((window == wnd) || (wnd == null)) wnd = window.content;
    menuitem = document.getElementById('charset.' + wnd.document.characterSet);

    if (menuitem) {
        // uncheck previously checked item to workaround Mac checkmark problem
        // bug 98625
        if (gPrevCharset) {
            var pref_item = document.getElementById('charset.' + gPrevCharset);
            if (pref_item)
              pref_item.setAttribute('checked', 'false');
        }
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
function getStyleSheetArray(frame)
{
  var styleSheets = frame.document.styleSheets;
  var styleSheetsArray = new Array(styleSheets.length);
  for (var i = 0; i < styleSheets.length; i++) {
    styleSheetsArray[i] = styleSheets[i];
  }
  return styleSheetsArray;
}

function getAllStyleSheets(frameset)
{
  var styleSheetsArray = getStyleSheetArray(frameset);
  for (var i = 0; i < frameset.frames.length; i++) {
    var frameSheets = getAllStyleSheets(frameset.frames[i]);
    styleSheetsArray = styleSheetsArray.concat(frameSheets);
  }
  return styleSheetsArray;
}

function stylesheetFillPopup(menuPopup)
{
  var noStyle = menuPopup.firstChild;
  var persistentOnly = noStyle.nextSibling;
  var sep = persistentOnly.nextSibling;
  while (sep.nextSibling)
    menuPopup.removeChild(sep.nextSibling);

  var styleSheets = getAllStyleSheets(window.content);
  var currentStyleSheets = [];
  var styleDisabled = getMarkupDocumentViewer().authorStyleDisabled;
  var haveAltSheets = false;
  var altStyleSelected = false;

  for (var i = 0; i < styleSheets.length; ++i) {
    var currentStyleSheet = styleSheets[i];

    // Skip any stylesheets that don't match the screen media type.
    var media = currentStyleSheet.media.mediaText.toLowerCase();
    if (media && (media.indexOf("screen") == -1) && (media.indexOf("all") == -1))
        continue;

    if (currentStyleSheet.title) {
      if (!currentStyleSheet.disabled)
        altStyleSelected = true;

      haveAltSheets = true;

      var lastWithSameTitle = null;
      if (currentStyleSheet.title in currentStyleSheets)
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];

      if (!lastWithSameTitle) {
        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("type", "radio");
        menuItem.setAttribute("label", currentStyleSheet.title);
        menuItem.setAttribute("data", currentStyleSheet.title);
        menuItem.setAttribute("checked", !currentStyleSheet.disabled && !styleDisabled);
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else {
        if (currentStyleSheet.disabled)
          lastWithSameTitle.removeAttribute("checked");
      }
    }
  }

  noStyle.setAttribute("checked", styleDisabled);
  persistentOnly.setAttribute("checked", !altStyleSelected && !styleDisabled);
  persistentOnly.hidden = (window.content.document.preferredStyleSheetSet) ? haveAltSheets : false;
  sep.hidden = (noStyle.hidden && persistentOnly.hidden) || !haveAltSheets;
  return true;
}

function stylesheetInFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (var i = 0; i < docStyleSheets.length; ++i) {
    if (docStyleSheets[i].title == title)
      return true;
  }
  return false;
}

function stylesheetSwitchFrame(frame, title) {
  var docStyleSheets = frame.document.styleSheets;

  for (var i = 0; i < docStyleSheets.length; ++i) {
    var docStyleSheet = docStyleSheets[i];

    if (title == "_nostyle")
      docStyleSheet.disabled = true;
    else if (docStyleSheet.title)
      docStyleSheet.disabled = (docStyleSheet.title != title);
    else if (docStyleSheet.disabled)
      docStyleSheet.disabled = false;
  }
}

function stylesheetSwitchAll(frameset, title) {
  if (!title || title == "_nostyle" || stylesheetInFrame(frameset, title)) {
    stylesheetSwitchFrame(frameset, title);
  }
  for (var i = 0; i < frameset.frames.length; i++) {
    stylesheetSwitchAll(frameset.frames[i], title);
  }
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

function WindowIsClosing()
{
  var browser = getBrowser();
  var cn = browser.tabContainer.childNodes;
  var numtabs = cn.length;
  var reallyClose = browser.warnAboutClosingTabs(true);

  for (var i = 0; reallyClose && i < numtabs; ++i) {
    var ds = browser.getBrowserForTab(cn[i]).docShell;

    if (ds.contentViewer && !ds.contentViewer.permitUnload())
      reallyClose = false;
  }

  if (reallyClose)
    return closeWindow(false);

  return reallyClose;
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

function BrowserOpenAddonsMgr()
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

  var docURI = makeURI(node.ownerDocument.URL,
                       node.ownerDocument.characterSet);

  var formURI = makeURI(node.form.getAttribute("action"),
                        node.ownerDocument.characterSet,
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

#ifndef MOZ_PLACES_BOOKMARKS
  var dialogArgs = {
    name: "",
    url: spec,
    charset: node.ownerDocument.characterSet,
    bWebPanel: false,
    keyword: "",
    bNeedKeyword: true,
    postData: postData,
    description: BookmarksUtils.getDescriptionFromDocument(node.ownerDocument)
  }
  openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
             BROWSER_ADD_BM_FEATURES, dialogArgs);
#else
  dump("*** IMPLEMENT ME: Bug 329281\n");
#endif
}

function SwitchDocumentDirection(aWindow) {
  aWindow.document.dir = (aWindow.document.dir == "ltr" ? "rtl" : "ltr");
  for (var run = 0; run < aWindow.frames.length; run++)
    SwitchDocumentDirection(aWindow.frames[run]);
}

function missingPluginInstaller(){
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

missingPluginInstaller.prototype.installSinglePlugin = function(aEvent){
  var tabbrowser = getBrowser();
  var missingPluginsArray = new Object;

  var pluginInfo = getPluginInfo(aEvent.target);
  missingPluginsArray[pluginInfo.mimetype] = pluginInfo;

  if (missingPluginsArray) {
    window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                      "PFSWindow", "modal,chrome,resizable=yes",
                      {plugins: missingPluginsArray, tab: tabbrowser.mCurrentTab});
  }

  aEvent.preventDefault();
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

  if (!(aEvent.target instanceof HTMLObjectElement)) {
    aEvent.target.addEventListener("click",
                                   gMissingPluginInstaller.installSinglePlugin,
                                   false);
  }

  var tabbrowser = getBrowser();
  const browsers = tabbrowser.mPanelContainer.childNodes;

  var window = aEvent.target.ownerDocument.defaultView;
  // walk up till the toplevel window
  while (window.parent != window)
    window = window.parent;

  var i = 0;
  for (; i < browsers.length; i++) {
    if (tabbrowser.getBrowserAtIndex(i).contentWindow == window)
      break;
  }

  var tab = tabbrowser.mTabContainer.childNodes[i];
  if (!tab.missingPlugins)
    tab.missingPlugins = new Object();

  var pluginInfo = getPluginInfo(aEvent.target);

  tab.missingPlugins[pluginInfo.mimetype] = pluginInfo;

  var browser = tabbrowser.getBrowserAtIndex(i);
  var notificationBox = gBrowser.getNotificationBox(browser);
  if (!notificationBox.getNotificationWithValue("missing-plugins")) {
    var bundle_browser = document.getElementById("bundle_browser");
    var messageString = bundle_browser.getString("missingpluginsMessage.title");
    var buttons = [{
      label: bundle_browser.getString("missingpluginsMessage.button.label"),
      accessKey: bundle_browser.getString("missingpluginsMessage.button.accesskey"),
      popup: null,
      callback: pluginsMissing
    }];

    const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
    const iconURL = "chrome://mozapps/skin/xpinstall/xpinstallItemGeneric.png";
    notificationBox.appendNotification(messageString, "missing-plugins",
                                       iconURL, priority, buttons);
  }
}

missingPluginInstaller.prototype.closeNotification = function() {
  var notificationBox = gBrowser.getNotificationBox();
  var notification = notificationBox.getNotificationWithValue("missing-plugins");

  if (notification) {
    notificationBox.removeNotification(notification);
  }
}

function pluginsMissing()
{
  // get the urls of missing plugins
  var tabbrowser = getBrowser();
  var missingPluginsArray = tabbrowser.mCurrentTab.missingPlugins;
  if (missingPluginsArray) {
    window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
      "PFSWindow", "modal,chrome,resizable=yes", {plugins: missingPluginsArray, tab: tabbrowser.mCurrentTab});
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
   * Initialize the Feed Handler
   */
  init: function() {
    gBrowser.addEventListener("DOMLinkAdded", 
                              function (event) { FeedHandler.onLinkAdded(event); }, 
                              true);
    gBrowser.addEventListener("pagehide", FeedHandler.onPageHide, true);
  },
  
  onPageHide: function(event) {
    var theBrowser = gBrowser.getBrowserForDocument(event.target);
    if (theBrowser)
      theBrowser.feeds = null;
  },
  
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
    
  /**
   * Locate the shell that has a specified document loaded in it. 
   * @param   doc
   *          The document to find a shell for.
   * @returns The doc shell that contains the specified document.
   */
  _getContentShell: function(doc) {
    var browsers = getBrowser().browsers;
    for (var i = 0; i < browsers.length; i++) {
      var shell = findChildShell(doc, browsers[i].docShell, null);
      if (shell)
        return { shell: shell, browser: browsers[i] };
    }
    return null;
  },
  
#ifndef MOZ_PLACES_BOOKMARKS
  /**
   * Adds a Live Bookmark to a feed
   * @param     url
   *            The URL of the feed being bookmarked
   * @title     title
   *            The title of the feed. Optional.
   * @subtitle  subtitle
   *            A short description of the feed. Optional.
   */
  addLiveBookmark: function(url, feedTitle, feedSubtitle) {
    var doc = gBrowser.selectedBrowser.contentDocument;
    var title = (arguments.length > 1) ? feedTitle : doc.title;
    var description;
    if (arguments.length > 2)
      description = feedSubtitle;
    else
      description = BookmarksUtils.getDescriptionFromDocument(doc);
    BookmarksUtils.addLivemark(doc.baseURI, url, title, description);
  },
#endif

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
  
  /**
   * A new <link> tag has been discovered - check to see if it advertises
   * an RSS feed. 
   */
  onLinkAdded: function(event) {
    // XXX this event listener can/should probably be combined with the onLinkAdded
    // listener in tabbrowser.xml, which only listens for favicons and then passes
    // them to onLinkIconAvailable in the ProgressListener.  We could extend the
    // progress listener to have a generic onLinkAvailable and have tabbrowser pass
    // along all events.  It should give us the browser for the tab, as well as
    // the actual event.

    var erel = event.target.rel;
    var etype = event.target.type;
    var etitle = event.target.title;
    const rssTitleRegex = /(^|\s)rss($|\s)/i;
    var rels = {};

    if (erel) {
      for each (var relValue in erel.split(/\s+/))
        rels[relValue] = true;
    }
    var isFeed = rels.feed;

    if (!isFeed &&
        (!rels.alternate || rels.stylesheet || !etype))
      return;

    if (!isFeed) {
      // Use type value
      etype = etype.replace(/^\s+/, "");
      etype = etype.replace(/\s+$/, "");
      etype = etype.replace(/\s*;.*/, "");
      etype = etype.toLowerCase();
      isFeed = (etype == "application/rss+xml" ||
                etype == "application/atom+xml");
      if (!isFeed) {
        // really slimy: general XML types with magic letters in the title
        isFeed = ((etype == "text/xml" || etype == "application/xml" ||
                   etype == "application/rdf+xml") && rssTitleRegex.test(etitle));
      }
    }

    if (isFeed) {
      const targetDoc = event.target.ownerDocument;

      // find which tab this is for, and set the attribute on the browser
      var browserForLink = gBrowser.getBrowserForDocument(targetDoc);
      if (!browserForLink) {
        // ??? this really shouldn't happen..
        return;
      }

      var feeds = [];
      if (browserForLink.feeds != null)
        feeds = browserForLink.feeds;
      var wrapper = event.target;

      try { 
        urlSecurityCheck(wrapper.href,
                         gBrowser.contentPrincipal,
                         Ci.nsIScriptSecurityManager.DISALLOW_INHERIT_PRINCIPAL);
      }
      catch (ex) {
        dump(ex.message);
        return; // doesn't pass security check
      }

      feeds.push({ href: wrapper.href,
                   type: etype,
                   title: wrapper.title});
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

#ifdef MOZ_PLACES
#include browser-places.js
#endif

/**
 * This object is for augmenting tabs
 */
var AugmentTabs = {

  tabContextMenu: null,
  undoCloseTabMenu: null,

  /**
   * Called in delayedStartup
   */
  init: function at_init() {
    // get tab context menu
    var tabbrowser = getBrowser();
    this.tabContextMenu = document.getAnonymousElementByAttribute(tabbrowser, "anonid", "tabContextMenu");

    // listen for tab-context menu showing
    this.tabContextMenu.addEventListener("popupshowing", this.onTabContextMenuLoad, false);

    if (gPrefService.getBoolPref("browser.sessionstore.enabled"))
      this._addUndoCloseTabContextMenu();
  },

  /**
   * Add undo-close-tab to tab context menu
   */
  _addUndoCloseTabContextMenu: function at_addUndoCloseTabContextMenu() {
    // get strings 
    var menuLabel = gNavigatorBundle.getString("tabContext.undoCloseTab");
    var menuAccessKey = gNavigatorBundle.getString("tabContext.undoCloseTabAccessKey");

    // create new menu item
    var undoCloseTabItem = document.createElement("menuitem");
    undoCloseTabItem.setAttribute("id", "tabContextUndoCloseTab");
    undoCloseTabItem.setAttribute("label", menuLabel);
    undoCloseTabItem.setAttribute("accesskey", menuAccessKey);
    undoCloseTabItem.setAttribute("command", "History:UndoCloseTab");

    // add to tab context menu
    var insertPos = this.tabContextMenu.lastChild.previousSibling;
    this.undoCloseTabMenu = this.tabContextMenu.insertBefore(undoCloseTabItem, insertPos);
  },

  onTabContextMenuLoad: function at_onTabContextMenuLoad() {
    if (AugmentTabs.undoCloseTabMenu) {
      // only add the menu of there are tabs to restore
      var ss = Cc["@mozilla.org/browser/sessionstore;1"].
               getService(Ci.nsISessionStore);
      AugmentTabs.undoCloseTabMenu.hidden = !(ss.getClosedTabCount(window) > 0);
    }
  }
};

/**
* History menu initialization
*/
#ifndef MOZ_PLACES
var HistoryMenu = {};
#endif

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
    var m = undoPopup.appendChild(document.createElement("menuitem"));
    m.setAttribute("label", undoItems[i].title);
    m.setAttribute("value", i);
    m.setAttribute("oncommand", "undoCloseTab(" + i + ");");
    m.addEventListener("click", undoCloseMiddleClick, false);
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
  getBrowser().moveTabToEnd();
}

/**
 * Re-open a closed tab.
 * @param aIndex
 *        The index of the tab (via nsSessionStore.getClosedTabData)
 */
function undoCloseTab(aIndex) {
  // wallpaper patch to prevent an unnecessary blank tab (bug 343895)
  var tabbrowser = getBrowser();
  var blankTabToRemove = null;
  if (tabbrowser.tabContainer.childNodes.length == 1 &&
      !gPrefService.getBoolPref("browser.tabs.autoHide") &&
      tabbrowser.selectedBrowser.sessionHistory.count < 2 &&
      tabbrowser.selectedBrowser.currentURI.spec == "about:blank" &&
      !tabbrowser.selectedBrowser.contentDocument.body.hasChildNodes() &&
      !tabbrowser.selectedTab.hasAttribute("busy"))
    blankTabToRemove = tabbrowser.selectedTab;

  var ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  if (ss.getClosedTabCount(window) == 0)
    return;
  ss.undoCloseTab(window, aIndex || 0);

  if (blankTabToRemove)
    tabbrowser.removeTab(blankTabToRemove);
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
 * This object encapsulates both legacy and places-based implementations
 * of the bookmark-all-tabs command. It also takes care of updating the command
 * enabled-state when tabs are created or removed.
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
#ifdef MOZ_PLACES_BOOKMARKS
    PlacesCommandHook.bookmarkCurrentPages();
#else
    addBookmarkAs(gBrowser, true);
#endif
  },

  // nsIDOMEventListener
  handleEvent: function(aEvent) {
    this._updateCommandState(aEvent.type == "TabClose");
  }
};
