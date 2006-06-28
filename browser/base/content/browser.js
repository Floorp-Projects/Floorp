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

const NS_ERROR_MODULE_NETWORK = 2152398848;
const NS_NET_STATUS_READ_FROM = NS_ERROR_MODULE_NETWORK + 8;
const NS_NET_STATUS_WROTE_TO  = NS_ERROR_MODULE_NETWORK + 9;
const kXULNS =
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

#ifndef MOZ_PLACES
// For Places-enabled builds, this is in
// chrome://browser/content/places/controller.js
var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
#endif

const nsCI               = Components.interfaces;
const nsIWebNavigation   = nsCI.nsIWebNavigation;

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
                             .getService(nsCI.nsIBrowserGlue);
var gRDF = null;
var gGlobalHistory = null;
var gURIFixup = null;
var gPageStyleButton = null;
var gCharsetMenu = null;
var gLastBrowserCharset = null;
var gPrevCharset = null;
var gURLBar = null;
var gProxyButton = null;
var gProxyFavIcon = null;
var gProxyDeck = null;
var gNavigatorBundle = null;
var gIsLoadingBlank = false;
var gLastValidURLStr = "";
var gLastValidURL = null;
var gHaveUpdatedToolbarState = false;
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

var gFormFillPrefListener = null;
var gFormHistory = null;
var gFormFillEnabled = true;

var gURLBarAutoFillPrefListener = null;
var gAutoHideTabbarPrefListener = null;

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

#ifndef MOZ_PLACES
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

function UpdateBackForwardButtons()
{
  var backBroadcaster = document.getElementById("Browser:Back");
  var forwardBroadcaster = document.getElementById("Browser:Forward");

  var webNavigation = gBrowser.webNavigation;

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = backBroadcaster.hasAttribute("disabled");
  var forwardDisabled = forwardBroadcaster.hasAttribute("disabled");
  if (backDisabled == webNavigation.canGoBack) {
    if (backDisabled)
      backBroadcaster.removeAttribute("disabled");
    else
      backBroadcaster.setAttribute("disabled", true);
  }

  if (forwardDisabled == webNavigation.canGoForward) {
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
 if (aEvent.button != 0 || aEvent.target.getAttribute("anonid") != "button")
   return;

 // relying on button.xml#menu-button-base internals for improved theme compatibility
 var button = aEvent.target._menubuttonParent;
 if (!button.disabled)
   gClickAndHoldTimer = setTimeout(ClickAndHoldMouseDownCallback, 500, button);
}

function MayStopClickAndHoldTimer(aEvent)
{
  // Note passing null here is a no-op
  clearTimeout(gClickAndHoldTimer);
}

function ClickAndHoldStopEvent(aEvent)
{
  if (aEvent.target._menubuttonParent.open)
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
                          false);  
    aElm.addEventListener("click",
                          ClickAndHoldStopEvent,
                          false);  
  }

  // The click-and-hold area does not include the dropmarkers of the buttons
  var backButton = document.getAnonymousElementByAttribute
    (document.getElementById("back-button"), "anonid", "button");
  if (backButton)
    _addClickAndHoldListenersOnElement(backButton);
  var forwardButton = document.getAnonymousElementByAttribute
    (document.getElementById("forward-button"), "anonid", "button");
  if (forwardButton)
    _addClickAndHoldListenersOnElement(forwardButton);
}
#endif

#ifndef MOZ_PLACES
function UpdateBookmarkAllTabsMenuitem()
{
  var tabbrowser = getBrowser();
  var numTabs = 0;
  if (tabbrowser)
    numTabs = tabbrowser.tabContainer.childNodes.length;

  var bookmarkAllCommand = document.getElementById("Browser:BookmarkAllTabs");
  if (numTabs > 1)
    bookmarkAllCommand.removeAttribute("disabled");
  else
    bookmarkAllCommand.setAttribute("disabled", "true");
}

function addBookmarkMenuitems()
{
  var tabbrowser = getBrowser();
  var tabMenu = document.getAnonymousElementByAttribute(tabbrowser,"anonid","tabContextMenu");
  var bookmarkAllTabsItem = document.createElement("menuitem");
  bookmarkAllTabsItem.setAttribute("label", gNavigatorBundle.getString("bookmarkAllTabs_label"));
  bookmarkAllTabsItem.setAttribute("accesskey", gNavigatorBundle.getString("bookmarkAllTabs_accesskey"));
  bookmarkAllTabsItem.setAttribute("command", "Browser:BookmarkAllTabs");
  // set up the bookmarkAllTabs menu item correctly when the menu popup is shown
  tabMenu.addEventListener("popupshowing", UpdateBookmarkAllTabsMenuitem, false);
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

  addBookmarkAs(tab.linkedBrowser, false);
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

    //Clear undo history of all URL Bars
    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
    var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
    var windows = windowManagerInterface.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      var urlBar = windows.getNext().gURLBar;
      if (urlBar) {
        urlBar.editor.enableUndo(false);
        urlBar.editor.enableUndo(true);
      }
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

    if (gBrowser.selectedBrowser.pageReport) {
      this._reportButton.setAttribute("blocked", "true");
      if (!gPrefService)
        gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefBranch);
      if (gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var bundle_browser = document.getElementById("bundle_browser");
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var message;
        var popupCount = gBrowser.selectedBrowser.pageReport.length;
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
    }
    else
      this._reportButton.removeAttribute("blocked");
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
    var pageReport = gBrowser.selectedBrowser.pageReport;
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
        menuitem.setAttribute("requestingWindowURI", pageReport[i].requestingWindowURI.spec);
        menuitem.setAttribute("popupWindowURI", popupURIspec);
        menuitem.setAttribute("popupWindowFeatures", pageReport[i].popupWindowFeatures);
#ifndef MOZILLA_1_8_BRANCH
# bug 314700
        menuitem.setAttribute("popupWindowName", pageReport[i].popupWindowName);
#endif
        menuitem.setAttribute("oncommand", "gPopupBlockerObserver.showBlockedPopup(event);");
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
    var requestingWindow = aEvent.target.getAttribute("requestingWindowURI");
    var requestingWindowURI =
                      Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService)
                                .newURI(requestingWindow, null, null);

    var popupWindowURI = aEvent.target.getAttribute("popupWindowURI");
    var features = aEvent.target.getAttribute("popupWindowFeatures");
#ifndef MOZILLA_1_8_BRANCH
# bug 314700
    var name = aEvent.target.getAttribute("popupWindowName");
#endif

    var shell = findChildShell(null, gBrowser.selectedBrowser.docShell,
                               requestingWindowURI);
    if (shell) {
      var ifr = shell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      var dwi = ifr.getInterface(Components.interfaces.nsIDOMWindowInternal);
#ifdef MOZILLA_1_8_BRANCH
# bug 314700
      dwi.open(popupWindowURI, "", features);
#else
      dwi.open(popupWindowURI, name, features);
#endif
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
            callback: function() { return xpinstallEditPermissions(browser.docShell); }
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
                   windowTitle    : bundlePreferences.getString("installpermissionstitle"),
                   introText      : bundlePreferences.getString("installpermissionstext") };
    wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
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
    if (window.opener.gFindBar && window.opener.gFindBar.mFindMode == FIND_NORMAL) {
      var openerFindBar = window.opener.document.getElementById("FindToolbar");
      if (openerFindBar && !openerFindBar.hidden)
        gFindBar.openFindBar();
    }

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

function prepareForStartup()
{
  gURLBar = document.getElementById("urlbar");
  gNavigatorBundle = document.getElementById("bundle_browser");
  gProgressMeterPanel = document.getElementById("statusbar-progresspanel");
  gBrowser.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);
  // Note: we need to listen to untrusted events, because the pluginfinder XBL
  // binding can't fire trusted ones (runs with page privileges).
  gBrowser.addEventListener("PluginNotFound", gMissingPluginInstaller.newMissingPlugin, true, true);
  gBrowser.addEventListener("NewTab", BrowserOpenTab, false);

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
  window.QueryInterface(nsCI.nsIInterfaceRequestor)
        .getInterface(nsIWebNavigation)
        .QueryInterface(nsCI.nsIDocShellTreeItem).treeOwner
        .QueryInterface(nsCI.nsIInterfaceRequestor)
        .getInterface(nsCI.nsIXULWindow)
        .XULBrowserWindow = window.XULBrowserWindow;
  window.QueryInterface(nsCI.nsIDOMChromeWindow).browserDOMWindow =
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
                             .getService(Components.interfaces.nsIPrefBranch);
  BrowserOffline.init();
  
  if (gURLBar && document.documentElement.getAttribute("chromehidden").indexOf("toolbar") != -1) {
    gURLBar.setAttribute("readonly", "true");
    gURLBar.setAttribute("enablehistory", "false");
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

  gFindBar.initFindBar();

#ifndef MOZ_PLACES
  // add bookmark options to context menu for tabs
  addBookmarkMenuitems();
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
  var toolbar = document.getElementById("bookmarksBarContent");
  toolbar._init();
  var menu = document.getElementById("bookmarksMenuPopup");
  menu._init();
  PlacesMenuDNDController.init();
  window.controllers.appendController(PlacesController);
#endif

  // called when we go into full screen, even if it is
  // initiated by a web page script
  window.addEventListener("fullscreen", onFullScreen, true);

  var element;
  if (gIsLoadingBlank && gURLBar && !gURLBar.hidden && !gURLBar.parentNode.parentNode.collapsed)
    element = gURLBar;
  else
    element = content;

  // This is a redo of the fix for jag bug 91884
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService(Components.interfaces.nsIWindowWatcher);
  if (window == ww.activeWindow) {
    element.focus();
  } else {
    // set the element in command dispatcher so focus will restore properly
    // when the window does become active
    if (element instanceof Components.interfaces.nsIDOMWindow) {
      document.commandDispatcher.focusedWindow = element;
      document.commandDispatcher.focusedElement = null;
    } else if (element instanceof Components.interfaces.nsIDOMElement) {
      document.commandDispatcher.focusedWindow = element.ownerDocument.defaultView;
      document.commandDispatcher.focusedElement = element;
    }
  }

  SetPageProxyState("invalid");

  var toolbox = document.getElementById("navigator-toolbox");
  toolbox.customizeDone = BrowserToolboxCustomizeDone;

  // Set up Sanitize Item
  gSanitizeListener = new SanitizeListener();

  var pbi = gPrefService.QueryInterface(Components.interfaces.nsIPrefBranchInternal);

  // Enable/Disable Form Fill
  gFormFillPrefListener = new FormFillPrefListener();
  pbi.addObserver(gFormFillPrefListener.domain, gFormFillPrefListener, false);
  gFormFillPrefListener.toggleFormFill();

  // Enable/Disable URL Bar Auto Fill
  gURLBarAutoFillPrefListener = new URLBarAutoFillPrefListener();
  pbi.addObserver(gURLBarAutoFillPrefListener.domain, gURLBarAutoFillPrefListener, false);

  // Enable/Disable auto-hide tabbar
  gAutoHideTabbarPrefListener = new AutoHideTabbarPrefListener();
  pbi.addObserver(gAutoHideTabbarPrefListener.domain, gAutoHideTabbarPrefListener, false);

  pbi.addObserver(gHomeButton.prefDomain, gHomeButton, false);
  gHomeButton.updateTooltip();

  gClickSelectsAll = gPrefService.getBoolPref("browser.urlbar.clickSelectsAll");
  if (gURLBar)
    gURLBar.clickSelectsAll = gClickSelectsAll;

#ifdef HAVE_SHELL_SERVICE
  // Perform default browser checking (after window opens).
  var shell = getShellService();
  if (shell) {
    var shouldCheck = shell.shouldCheckDefaultBrowser;
    if (shouldCheck && !shell.isDefaultBrowser(true)) {
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
                            (IPS.BUTTON_TITLE_YES * IPS.BUTTON_POS_0) +
                            (IPS.BUTTON_TITLE_NO * IPS.BUTTON_POS_1),
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

  // initialize the session-restore service
  var ssEnabled = true;
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  try {
    ssEnabled = prefBranch.getBoolPref("browser.sessionstore.enabled");
  } catch (ex) {}

  if (ssEnabled) {
    var wType = window.document.documentElement.getAttribute("windowtype");
    if (wType == "navigator:browser") {
      try {
        var ss = Cc["@mozilla.org/browser/sessionstore;1"].
                 getService(Ci.nsISessionStore);
        ss.init(window);
      } catch(ex) {
        dump("nsSessionStore could not be initialized: " + ex + "\n");
      }
    }
  }

  // browser-specific tab augmentation
  AugmentTabs.init();
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

#ifndef MOZ_PLACES
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
    var pbi = gPrefService.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.removeObserver(gFormFillPrefListener.domain, gFormFillPrefListener);
    pbi.removeObserver(gURLBarAutoFillPrefListener.domain, gURLBarAutoFillPrefListener);
    pbi.removeObserver(gAutoHideTabbarPrefListener.domain, gAutoHideTabbarPrefListener);
    pbi.removeObserver(gHomeButton.prefDomain, gHomeButton);
  } catch (ex) {
  }

  if (gSanitizeListener)
    gSanitizeListener.shutdown();

  BrowserOffline.uninit();
  
  gFindBar.uninitFindBar();

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
  window.QueryInterface(nsCI.nsIDOMChromeWindow).browserDOMWindow = null;

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
#ifndef MOZ_PLACES
  initServices();
  initBMService();
#else
  var menu = document.getElementById("bookmarksMenuPopup");
  menu._init();
  window.controllers.appendController(PlacesController);
#endif

  // init global pref service
  gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);

  // Set up Sanitize Item
  gSanitizeListener = new SanitizeListener();
}
#endif

function FormFillPrefListener()
{
  gBrowser.attachFormFill();
}

FormFillPrefListener.prototype =
{
  domain: "browser.formfill.enable",
  observe: function (aSubject, aTopic, aPrefName)
  {
    if (aTopic != "nsPref:changed" || aPrefName != this.domain)
      return;

    this.toggleFormFill();
  },

  toggleFormFill: function ()
  {
    try {
      gFormFillEnabled = gPrefService.getBoolPref(this.domain);
    }
    catch (e) {
    }
    var formController = Components.classes["@mozilla.org/satchel/form-fill-controller;1"].getService(Components.interfaces.nsIAutoCompleteInput);
    formController.disableAutoComplete = !gFormFillEnabled;
  }
}

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
  var pbi = gPrefService.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  pbi.addObserver(this.promptDomain, this, false);

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
    var pbi = gPrefService.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.removeObserver(this.promptDomain, this);
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
  if (!event.altKey)
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
  if (newTab != oldTab) {
    oldTab.selected = false;
    gBrowser.selectedTab = newTab;
  }

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

function onGoMenuHidden()
{
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

function updateGoMenu(goMenu)
{
  // In case the timer didn't fire.
  destroyGoMenuItems(goMenu);

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
  if (gURLBar && !gURLBar.parentNode.parentNode.collapsed &&
      !(window.getComputedStyle(gURLBar.parentNode, null).display == "none")) {
    gURLBar.focus();
    gURLBar.select();
  }
#ifdef XP_MACOSX
  else if (window.location.href != getBrowserURL()) {
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
  }
#endif
  else
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

  if (gBrowser.localName == "tabbrowser" && (gBrowser.tabContainer.childNodes.length > 1 ||
      !gPrefService.getBoolPref("browser.tabs.autoHide") && window.toolbar.visible)) {
    // Just close up a tab.
    gBrowser.removeCurrentTab();
    return;
  }

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

  content.focus();
}

function getShortcutOrURI(aURL, aPostDataRef)
{
  // rjc: added support for URL shortcuts (3/30/1999)
  try {
    var shortcutURL = null;
#ifdef MOZ_PLACES
    var bookmarkService = Components.classes["@mozilla.org/browser/nav-bookmarks-service;1"]
                             .getService(nsCI.nsINavBookmarksService);
    var shortcutURI = bookmarkService.getURIForKeyword(aURL);
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
#ifdef MOZ_PLACES
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
#ifndef MOZ_PLACES
          // FIXME: Bug 327328, we don't have last charset in places yet.
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
      var firstSlash = url.indexOf("/");
      if (firstSlash >= 0)
        url = "http://www." + url.substring(0, firstSlash) + suffix +
              url.substring(firstSlash + 1, url.length);
      else
        url = "http://www." + url + suffix;
    }
  }

  gURLBar.value = getShortcutOrURI(url, aPostDataRef);
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
          var uri = makeURI(gURLBar.value);
          const secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                                   .getService(Components.interfaces.nsIScriptSecurityManager);
          const nsIScriptSecMan = Components.interfaces.nsIScriptSecurityManager;
          secMan.checkLoadURI(gBrowser.currentURI, uri, nsIScriptSecMan.DISALLOW_SCRIPT_OR_DATA);
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

function updateToolbarStates(toolbarMenuElt)
{
  if (!gHaveUpdatedToolbarState) {
    var mainWindow = document.getElementById("main-window");
    if (mainWindow.hasAttribute("chromehidden")) {
      gHaveUpdatedToolbarState = true;
      var i;
      for (i = 0; i < toolbarMenuElt.childNodes.length; ++i)
        document.getElementById(toolbarMenuElt.childNodes[i].getAttribute("observes")).removeAttribute("checked");
      var toolbars = document.getElementsByTagName("toolbar");

      // Start i at 1, since we skip the menubar.
      for (i = 1; i < toolbars.length; ++i) {
        if (toolbars[i].getAttribute("class").indexOf("chromeclass") != -1)
          toolbars[i].setAttribute("collapsed", "true");
      }
      var statusbars = document.getElementsByTagName("statusbar");
      for (i = 1; i < statusbars.length; ++i) {
        if (statusbars[i].getAttribute("class").indexOf("chromeclass") != -1)
          statusbars[i].setAttribute("collapsed", "true");
      }
      mainWindow.removeAttribute("chromehidden");
    }
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

    var findBar = document.getElementById("FindToolbar");
    gChromeState.findOpen = !findBar.hidden;
    gFindBar.closeFindBar();
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
      gFindBar.openFindBar();
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

  while (!titleText && !XLinkTitleText && tipElement) {
    if (tipElement.nodeType == Node.ELEMENT_NODE) {
      titleText = tipElement.getAttribute("title");
      XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
    }
    tipElement = tipElement.parentNode;
  }

  var texts = [titleText, XLinkTitleText];
  var tipNode = document.getElementById("aHTMLTooltip");

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
                          (promptService.BUTTON_TITLE_YES * promptService.BUTTON_POS_0) +
                          (promptService.BUTTON_TITLE_NO * promptService.BUTTON_POS_1),
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
    if (url != aXferData.data) {  //do nothing if it's not a valid URL
      var dialogArgs = {
        name: split[1],
        url: url
      }
#ifndef MOZ_PLACES
      openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
                 BROWSER_ADD_BM_FEATURES, dialogArgs);
#else
      dump("*** IMPLEMENT ME");
#endif
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
        var uri = makeURI(url);
        const secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                                 .getService(Components.interfaces.nsIScriptSecurityManager);
        const nsIScriptSecMan = Components.interfaces.nsIScriptSecurityManager;
        secMan.checkLoadURI(gBrowser.currentURI, uri, nsIScriptSecMan.DISALLOW_SCRIPT_OR_DATA);
        loadURI(uri.spec, null, postData.value, true);
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
    var erel = target.rel;
    var etype = target.type;
    var etitle = target.title;
    var ehref = target.href;
    const searchRelRegex = /(^|\s)search($|\s)/i;
    const searchHrefRegex = /^(https?|ftp):\/\//i;

    if (!etype)
      return;
      
    if (etype == "application/opensearchdescription+xml" &&
        searchRelRegex.test(erel) && searchHrefRegex.test(ehref))
    {
      const targetDoc = target.ownerDocument;
      // Set the attribute of the (first) search button.
      var searchButton = document.getAnonymousElementByAttribute(this.getSearchBar(),
                                  "anonid", "search-go-button");
      if (searchButton) {
        var browser = gBrowser.getBrowserForDocument(targetDoc);
         // Append the URI and an appropriate title to the browser data.
        var iconURL = null;
        if (gBrowser.shouldLoadFavIcon(browser.currentURI))
          iconURL = browser.currentURI.prePath + "/favicon.ico";
        var usableTitle = target.title || browser.contentTitle || target.href;
        var hidden = false;
        if (target.title) {
          // If this engine (identified by title) is already in the list, add it
          // to the list of hidden engines rather than to the main list.
          // XXX This will need to be changed when engines are identified by URL;
          // see bug 335102.
          var searchService =
              Components.classes["@mozilla.org/browser/search-service;1"]
                        .getService(Components.interfaces.nsIBrowserSearchService);
          if (searchService.getEngineByName(target.title))
            hidden = true;
        }

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
                       title: usableTitle,
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
                                "anonid", "search-go-button");
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
        win = window.openDialog("chrome://browser/content/", "_blank",
                                "chrome,all,dialog=no", "about:blank");
        win.addEventListener("load", BrowserSearch.webSearch, false);
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
    if (searchBar && !searchBar.parentNode.parentNode.collapsed &&
        window.getComputedStyle(searchBar.parentNode, null).display != "none")
      return searchBar;
    return null;
  },

  loadAddEngines: function BrowserSearch_loadAddEngines() {
    var newWindowPref = gPrefService.getIntPref("browser.link.open_newwindow");
    var where = newWindowPref == 3 ? "tab" : "window";
    var regionBundle = document.getElementById("bundle_browser_region");
    var searchEnginesURL = regionBundle.getString("searchEnginesURL");
    openUILinkIn(searchEnginesURL, where);
  }
}

function FillHistoryMenu(aParent, aMenu, aInsertBefore)
  {
    // Remove old entries if any
    deleteHistoryItems(aParent);

    var webNav = getWebNavigation();
    if (!webNav) {
      // This is always the case for non-browser windows (and the hidden window)
      // on OS X
      return true;
    }
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
        case "history":
          aInsertBefore.hidden = (count == 0);
          end = count > MAX_HISTORY_MENU_ITEMS ? count - MAX_HISTORY_MENU_ITEMS : 0;
          for (j = count - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createRadioMenuItem(aParent,
                                    j,
                                    entry.title,
                                    entry.URI ? entry.URI.spec : null,
                                    j==index,
                                    aInsertBefore);
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

function createRadioMenuItem( aParent, aIndex, aLabel, aStatusText, aChecked, aInsertBefore)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "type", "radio" );
    menuitem.setAttribute( "label", aLabel );
    menuitem.setAttribute( "index", aIndex );
    menuitem.setAttribute( "statustext", aStatusText );
    if (aChecked==true)
      menuitem.setAttribute( "checked", "true" );
    if (aInsertBefore)
      aParent.insertBefore( menuitem, aInsertBefore );
    else
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

  window.openDialog("chrome://global/content/customizeToolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById("navigator-toolbox"));
}

function BrowserToolboxCustomizeDone(aToolboxChanged)
{
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
    gURLBar.value = url;
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

#ifndef MOZ_PLACES
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
#else
  var bookmarksBar = document.getElementById("bookmarksBarContent");
  bookmarksBar._init();
#endif

  // XXX Shouldn't have to do this, but I do
  window.focus();
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

    var i, savedMode, savedIconSize;
    for (i = 0; i < els.length; ++i) {
      // XXX don't interfere with previously collapsed toolbars
      if (els[i].getAttribute("fullscreentoolbar") == "true") {
        if (!aShow) {
          var toolbarMode = els[i].getAttribute("mode");
          var iconSize = els[i].getAttribute("iconsize");
          var contextMenu = els[i].getAttribute("context");

          if (toolbarMode != "text") {
            els[i].setAttribute("saved-mode", toolbarMode);
            els[i].setAttribute("saved-iconsize", iconSize);
            els[i].setAttribute("mode", "icons");
            els[i].setAttribute("iconsize", "small");
          }

          // XXX See bug 202978: we disable the context menu
          // to prevent customization while in fullscreen, which
          // causes menu breakage.
          els[i].setAttribute("saved-context", contextMenu);
          els[i].removeAttribute("context");
        }
        else {
          if (els[i].hasAttribute("saved-mode")) {
            savedMode = els[i].getAttribute("saved-mode");
            els[i].setAttribute("mode", savedMode);
            els[i].removeAttribute("saved-mode");
          }

          if (els[i].hasAttribute("saved-iconsize")) {
            savedIconSize = els[i].getAttribute("saved-iconsize");
            els[i].setAttribute("iconsize", savedIconSize);
            els[i].removeAttribute("saved-iconsize");
          }

          // XXX see above.
          if (els[i].hasAttribute("saved-context")) {
            var savedContext = els[i].getAttribute("saved-context");
            els[i].setAttribute("context", savedContext);
            els[i].removeAttribute("saved-context");
          }
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
#ifndef XP_MACOSX
    var controls = document.getElementsByAttribute("fullscreencontrol", "true");
    for (i = 0; i < controls.length; ++i)
      controls[i].hidden = aShow;
#endif

    // XXXvladimir this was a fix for bug 174174, but I don't think it's necessary
    // any more?
    var toolbox = document.getElementById("navigator-toolbox");
    if (!aShow) {
      var toolboxMode = toolbox.getAttribute("mode");
      var toolboxIconSize = toolbox.getAttribute("iconsize");
      if (toolboxMode != "text") {
        toolbox.setAttribute("saved-mode", toolboxMode);
        toolbox.setAttribute("saved-iconsize", toolboxIconSize);
        toolbox.setAttribute("mode", "icons");
        toolbox.setAttribute("iconsize", "small");
      }
      else {
        if (toolbox.hasAttribute("saved-mode")) {
          savedMode = toolbox.getAttribute("saved-mode");
          toolbox.setAttribute("mode", savedMode);
          toolbox.removeAttribute("saved-mode");
        }

        if (toolbox.hasAttribute("saved-iconsize")) {
          savedIconSize = toolbox.getAttribute("saved-iconsize");
          toolbox.setAttribute("iconsize", savedIconSize);
          toolbox.removeAttribute("saved-iconsize");
        }
      }
    }
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
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsIXULBrowserWindow) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
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

#ifdef MOZ_PLACES
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
#ifndef MOZ_PLACES
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

  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {

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
      var newSpec = aLocation.spec;
      var newIndexOfHash = newSpec.indexOf("#");
      if (newIndexOfHash != -1)
        newSpec = newSpec.substr(0, newSpec.indexOf("#"));
      if (newSpec != oldSpec) {
        gBrowser.getNotificationBox(selectedBrowser).removeAllNotifications(true);
      }
    }
    selectedBrowser.lastURI = aLocation;

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
    var findField = document.getElementById("find-field");
    if (aWebProgress.DOMWindow == content) {

      var location = aLocation.spec;

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

      if (findField)
        setTimeout(function() { findField.value = browser.findString; }, 0, findField, browser);

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
          if (location && gURIFixup)
            try {
              var locationURI = gURIFixup.createExposableURI(aLocation);
              location = locationURI.spec;
            } catch (exception) {}

          if (getBrowser().forceSyncURLBarUpdate) {
            gURLBar.value = ""; // hack for bug 249322
            gURLBar.value = location;
            SetPageProxyState("valid");
          } else {
            setTimeout(function(loc) {
                         gURLBar.value = ""; // hack for bug 249322
                         gURLBar.value = loc;
                         SetPageProxyState("valid");
                       }, 0, location);
          }

          // Setting the urlBar value in some cases causes userTypedValue to
          // become set because of oninput, so reset it to its old value.
          browser.userTypedValue = userTypedValue;
        } else {
          gURLBar.value = userTypedValue;
          SetPageProxyState("invalid");
        }
      }
    }
    UpdateBackForwardButtons();
    if (findField && gFindBar.mFindMode != FIND_NORMAL) {
      // Close the Find toolbar if we're in old-style TAF mode
      gFindBar.closeFindBar();
    }

    //fix bug 253793 - turn off highlight when page changes
    if (document.getElementById("highlight").checked)
      document.getElementById("highlight").removeAttribute("checked");

    var self = this;
    setTimeout(function() { self.asyncUpdateUI(); }, 0);
  },
  
  asyncUpdateUI : function () {
    FeedHandler.updateFeeds();
    BrowserSearch.updateSearchButton();
#ifdef ALTSS_ICON
    updatePageStyles();
#endif
#ifdef MOZ_PLACES
    PlacesCommandHook.updateTagButton();
#endif
  },

  onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
  {
    this.status = aMessage;
    this.updateStatusField();
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
    setTimeout(function() { if (document.getElementById("highlight").checked) toggleHighlight(true); }, 0);
  }
}

function nsBrowserAccess()
{
}

nsBrowserAccess.prototype =
{
  QueryInterface : function(aIID)
  {
    if (aIID.equals(nsCI.nsIBrowserDOMWindow) ||
        aIID.equals(nsCI.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  openURI : function(aURI, aOpener, aWhere, aContext)
  {
    var newWindow = null;
    var referrer = null;
    var isExternal = (aContext == nsCI.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      dump("use -chrome command-line option to load external chrome urls\n");
      return null;
    }

    var loadflags = isExternal ?
                       nsCI.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                       nsCI.nsIWebNavigation.LOAD_FLAGS_NONE;
    var location;
    if (aWhere == nsCI.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      switch (aContext) {
        case nsCI.nsIBrowserDOMWindow.OPEN_EXTERNAL :
          aWhere = gPrefService.getIntPref("browser.link.open_external");
          break;
        default : // OPEN_NEW or an illegal value
          aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
      }
    }
    var url = aURI ? aURI.spec : "about:blank";
    switch(aWhere) {
      case nsCI.nsIBrowserDOMWindow.OPEN_NEWWINDOW :
        newWindow = openDialog(getBrowserURL(), "_blank", "all,dialog=no", url);
        break;
      case nsCI.nsIBrowserDOMWindow.OPEN_NEWTAB :
        var loadInBackground = gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground");
        var newTab = gBrowser.loadOneTab("about:blank", null, null, null, loadInBackground, false);
        newWindow = gBrowser.getBrowserForTab(newTab).docShell
                            .QueryInterface(nsCI.nsIInterfaceRequestor)
                            .getInterface(nsCI.nsIDOMWindow);
        try {
          if (aOpener) {
            location = aOpener.location;
            referrer =
                    Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService)
                              .newURI(location, null, null);
          }
          newWindow.QueryInterface(nsCI.nsIInterfaceRequestor)
                   .getInterface(nsCI.nsIWebNavigation)
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

            newWindow.QueryInterface(nsCI.nsIInterfaceRequestor)
                     .getInterface(nsIWebNavigation)
                     .loadURI(url, loadflags, referrer, null, null);
          } else {
            newWindow = gBrowser.selectedBrowser.docShell
                                .QueryInterface(nsCI.nsIInterfaceRequestor)
                                .getInterface(nsCI.nsIDOMWindow);
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

// |forceOpen| is a bool that indicates that the sidebar should be forced open.  In other words
// the toggle won't be allowed to close the sidebar.
function toggleSidebar(aCommandID, forceOpen) {

  var sidebarBox = document.getElementById("sidebar-box");
  if (!aCommandID)
    aCommandID = sidebarBox.getAttribute("sidebarcommand");

  var elt = document.getElementById(aCommandID);
  var sidebar = document.getElementById("sidebar");
  var sidebarTitle = document.getElementById("sidebar-title");
  var sidebarSplitter = document.getElementById("sidebar-splitter");

  if (!forceOpen && elt.getAttribute("checked") == "true") {
    elt.removeAttribute("checked");
    sidebarBox.setAttribute("sidebarcommand", "");
    sidebarTitle.setAttribute("value", "");
    sidebarBox.hidden = true;
    sidebarSplitter.hidden = true;
    content.focus();
    return;
  }

  var elts = document.getElementsByAttribute("group", "sidebar");
  for (var i = 0; i < elts.length; ++i)
    elts[i].removeAttribute("checked");

  elt.setAttribute("checked", "true");;

  if (sidebarBox.hidden) {
    sidebarBox.hidden = false;
    sidebarSplitter.hidden = false;
  }

  var url = elt.getAttribute("sidebarurl");
  var title = elt.getAttribute("sidebartitle");
  if (!title)
    title = elt.getAttribute("label");
  sidebar.setAttribute("src", url);
  sidebarBox.setAttribute("src", url);
  sidebarBox.setAttribute("sidebarcommand", elt.id);
  sidebarTitle.setAttribute("value", title);
  if (aCommandID != "viewWebPanelsSidebar") { // no searchbox there
    // if the sidebar we want is already constructed, focus the searchbox
    if ((aCommandID == "viewBookmarksSidebar" && sidebar.contentDocument.getElementById("bookmarksPanel"))
    || (aCommandID == "viewHistorySidebar" && sidebar.contentDocument.getElementById("history-panel")))
      sidebar.contentDocument.getElementById("search-box").focus();
    // otherwiese, attach an onload handler
    else
      sidebar.addEventListener("load", asyncFocusSearchBox, true);
  }
}

function asyncFocusSearchBox(event)
{
  var sidebar = document.getElementById("sidebar");
  var searchBox = sidebar.contentDocument.getElementById("search-box");
  if (searchBox)
    searchBox.focus();
  sidebar.removeEventListener("load", asyncFocusSearchBox, true);
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
      var navigatorRegionBundle = document.getElementById("bundle_browser_region");
      url = navigatorRegionBundle.getString("homePageDefault");
    }

    return url;
  }
};

function nsContextMenu( xulMenu ) {
    this.target            = null;
    this.menu              = null;
    this.onTextInput       = false;
    this.onKeywordField    = false;
    this.onImage           = false;
    this.onLoadedImage     = false;
    this.onLink            = false;
    this.onMailtoLink      = false;
    this.onSaveableLink    = false;
    this.onMetaDataItem    = false;
    this.onMathML          = false;
    this.link              = false;
    this.linkURL           = "";
    this.linkURI           = null;
    this.linkProtocol      = null;
    this.inFrame           = false;
    this.hasBGImage        = false;
    this.isTextSelected    = false;
    this.isContentSelected = false;
    this.inDirList         = false;
    this.shouldDisplay     = true;
    this.isDesignMode      = false;

    // Initialize new menu.
    this.initMenu( xulMenu );
}

// Prototype for nsContextMenu "class."
nsContextMenu.prototype = {
    // onDestroy is a no-op at this point.
    onDestroy : function () {
    },
    // Initialize context menu.
    initMenu : function ( popup ) {
        // Save menu.
        this.menu = popup;

        // Get contextual info.
        this.setTarget( document.popupNode, document.popupRangeParent,
                        document.popupRangeOffset );

        this.isTextSelected = this.isTextSelection();
        this.isContentSelected = this.isContentSelection();

        // Initialize (disable/remove) menu items.
        this.initItems();
    },
    initItems : function () {
        this.initOpenItems();
        this.initNavigationItems();
        this.initViewItems();
        this.initMiscItems();
        this.initSpellingItems();
        this.initSaveItems();
        this.initClipboardItems();
        this.initMetadataItems();
    },
    initOpenItems : function () {
        this.showItem( "context-openlink", this.onSaveableLink || ( this.inDirList && this.onLink ) );
        this.showItem( "context-openlinkintab", this.onSaveableLink || ( this.inDirList && this.onLink ) );

        this.showItem( "context-sep-open", this.onSaveableLink || ( this.inDirList && this.onLink ) );
    },
    initNavigationItems : function () {
        // Back determined by canGoBack broadcaster.
        this.setItemAttrFromNode( "context-back", "disabled", "canGoBack" );

        // Forward determined by canGoForward broadcaster.
        this.setItemAttrFromNode( "context-forward", "disabled", "canGoForward" );

        this.showItem( "context-back", !( this.isContentSelected || this.onLink || this.onImage || this.onTextInput ) );
        this.showItem( "context-forward", !( this.isContentSelected || this.onLink || this.onImage || this.onTextInput ) );

        this.showItem( "context-reload", !( this.isContentSelected || this.onLink || this.onImage || this.onTextInput ) );

        this.showItem( "context-stop", !( this.isContentSelected || this.onLink || this.onImage || this.onTextInput ) );
        this.showItem( "context-sep-stop", !( this.isContentSelected || this.onLink || this.onTextInput || this.onImage ) );

        // XXX: Stop is determined in navigator.js; the canStop broadcaster is broken
        //this.setItemAttrFromNode( "context-stop", "disabled", "canStop" );
    },
    initSaveItems : function () {
        this.showItem( "context-savepage", !( this.inDirList || this.isContentSelected || this.onTextInput || this.onLink || this.onImage ));
        this.showItem( "context-sendpage", !( this.inDirList || this.isContentSelected || this.onTextInput || this.onLink || this.onImage ));

        // Save+Send link depends on whether we're in a link.
        this.showItem( "context-savelink", this.onSaveableLink );
        this.showItem( "context-sendlink", this.onSaveableLink );

        // Save+Send image depends on whether we're on an image.
        this.showItem( "context-saveimage", this.onLoadedImage );
        this.showItem( "context-sendimage", this.onImage );
    },
    initViewItems : function () {
        // View source is always OK, unless in directory listing.
        this.showItem( "context-viewpartialsource-selection", this.isContentSelected );
        this.showItem( "context-viewpartialsource-mathml", this.onMathML && !this.isContentSelected );
        this.showItem( "context-viewsource", !( this.inDirList || this.onImage || this.isContentSelected || this.onLink || this.onTextInput ) );
        this.showItem( "context-viewinfo", !( this.inDirList || this.onImage || this.isContentSelected || this.onLink || this.onTextInput ) );

        this.showItem( "context-sep-properties", !( this.inDirList || this.isContentSelected || this.onTextInput ) );
        // Set as Desktop background depends on whether an image was clicked on,
        // and only works if we have a shell service.
        var haveSetDesktopBackground = false;
#ifdef HAVE_SHELL_SERVICE
        // Only enable Set as Desktop Background if we can get the shell service.
        var shell = getShellService();
        if (shell)
          haveSetDesktopBackground = true;
#endif
        this.showItem( "context-setDesktopBackground", haveSetDesktopBackground && this.onLoadedImage );

        if ( haveSetDesktopBackground && this.onLoadedImage )
            this.setItemAttr( "context-setDesktopBackground", "disabled", this.disableSetDesktopBackground());

        // View Image depends on whether an image was clicked on.
        this.showItem( "context-viewimage", this.onImage  && ( !this.onStandaloneImage || this.inFrame ) );

        // View background image depends on whether there is one.
        this.showItem( "context-viewbgimage", !( this.inDirList || this.onImage || this.isContentSelected || this.onLink || this.onTextInput ) );
        this.showItem( "context-sep-viewbgimage", !( this.inDirList || this.onImage || this.isContentSelected || this.onLink || this.onTextInput ) );
        this.setItemAttr( "context-viewbgimage", "disabled", this.hasBGImage ? null : "true");
    },
    initMiscItems : function () {
        // Use "Bookmark This Link" if on a link.
        this.showItem( "context-bookmarkpage", !( this.isContentSelected || this.onTextInput || this.onLink || this.onImage ) );
        this.showItem( "context-bookmarklink", this.onLink && !this.onMailtoLink );
        this.showItem( "context-searchselect", this.isTextSelected );
        this.showItem( "context-keywordfield", this.onTextInput && this.onKeywordField );
        this.showItem( "frame", this.inFrame );
        this.showItem( "frame-sep", this.inFrame );

        // BiDi UI
        this.showItem( "context-sep-bidi", gBidiUI);
        this.showItem( "context-bidi-text-direction-toggle", this.onTextInput && gBidiUI);
        this.showItem( "context-bidi-page-direction-toggle", !this.onTextInput && gBidiUI);

        if (this.onImage) {
          var blockImage = document.getElementById("context-blockimage");

          var uri = this.target.QueryInterface(Components.interfaces.nsIImageLoadingContent).currentURI;

          var hostLabel;
          // this throws if the image URI doesn't have a host (eg, data: image URIs)
          // see bug 293758 for details
          try {
            hostLabel = uri.host;
          } catch (ex) { }

          if (hostLabel) {
            var shortenedUriHost = hostLabel.replace(/^www\./i,"");
            if (shortenedUriHost.length > 15)
              shortenedUriHost = shortenedUriHost.substr(0,15) + "...";
            blockImage.label = gNavigatorBundle.getFormattedString("blockImages", [shortenedUriHost]);

            if (this.isImageBlocked())
              blockImage.setAttribute("checked", "true");
            else
              blockImage.removeAttribute("checked");
          }
        }

        // Only show the block image item if the image can be blocked
        this.showItem( "context-blockimage", this.onImage && hostLabel);
    },
    initSpellingItems : function () {
        var canSpell = InlineSpellCheckerUI.canSpellCheck;
        var onMisspelling = InlineSpellCheckerUI.overMisspelling;
        this.showItem("spell-check-enabled", canSpell);
        this.showItem("spell-separator", canSpell);
        if (canSpell)
            document.getElementById("spell-check-enabled").setAttribute("checked",
                                                                        InlineSpellCheckerUI.enabled);
        this.showItem("spell-add-to-dictionary", onMisspelling);

        // suggestion list
        this.showItem("spell-suggestions-separator", onMisspelling);
        if (onMisspelling) {
            var menu = document.getElementById("contentAreaContextMenu");
            var suggestionsSeparator = document.getElementById("spell-suggestions-separator");
            var numsug = InlineSpellCheckerUI.addSuggestionsToMenu(menu, suggestionsSeparator, 5);
            this.showItem("spell-no-suggestions", numsug == 0);
        } else {
            this.showItem("spell-no-suggestions", false);
        }

        // dictionary list
        this.showItem("spell-dictionaries", InlineSpellCheckerUI.enabled);
        if (canSpell) {
            var dictMenu = document.getElementById("spell-dictionaries-menu");
            var dictSep = document.getElementById("spell-language-separator");
            InlineSpellCheckerUI.addDictionaryListToMenu(dictMenu, dictSep);
        }
    },
    initClipboardItems : function () {

        // Copy depends on whether there is selected text.
        // Enabling this context menu item is now done through the global
        // command updating system
        // this.setItemAttr( "context-copy", "disabled", !this.isTextSelected() );

        goUpdateGlobalEditMenuItems();

        this.showItem( "context-undo", this.onTextInput );
        this.showItem( "context-sep-undo", this.onTextInput );
        this.showItem( "context-cut", this.onTextInput );
        this.showItem( "context-copy", this.isContentSelected || this.onTextInput );
        this.showItem( "context-paste", this.onTextInput );
        this.showItem( "context-delete", this.onTextInput );
        this.showItem( "context-sep-paste", this.onTextInput );
        this.showItem( "context-selectall", !( this.onLink || this.onImage ) || this.isDesignMode );
        this.showItem( "context-sep-selectall", this.isContentSelected );

        // XXX dr
        // ------
        // nsDocumentViewer.cpp has code to determine whether we're
        // on a link or an image. we really ought to be using that...

        // Copy email link depends on whether we're on an email link.
        this.showItem( "context-copyemail", this.onMailtoLink );

        // Copy link location depends on whether we're on a link.
        this.showItem( "context-copylink", this.onLink );
        this.showItem( "context-sep-copylink", this.onLink && this.onImage);

#ifdef CONTEXT_COPY_IMAGE_CONTENTS
        // Copy image contents depends on whether we're on an image.
        this.showItem( "context-copyimage-contents", this.onImage );
#endif
        // Copy image location depends on whether we're on an image.
        this.showItem( "context-copyimage", this.onImage );
        this.showItem( "context-sep-copyimage", this.onImage );
    },
    initMetadataItems : function () {
        // Show if user clicked on something which has metadata.
        this.showItem( "context-metadata", this.onMetaDataItem );
    },
    // Set various context menu attributes based on the state of the world.
    setTarget : function ( node, rangeParent, rangeOffset ) {
        const xulNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
        if ( node.namespaceURI == xulNS ) {
          this.shouldDisplay = false;
          return;
        }

        // Initialize contextual info.
        this.onImage           = false;
        this.onLoadedImage     = false;
        this.onStandaloneImage = false;
        this.onMetaDataItem    = false;
        this.onTextInput       = false;
        this.onKeywordField    = false;
        this.imageURL          = "";
        this.onLink            = false;
        this.linkURL           = "";
        this.linkURI           = null;
        this.linkProtocol      = "";
        this.onMathML          = false;
        this.inFrame           = false;
        this.hasBGImage        = false;
        this.bgImageURL        = "";

        // Clear any old spellchecking items from the menu, this used to
        // be in the menu hiding code but wasn't getting called in all
        // situations. Here, we can ensure it gets cleaned up any time the
        // menu is shown. Note: must be before uninit because that clears the
        // internal vars
        InlineSpellCheckerUI.clearSuggestionsFromMenu();
        InlineSpellCheckerUI.clearDictionaryListFromMenu();

        InlineSpellCheckerUI.uninit();

        // Remember the node that was clicked.
        this.target = node;
        
        // Remember the URL of the document containing the node
        // for referrer header and for security checks.
        this.docURL = node.ownerDocument.location.href;

        // First, do checks for nodes that never have children.
        if ( this.target.nodeType == Node.ELEMENT_NODE ) {
            // See if the user clicked on an image.
            if ( this.target instanceof Components.interfaces.nsIImageLoadingContent && this.target.currentURI  ) {
                this.onImage = true;
                this.onMetaDataItem = true;
                        
                var request = this.target.getRequest( Components.interfaces.nsIImageLoadingContent.CURRENT_REQUEST );
                if (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE))
                    this.onLoadedImage = true;
                this.imageURL = this.target.currentURI.spec;

                if ( this.target.ownerDocument instanceof ImageDocument)
                   this.onStandaloneImage = true;
            } else if ( this.target instanceof HTMLInputElement ) {
               this.onTextInput = this.isTargetATextBox(this.target);
               // allow spellchecking UI on all writable text boxes except passwords
               if (this.onTextInput && ! this.target.readOnly && this.target.type != "password") {
                   InlineSpellCheckerUI.init(this.target.QueryInterface(Components.interfaces.nsIDOMNSEditableElement).editor);
                   InlineSpellCheckerUI.initFromEvent(rangeParent, rangeOffset);
               }
               this.onKeywordField = this.isTargetAKeywordField(this.target);
            } else if ( this.target instanceof HTMLTextAreaElement ) {
                 this.onTextInput = true;
                 if (! this.target.readOnly) {
                     InlineSpellCheckerUI.init(this.target.QueryInterface(Components.interfaces.nsIDOMNSEditableElement).editor);
                     InlineSpellCheckerUI.initFromEvent(rangeParent, rangeOffset);
                 }
            } else if ( this.target instanceof HTMLHtmlElement ) {
               // pages with multiple <body>s are lame. we'll teach them a lesson.
               var bodyElt = this.target.ownerDocument.getElementsByTagName("body")[0];
               if ( bodyElt ) {
                 var computedURL = this.getComputedURL( bodyElt, "background-image" );
                 if ( computedURL ) {
                   this.hasBGImage = true;
                   this.bgImageURL = makeURLAbsolute( bodyElt.baseURI,
                                                      computedURL );
                 }
               }
            } else if ( "HTTPIndex" in content &&
                        content.HTTPIndex instanceof Components.interfaces.nsIHTTPIndex ) {
                this.inDirList = true;
                // Bubble outward till we get to an element with URL attribute
                // (which should be the href).
                var root = this.target;
                while ( root && !this.link ) {
                    if ( root.tagName == "tree" ) {
                        // Hit root of tree; must have clicked in empty space;
                        // thus, no link.
                        break;
                    }
                    if ( root.getAttribute( "URL" ) ) {
                        // Build pseudo link object so link-related functions work.
                        this.onLink = true;
                        this.link = { href : root.getAttribute("URL"),
                                      getAttribute: function (attr) {
                                          if (attr == "title") {
                                              return root.firstChild.firstChild.getAttribute("label");
                                          } else {
                                              return "";
                                          }
                                      }
                                    };
                        // If element is a directory, then you can't save it.
                        if ( root.getAttribute( "container" ) == "true" ) {
                            this.onSaveableLink = false;
                        } else {
                            this.onSaveableLink = true;
                        }
                    } else {
                        root = root.parentNode;
                    }
                }
            }
        }

        // Second, bubble out, looking for items of interest that can have childen.
        // Always pick the innermost link, background image, etc.
        
        const XMLNS = "http://www.w3.org/XML/1998/namespace";
        var elem = this.target;
        while ( elem ) {
            if ( elem.nodeType == Node.ELEMENT_NODE ) {
            
                // Link?
                if ( !this.onLink &&
                     ( (elem instanceof HTMLAnchorElement && elem.href) ||
                        elem instanceof HTMLAreaElement ||
                        elem instanceof HTMLLinkElement ||
                        elem.getAttributeNS( "http://www.w3.org/1999/xlink", "type") == "simple" ) ) {
                    
                    // Target is a link or a descendant of a link.
                    this.onLink = true;
                    this.onMetaDataItem = true;

                    // xxxmpc: this is kind of a hack to work around a Gecko bug (see bug 266932)
                    // we're going to walk up the DOM looking for a parent link node,
                    // this shouldn't be necessary, but we're matching the existing behaviour for left click
                    var realLink = elem;
                    var parent = elem.parentNode;
                    while (parent) {
                      try {
                        if ( (parent instanceof HTMLAnchorElement && elem.href) ||
                             parent instanceof HTMLAreaElement ||
                             parent instanceof HTMLLinkElement ||
                             parent.getAttributeNS( "http://www.w3.org/1999/xlink", "type") == "simple")
                          realLink = parent;
                      } catch (e) {}
                      parent = parent.parentNode;
                    }
                    
                    // Remember corresponding element.
                    this.link = realLink;
                    this.linkURL = this.getLinkURL();
                    this.linkURI = this.getLinkURI();
                    this.linkProtocol = this.getLinkProtocol();
                    this.onMailtoLink = (this.linkProtocol == "mailto");
                    this.onSaveableLink = this.isLinkSaveable( this.link );
                }

                // Metadata item?
                if ( !this.onMetaDataItem ) {
                    // We display metadata on anything which fits
                    // the below test, as well as for links and images
                    // (which set this.onMetaDataItem to true elsewhere)
                    if ( ( elem instanceof HTMLQuoteElement && elem.cite)    ||
                         ( elem instanceof HTMLTableElement && elem.summary) ||
                         ( elem instanceof HTMLModElement &&
                             ( elem.cite || elem.dateTime ) )                ||
                         ( elem instanceof HTMLElement &&
                             ( elem.title || elem.lang ) )                   ||
                         elem.getAttributeNS(XMLNS, "lang") ) {
                        this.onMetaDataItem = true;
                    }
                }

                // Background image?  Don't bother if we've already found a
                // background image further down the hierarchy.  Otherwise,
                // we look for the computed background-image style.
                if ( !this.hasBGImage ) {
                    var bgImgUrl = this.getComputedURL( elem, "background-image" );
                    if ( bgImgUrl ) {
                        this.hasBGImage = true;
                        this.bgImageURL = makeURLAbsolute( elem.baseURI,
                                                           bgImgUrl );
                    }
                }
            }
            elem = elem.parentNode;
        }
        
        // See if the user clicked on MathML
        const NS_MathML = "http://www.w3.org/1998/Math/MathML";
        if ((this.target.nodeType == Node.TEXT_NODE &&
             this.target.parentNode.namespaceURI == NS_MathML)
             || (this.target.namespaceURI == NS_MathML))
          this.onMathML = true;

        // See if the user clicked in a frame.
        if ( this.target.ownerDocument != window.content.document ) {
            this.inFrame = true;
        }

        // if the document is editable, show context menu like in text inputs
        var win = this.target.ownerDocument.defaultView;
        if (win) {
          var editingSession = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                                  .getInterface(Components.interfaces.nsIWebNavigation)
                                  .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                                  .getInterface(Components.interfaces.nsIEditingSession);
          if (editingSession.windowIsEditable(win)) {
            this.onTextInput       = true;
            this.onKeywordField    = false;
            this.onImage           = false;
            this.onLoadedImage     = false;
            this.onMetaDataItem    = false;
            this.onMathML          = false;
            this.inFrame           = false;
            this.hasBGImage        = false;
            this.isDesignMode      = true;
            InlineSpellCheckerUI.init(editingSession.getEditorForWindow(win));
            var canSpell = InlineSpellCheckerUI.canSpellCheck;
            InlineSpellCheckerUI.initFromEvent(rangeParent, rangeOffset);
            this.showItem("spell-check-enabled", canSpell);
            this.showItem("spell-separator", canSpell);
          }
        }
    },
    // Returns the computed style attribute for the given element.
    getComputedStyle: function( elem, prop ) {
         return elem.ownerDocument.defaultView.getComputedStyle( elem, '' ).getPropertyValue( prop );
    },
    // Returns a "url"-type computed style attribute value, with the url() stripped.
    getComputedURL: function( elem, prop ) {
         var url = elem.ownerDocument.defaultView.getComputedStyle( elem, '' ).getPropertyCSSValue( prop );
         return ( url.primitiveType == CSSPrimitiveValue.CSS_URI ) ? url.getStringValue() : null;
    },
    // Returns true if clicked-on link targets a resource that can be saved.
    isLinkSaveable : function ( link ) {
        // We don't do the Right Thing for news/snews yet, so turn them off
        // until we do.
        return this.linkProtocol && !(
                 this.linkProtocol == "mailto"     ||
                 this.linkProtocol == "javascript" ||
                 this.linkProtocol == "news"       ||
                 this.linkProtocol == "snews"      );
    },

    // Open linked-to URL in a new window.
    openLink : function () {
        openNewWindowWith(this.linkURL, this.docURL, null, false);
    },
    // Open linked-to URL in a new tab.
    openLinkInTab : function () {
        openNewTabWith(this.linkURL, this.docURL, null, null, false);
    },
    // Open frame in a new tab.
    openFrameInTab : function () {
        openNewTabWith(this.target.ownerDocument.location.href, null, null, null, false);
    },
    // Reload clicked-in frame.
    reloadFrame : function () {
        this.target.ownerDocument.location.reload();
    },
    // Open clicked-in frame in its own window.
    openFrame : function () {
        openNewWindowWith(this.target.ownerDocument.location.href, null, null, false);
    },
    // Open clicked-in frame in the same window.
    showOnlyThisFrame : function () {
        try {
          const secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(Components.interfaces.nsIScriptSecurityManager);
          const nsIScriptSecMan = Components.interfaces.nsIScriptSecurityManager;
          secMan.checkLoadURI(gBrowser.currentURI, makeURI(this.target.ownerDocument.location.href),
                              nsIScriptSecMan.DISALLOW_SCRIPT);
          window.loadURI(this.target.ownerDocument.location.href, null, null, false);
        } catch(e) {}
    },
    // View Partial Source
    viewPartialSource : function ( context ) {
        var focusedWindow = document.commandDispatcher.focusedWindow;
        if (focusedWindow == window)
          focusedWindow = content;
        var docCharset = null;
        if (focusedWindow)
          docCharset = "charset=" + focusedWindow.document.characterSet;

        // "View Selection Source" and others such as "View MathML Source"
        // are mutually exclusive, with the precedence given to the selection
        // when there is one
        var reference = null;
        if (context == "selection")
          reference = focusedWindow.getSelection();
        else if (context == "mathml")
          reference = this.target;
        else
          throw "not reached";

        var docUrl = null; // unused (and play nice for fragments generated via XSLT too)
        window.openDialog("chrome://global/content/viewPartialSource.xul",
                          "_blank", "scrollbars,resizable,chrome,dialog=no",
                          docUrl, docCharset, reference, context);
    },
    // Open new "view source" window with the frame's URL.
    viewFrameSource : function () {
        BrowserViewSourceOfDocument(this.target.ownerDocument);
    },
    viewInfo : function () {
      BrowserPageInfo();
    },
    viewFrameInfo : function () {
      BrowserPageInfo(this.target.ownerDocument);
    },
    // Change current window to the URL of the image.
    viewImage : function (e) {
        urlSecurityCheck( this.imageURL, this.docURL );
        try {
          if (this.docURL != gBrowser.currentURI) {
            const secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                           .getService(Components.interfaces.nsIScriptSecurityManager);
            const nsIScriptSecMan = Components.interfaces.nsIScriptSecurityManager;
            secMan.checkLoadURI(gBrowser.currentURI, makeURI(this.imageURL),
                                nsIScriptSecMan.DISALLOW_SCRIPT);
          }
          openUILink( this.imageURL, e );
        } catch(e) {}
    },
    // Change current window to the URL of the background image.
    viewBGImage : function (e) {
        urlSecurityCheck( this.bgImageURL, this.docURL );
        try {
          if (this.docURL != gBrowser.currentURI) {
            const secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                           .getService(Components.interfaces.nsIScriptSecurityManager);
            const nsIScriptSecMan = Components.interfaces.nsIScriptSecurityManager;
            secMan.checkLoadURI(gBrowser.currentURI, makeURI(this.bgImageURL),
                                nsIScriptSecMan.DISALLOW_SCRIPT);
          }
          openUILink( this.bgImageURL, e );
        } catch(e) {}
    },
    disableSetDesktopBackground: function() {
        // Disable the Set as Desktop Background menu item if we're still trying
        // to load the image or the load failed.

        const nsIImageLoadingContent = Components.interfaces.nsIImageLoadingContent;
        if (!(this.target instanceof nsIImageLoadingContent))
            return true;

        if (("complete" in this.target) && !this.target.complete)
            return true;

        if (this.target.currentURI.schemeIs("javascript"))
            return true;

        var request = this.target.QueryInterface(nsIImageLoadingContent)
                                 .getRequest(nsIImageLoadingContent.CURRENT_REQUEST);
        if (!request)
            return true;

        return false;
    },
    setDesktopBackground: function() {
      // Paranoia: check disableSetDesktopBackground again, in case the
      // image changed since the context menu was initiated.
      if (this.disableSetDesktopBackground())
        return;

      urlSecurityCheck(this.target.currentURI.spec, this.docURL);

      // Confirm since it's annoying if you hit this accidentally.
      const kDesktopBackgroundURL = 
                    "chrome://browser/content/setDesktopBackground.xul";
#ifdef XP_MACOSX
      // On Mac, the Set Desktop Background window is not modal.
      // Don't open more than one Set Desktop Background window.
      var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                         .getService(Components.interfaces.nsIWindowMediator);
      var dbWin = wm.getMostRecentWindow("Shell:SetDesktopBackground");
      if (dbWin) {
        dbWin.gSetBackground.init(this.target);
        dbWin.focus();
      }
      else {
        openDialog(kDesktopBackgroundURL, "",
                   "centerscreen,chrome,dialog=no,dependent,resizable=no",
                   this.target);
      }
#else
      // On non-Mac platforms, the Set Wallpaper dialog is modal.
      openDialog(kDesktopBackgroundURL, "",
                 "centerscreen,chrome,dialog,modal,dependent",
                 this.target);
#endif
    },
    // Save URL of clicked-on frame.
    saveFrame : function () {
        saveDocument( this.target.ownerDocument );
    },
    // Save URL of clicked-on link.
    saveLink : function () {
        urlSecurityCheck(this.linkURL, this.docURL);
        saveURL( this.linkURL, this.linkText(), null, true, false,
                 makeURI(this.docURL, this.target.ownerDocument.characterSet) );
    },
    sendLink : function () {
        MailIntegration.sendMessage( this.linkURL, "" ); // we don't know the title of the link so pass in an empty string
    },
    // Save URL of clicked-on image.
    saveImage : function () {
        urlSecurityCheck(this.imageURL, this.docURL);
        saveImageURL( this.imageURL, null, "SaveImageTitle", false,
                      false, makeURI(this.docURL) );
    },
    sendImage : function () {
        MailIntegration.sendMessage(this.imageURL, "");
    },
    toggleImageBlocking : function (aBlock) {
      var nsIPermissionManager = Components.interfaces.nsIPermissionManager;
      var permissionmanager =
        Components.classes["@mozilla.org/permissionmanager;1"]
                  .getService(nsIPermissionManager);

      var uri = this.target.QueryInterface(Components.interfaces.nsIImageLoadingContent).currentURI;

      permissionmanager.add(uri, "image",
                            aBlock ? nsIPermissionManager.DENY_ACTION : nsIPermissionManager.ALLOW_ACTION);
    },
    isImageBlocked : function() {
      var nsIPermissionManager = Components.interfaces.nsIPermissionManager;
      var permissionmanager =
        Components.classes["@mozilla.org/permissionmanager;1"]
          .getService(Components.interfaces.nsIPermissionManager);

      var uri = this.target.QueryInterface(Components.interfaces.nsIImageLoadingContent).currentURI;

      return permissionmanager.testPermission(uri, "image") == nsIPermissionManager.DENY_ACTION;
    },
    // Generate email address and put it on clipboard.
    copyEmail : function () {
        // Copy the comma-separated list of email addresses only.
        // There are other ways of embedding email addresses in a mailto:
        // link, but such complex parsing is beyond us.
        var url = this.linkURL;
        var qmark = url.indexOf( "?" );
        var addresses;

        if ( qmark > 7 ) {                   // 7 == length of "mailto:"
            addresses = url.substring( 7, qmark );
        } else {
            addresses = url.substr( 7 );
        }

        // Let's try to unescape it using a character set
        // in case the address is not ASCII.
        try {
          var characterSet = this.target.ownerDocument.characterSet;
          const textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                         .getService(Components.interfaces.nsITextToSubURI);
          addresses = textToSubURI.unEscapeURIForUI(characterSet, addresses);
        }
        catch(ex) {
          // Do nothing.
        }

        var clipboard = this.getService( "@mozilla.org/widget/clipboardhelper;1",
                                         Components.interfaces.nsIClipboardHelper );
        clipboard.copyString(addresses);
    },
    addBookmark : function() {
      var docshell = document.getElementById( "content" ).webNavigation;
#ifndef MOZ_PLACES
      BookmarksUtils.addBookmark( docshell.currentURI.spec,
                                  docshell.document.title,
                                  docshell.document.charset,
                                  BookmarksUtils.getDescriptionFromDocument(docshell.document));
#else
      dump("*** IMPLEMENT ME\n");
#endif
    },
    addBookmarkForFrame : function() {
#ifndef MOZ_PLACES
      var doc = this.target.ownerDocument;
      var uri = doc.location.href;
      var title = doc.title;
      var description = BookmarksUtils.getDescriptionFromDocument(doc);
      if ( !title )
        title = uri;
      BookmarksUtils.addBookmark(uri, title, doc.charset, description);
#else
      dump("*** IMPLEMENT ME\n");
#endif
    },
    // Open Metadata window for node
    showMetadata : function () {
        window.openDialog(  "chrome://browser/content/metaData.xul",
                            "_blank",
                            "scrollbars,resizable,chrome,dialog=no",
                            this.target);
    },

    ///////////////
    // Utilities //
    ///////////////

    // Create instance of component given contractId and iid (as string).
    createInstance : function ( contractId, iidName ) {
        var iid = Components.interfaces[ iidName ];
        return Components.classes[ contractId ].createInstance( iid );
    },
    // Get service given contractId and iid (as string).
    getService : function ( contractId, iidName ) {
        var iid = Components.interfaces[ iidName ];
        return Components.classes[ contractId ].getService( iid );
    },
    // Show/hide one item (specified via name or the item element itself).
    showItem : function ( itemOrId, show ) {
        var item = itemOrId.constructor == String ? document.getElementById(itemOrId) : itemOrId;
        if (item)
          item.hidden = !show;
    },
    // Set given attribute of specified context-menu item.  If the
    // value is null, then it removes the attribute (which works
    // nicely for the disabled attribute).
    setItemAttr : function ( id, attr, val ) {
        var elem = document.getElementById( id );
        if ( elem ) {
            if ( val == null ) {
                // null indicates attr should be removed.
                elem.removeAttribute( attr );
            } else {
                // Set attr=val.
                elem.setAttribute( attr, val );
            }
        }
    },
    // Set context menu attribute according to like attribute of another node
    // (such as a broadcaster).
    setItemAttrFromNode : function ( item_id, attr, other_id ) {
        var elem = document.getElementById( other_id );
        if ( elem && elem.getAttribute( attr ) == "true" ) {
            this.setItemAttr( item_id, attr, "true" );
        } else {
            this.setItemAttr( item_id, attr, null );
        }
    },
    // Temporary workaround for DOM api not yet implemented by XUL nodes.
    cloneNode : function ( item ) {
        // Create another element like the one we're cloning.
        var node = document.createElement( item.tagName );

        // Copy attributes from argument item to the new one.
        var attrs = item.attributes;
        for ( var i = 0; i < attrs.length; i++ ) {
            var attr = attrs.item( i );
            node.setAttribute( attr.nodeName, attr.nodeValue );
        }

        // Voila!
        return node;
    },
    // Generate fully qualified URL for clicked-on link.
    getLinkURL : function () {
        var href = this.link.href;
        
        if (href) {
          return href;
        }

        var href = this.link.getAttributeNS("http://www.w3.org/1999/xlink",
                                          "href");

        if (!href || !href.match(/\S/)) {
          throw "Empty href"; // Without this we try to save as the current doc, for example, HTML case also throws if empty
        }
        href = makeURLAbsolute(this.link.baseURI, href);
        return href;
    },
    
    getLinkURI : function () {
         var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
         try {
           return ioService.newURI(this.linkURL, null, null);
         } catch (ex) {
           // e.g. empty URL string
           return null;
         }
    },
    
    getLinkProtocol : function () {
        if (this.linkURI) {
            return this.linkURI.scheme; // can be |undefined|
        } else {
            return null;
        }
    },

    // Get text of link.
    linkText : function () {
        var text = gatherTextUnder( this.link );
        if (!text || !text.match(/\S/)) {
          text = this.link.getAttribute("title");
          if (!text || !text.match(/\S/)) {
            text = this.link.getAttribute("alt");
            if (!text || !text.match(/\S/)) {
              text = this.linkURL;
            }
          }
        }

        return text;
    },

    // Get selected text. Only display the first 15 chars.
    isTextSelection : function() {
      // Get 16 characters, so that we can trim the selection if it's greater
      // than 15 chars
      var selectedText = getBrowserSelection(16);

      if (!selectedText)
        return false;

      if (selectedText.length > 15)
        selectedText = selectedText.substr(0,15) + "...";

      // Use the current engine if the search bar is visible, the default
      // engine otherwise.
      var engineName = "";
      var ss = Cc["@mozilla.org/browser/search-service;1"].
               getService(Ci.nsIBrowserSearchService);
      if (BrowserSearch.getSearchBar())
        engineName = ss.currentEngine.name;
      else
        engineName = ss.defaultEngine.name;

      // format "Search <engine> for <selection>" string to show in menu
      var menuLabel = gNavigatorBundle.getFormattedString("contextMenuSearchText",
                                                          [engineName,
                                                           selectedText]);
      this.setItemAttr("context-searchselect", "label", menuLabel);

      return true;
    },

    // Returns true if anything is selected.
    isContentSelection : function() {
        return !document.commandDispatcher.focusedWindow.getSelection().isCollapsed;
    },

    toString : function () {
        return "contextMenu.target     = " + this.target + "\n" +
               "contextMenu.onImage    = " + this.onImage + "\n" +
               "contextMenu.onLink     = " + this.onLink + "\n" +
               "contextMenu.link       = " + this.link + "\n" +
               "contextMenu.inFrame    = " + this.inFrame + "\n" +
               "contextMenu.hasBGImage = " + this.hasBGImage + "\n";
    },

    isTargetATextBox : function ( node )
    {
      if (node instanceof HTMLInputElement)
        return (node.type == "text" || node.type == "password")

      return (node instanceof HTMLTextAreaElement);
    },
    isTargetAKeywordField : function ( node )
    {
      var form = node.form;
      if (!form)
        return false;
      var method = form.method.toUpperCase();

      // These are the following types of forms we can create keywords for:
      //
      // method   encoding type       can create keyword
      // GET      *                                 YES
      //          *                                 YES
      // POST                                       YES
      // POST     application/x-www-form-urlencoded YES
      // POST     text/plain                        NO (a little tricky to do)
      // POST     multipart/form-data               NO
      // POST     everything else                   YES
      return (method == "GET" || method == "") ||
             (form.enctype != "text/plain") && (form.enctype != "multipart/form-data");
    },

    // Determines whether or not the separator with the specified ID should be
    // shown or not by determining if there are any non-hidden items between it
    // and the previous separator.
    shouldShowSeparator : function ( aSeparatorID )
    {
      var separator = document.getElementById(aSeparatorID);
      if (separator) {
        var sibling = separator.previousSibling;
        while (sibling && sibling.localName != "menuseparator") {
          if (sibling.getAttribute("hidden") != "true")
            return true;
          sibling = sibling.previousSibling;
        }
      }
      return false;
    },

    addDictionaries : function()
    {
      var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                            .getService(Components.interfaces.nsIPromptService);
      // FIXME bug 335605: hook this up so that it takes you to the download
      // web page
      var rv = ps.alert(window, "Add Dictionaries",
          "This command hasn't been hooked up yet. Instead, go to Thunderbird's download page:\n\nhttp://www.mozilla.org/products/thunderbird/dictionaries.html\n\nThese plugins will work in Firefox as well.");
    }
}

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
   if (!event.isTrusted) {
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
       var docWrapper = wrapper.ownerDocument;
       var locWrapper = docWrapper.location;
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

         if (!webPanelSecurityCheck(locWrapper.href, wrapper.href))
           return false;

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
#ifndef MOZ_PLACES
         openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
                    BROWSER_ADD_BM_FEATURES, dialogArgs);
         event.preventDefault();
#else
         dump("*** IMPLEMENT ME");
#endif
         return false;
       }
       else if (target == "_search") {
         // Used in WinIE as a way of transiently loading pages in a sidebar.  We
         // mimic that WinIE functionality here and also load the page transiently.

         // javascript links targeting the sidebar shouldn't be allowed
         // we copied this from IE, and IE blocks this completely
         if (wrapper.href.substr(0, 11) === "javascript:")
           return false;

         // data: URIs are just as dangerous
         if (wrapper.href.substr(0, 5) === "data:")
           return false;

         if (!webPanelSecurityCheck(locWrapper.href, wrapper.href))
           return false;

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
       !event.getPreventDefault() &&
       gPrefService.getBoolPref("middlemouse.contentLoadURL") &&
       !gPrefService.getBoolPref("general.autoScroll")) {
     middleMousePaste(event);
   }
   return true;
 }

function handleLinkClick(event, href, linkNode)
{
  var docURL = event.target.ownerDocument.location.href;

  switch (event.button) {
    case 0:
#ifdef XP_MACOSX
      if (event.metaKey) { // Cmd
#else
      if (event.ctrlKey) {
#endif
        openNewTabWith(href, docURL, null, event, false);
        event.stopPropagation();
        return true;
      }
                                                       // if left button clicked
#ifdef MOZ_FEEDS
      if (event.shiftKey && event.altKey) {
        var feedService = 
            Cc["@mozilla.org/browser/feeds/result-service;1"].
            getService(Ci.nsIFeedResultService);
        feedService.forcePreviewPage = true;
        loadURI(href, null, null, false);
        return false;
      }
#endif
                                                       
      if (event.shiftKey) {
        openNewWindowWith(href, docURL, null, false);
        event.stopPropagation();
        return true;
      }

      if (event.altKey) {
        saveURL(href, linkNode ? gatherTextUnder(linkNode) : "", null, true,
                true, makeURI(docURL, event.target.ownerDocument.characterSet));
        return true;
      }

      return false;
    case 1:                                                         // if middle button clicked
      var tab;
      try {
        tab = gPrefService.getBoolPref("browser.tabs.opentabfor.middleclick")
      }
      catch(ex) {
        tab = true;
      }
      if (tab)
        openNewTabWith(href, docURL, null, event, false);
      else
        openNewWindowWith(href, docURL, null, false);
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
        SetDefaultCharacterSet(charset);
    } else if (name == 'charsetCustomize') {
        //do nothing - please remove this else statement, once the charset prefs moves to the pref window
    } else {
        SetForcedCharset(node.getAttribute('id'));
        SetDefaultCharacterSet(node.getAttribute('id'));
    }
    } catch(ex) { alert(ex); }
}

function SetDefaultCharacterSet(charset)
{
    BrowserSetDefaultCharacterSet(charset);
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

function BrowserSetDefaultCharacterSet(aCharset)
{
  // no longer needed; set when setting Force; see bug 79608
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
  persistentOnly.hidden = (window.content.document.preferredStylesheetSet) ? haveAltSheets : false;
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

#ifdef ALTSS_ICON
function updatePageStyles(evt) {
  if (!gPageStyleButton)
    gPageStyleButton = document.getElementById("page-theme-button");

  var hasAltSS = false;
  var stylesheets = window.content.document.styleSheets;
  var preferredSheet = window.content.document.preferredStylesheetSet;
  for (var i = 0; i < stylesheets.length; ++i) {
    var currentStyleSheet = stylesheets[i];

    if (currentStyleSheet.title && currentStyleSheet.title != preferredSheet) {
        // Skip any stylesheets that don't match the screen media type.
        var media = currentStyleSheet.media.mediaText.toLowerCase();
        if (media && (media.indexOf("screen") == -1) && (media.indexOf("all") == -1))
            continue;
        hasAltSS = true;
        break;
    }
  }

  if (hasAltSS) {
    gPageStyleButton.setAttribute("themes", "true");
    // FIXME: Do a first-time explanation of page styles here perhaps?
    // Avoid for now since Firebird's default home page has an alt sheet.
  }
  else
    gPageStyleButton.removeAttribute("themes");
}
#endif
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
  
    if (!this._canGoOffline()) {
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
  sendLinkForContent: function () {
    this.sendMessage(window.content.location.href,
                     window.content.document.title);
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

#ifndef MOZ_PLACES
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
  dump("*** IMPLEMENT ME\n");
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
  },
  
  /**
   * Called when the user clicks on the Feed icon in the location bar. 
   * Builds a menu of unique feeds associated with the page, and if there
   * is only one, shows the feed inline in the browser window. 
   * @param   event
   *          The popupshowing event from the feed list menupopup
   * @returns true if the menu should be shown, false if there was only
   *          one feed and the feed should be shown inline in the browser
   *          window (do not show the menupopup).
   */
  buildFeedList: function(event) {
    var menuPopup = event.target;
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
    
    // Get the list of unique feeds, and if there's only one unique entry, 
    // show the feed in the browser rather than displaying a menu. 
    var feeds = this.harvestFeeds(feeds);
    if (feeds.length == 1) {
      this.subscribeToFeed(feeds[0].href);
      return false;
    }

    // Build the menu showing the available feed choices for viewing. 
    for (var i = 0; i < feeds.length; ++i) {
      var feedInfo = feeds[i];
      var menuItem = document.createElement("menuitem");
      var baseTitle = feedInfo.title || feedInfo.href;
#ifdef MOZ_FEEDS
      var labelStr = gNavigatorBundle.getFormattedString("feedShowFeedNew", [baseTitle]);
#else
      var labelStr = gNavigatorBundle.getFormattedString("feedShowFeed", [baseTitle]);
#endif
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
   *          The feed to subscribe to
   */
  subscribeToFeed: function(href) {
#ifdef MOZ_PLACES
#ifdef MOZ_FEEDS
      // Just load the feed in the content area to either subscribe or show the
      // preview UI
      loadURI(href, null, null, false);
#else
      PlacesCommandHook.addLiveBookmark(feeds[0].href);
#endif
#else
#ifdef MOZ_FEEDS
      // Just load the feed in the content area to either subscribe or show the
      // preview UI
      loadURI(href, null, null, false);
#else
      this.addLiveBookmark(feeds[0].href);
#endif
#endif
  },

  /**
   * Attempt to generate a list of unique feeds from the list of feeds
   * supplied by the web page. It is fairly common for a site to supply
   * feeds in multiple formats but with divergent |title| attributes so
   * we need to make a rough pass at trying to not show a menu when there
   * is in fact only one feed. If this is the case, by default select
   * the ATOM feed if one is supplied, otherwise pick the first one. 
   * @param   feeds
   *          An array of Feed info JS Objects representing the list of
   *          feeds advertised by the web page
   * @returns An array of what should be mostly unique feeds. 
   */
  harvestFeeds: function(feeds) {
    var feedHash = { };
    for (var i = 0; i < feeds.length; ++i) {
      var feed = feeds[i];
      if (!(feed.type in feedHash))
        feedHash[feed.type] = [];
      feedHash[feed.type].push(feed);
    }
    var mismatch = false;
    var count = 0;
    var defaultType = null;
    for (var type in feedHash) {
      // The default type is whichever is listed first on the web page.
      // Nothing fancy, just something that works.
      if (!defaultType) {
        defaultType = type;
        count = feedHash[type].length;
      }
      if (feedHash[type].length != count) {
        mismatch = true;
        break;
      }
      count = feedHash[type].length;
    }
    // There are more feeds of one type than another - this implies the
    // content developer is supplying multiple channels, let's not do 
    // anything fancier than this and just return the full set. 
    if (mismatch)
      return feeds;
      
    // Look for an atom feed by default, fall back to whichever was listed
    // first if there is no atom feed supplied. 
    const ATOMTYPE = "application/atom+xml";
    return ATOMTYPE in feedHash ? feedHash[ATOMTYPE] : feedHash[defaultType];
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
  
#ifndef MOZ_PLACES
  /**
   * Adds a Live Bookmark to a feed
   * @param   url
   *          The URL of the feed being bookmarked
   */
  addLiveBookmark: function(url) {
    var doc = gBrowser.selectedBrowser.contentDocument;
    var title = doc.title;
    var description = BookmarksUtils.getDescriptionFromDocument(doc);
    BookmarksUtils.addLivemark(doc.baseURI, url, title, description);
  },
#endif
  
  /**
   * Update the browser UI to show whether or not feeds are available when
   * a page is loaded or the user switches tabs to a page that has feeds. 
   */
  updateFeeds: function() {
    var feedButton = document.getElementById("feed-button");
    if (!feedButton)
      return;
    if (!this._feedMenuitem)
      this._feedMenuitem = document.getElementById("subscribeToPageMenuitem");
    if (!this._feedMenupopup)
      this._feedMenupopup = document.getElementById("subscribeToPageMenupopup");

    var feeds = gBrowser.mCurrentBrowser.feeds;
    if (!feeds || feeds.length == 0) {
      if (feedButton.hasAttribute("feeds"))
        feedButton.removeAttribute("feeds");
      feedButton.setAttribute("tooltiptext", 
                              gNavigatorBundle.getString("feedNoFeeds"));
      this._feedMenuitem.setAttribute("disabled", "true");
      this._feedMenupopup.setAttribute("hidden", "true");
      this._feedMenuitem.removeAttribute("hidden");
    } else {
      feedButton.setAttribute("feeds", "true");
      feedButton.setAttribute("tooltiptext", 
#ifdef MOZ_FEEDS
                              gNavigatorBundle.getString("feedHasFeedsNew"));
#else
                              gNavigatorBundle.getString("feedHasFeeds"));
#endif
      // check for dupes before we pick which UI to expose
      feeds = this.harvestFeeds(feeds);
      
      if (feeds.length > 1) {
        this._feedMenuitem.setAttribute("hidden", "true");
        this._feedMenupopup.removeAttribute("hidden");
      } else {
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
    const alternateRelRegex = /(^|\s)alternate($|\s)/i;
    const rssTitleRegex = /(^|\s)rss($|\s)/i;

    if (!alternateRelRegex.test(erel) ||
        !etype)
      return;

    etype = etype.replace(/^\s+/, "");
    etype = etype.replace(/\s+$/, "");
    etype = etype.replace(/\s*;.*/, "");
    etype = etype.toLowerCase();

    if (etype == "application/rss+xml" ||
        etype == "application/atom+xml" ||
        (etype == "text/xml" ||
         etype == "application/xml" ||
         etype == "application/rdf+xml") &&
        rssTitleRegex.test(etitle))
    {
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
      feeds.push({ href: wrapper.href,
                   type: etype,
                   title: wrapper.title});
      browserForLink.feeds = feeds;
      if (browserForLink == gBrowser || browserForLink == gBrowser.mCurrentBrowser) {
        var feedButton = document.getElementById("feed-button");
        if (feedButton) {
          feedButton.setAttribute("feeds", "true");
          feedButton.setAttribute("tooltiptext", 
#ifdef MOZ_FEEDS
                                  gNavigatorBundle.getString("feedHasFeedsNew"));
#else
                                  gNavigatorBundle.getString("feedHasFeeds"));
#endif
        }
      }
    }
  }
};

#ifdef MOZ_PLACES

/**
 * This is a generic command controller for browser commands. Features can
 * register commands with this controller and the events that should trigger
 * updates to their state. Each command object must implement this interface:
 * 
 * readonly attribute boolean enabled; // true if the command is enabled
 * void execute(); // performs the command
 */
var BrowserController = {
  EVENT_TABCHANGE: "tabchange",
  
  /**
   * A hash of command-name->command-objects
   */
  commands: { },
  
  /**
   * A hash of event-name->array-of-command-names
   */
  events: { },
  
  /**
   * See nsIController.idl
   */
  supportsCommand: function BC_supportsCommand(command) {
    //LOG("BrowserController.supportsCommand: " + command);
    return command in this.commands;
  },

  /**
   * See nsIController.idl
   */
  isCommandEnabled: function BC_isCommandEnabled(command) {
    //LOG("BrowserController.isCommandEnabled: " + command);
    NS_ASSERT(this.supportsCommand(command), 
           "Controller does not support: " + command);
    return this.commands[command].enabled;
  },
  
  /**
   * See nsIController.idl
   */
  doCommand: function BC_doCommand(command) {
    //LOG("BrowserController.doCommand: " + command);
    NS_ASSERT(this.supportsCommand(command), 
           "Controller does not support: " + command);
    this.commands[command].execute();
  },
  
  /**
   * See nsIController.idl
   */
  onEvent: function BC_onEvent(event) {
    if (event in this.events) {
      var commandsForEvent = this.events[event];
      for (var i = 0; i < commandsForEvent.length; ++i) 
        CommandUpdater.updateCommand(commandsForEvent[i]);
    }
  }
};
window.controllers.appendController(BrowserController);

#include browser-places.js
#include ../../../toolkit/content/debug.js

#endif

/**
 * This object is for augmenting tabs
 */
var AugmentTabs = {
  /**
   * Called in delayedStartup
   */
  init: function at_init() {
    // add the tab context menu for undo-close-tab (bz254021)
    var ssEnabled = true;
    var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
    try {
      ssEnabled = prefBranch.getBoolPref("browser.sessionstore.enabled");
    } catch (ex) {}

    if (ssEnabled)
      this._addUndoCloseTabContextMenu();
  },

  /**
   * Add undo-close-tab to tab context menu
   */
  _addUndoCloseTabContextMenu: function at_addUndoCloseTabContextMenu() {
    // get tab context menu
    var tabbrowser = getBrowser();
    var tabMenu = document.getAnonymousElementByAttribute(tabbrowser,"anonid","tabContextMenu");

    // get strings 
    var menuLabel = gNavigatorBundle.getString("tabContext.undoCloseTab");
    var menuAccessKey = gNavigatorBundle.getString("tabContext.undoCloseTabAccessKey");

    // create new menu item
    var undoCloseTabItem = document.createElement("menuitem");
    undoCloseTabItem.setAttribute("label", menuLabel);
    undoCloseTabItem.setAttribute("accesskey", menuAccessKey);
    undoCloseTabItem.addEventListener("command", this.undoCloseTab, false);

    // add to tab context menu
    var insertPos = tabMenu.lastChild.previousSibling;
    tabMenu.insertBefore(undoCloseTabItem, insertPos);
  },

  /**
   * Re-open the most-recently-closed tab
   */
  undoCloseTab: function at_undoCloseTab() {
    // get session-store service
    var ss = Cc["@mozilla.org/browser/sessionstore;1"].
             getService(Ci.nsISessionStore);
    ss.undoCloseTab(window, 0);
  }
};
