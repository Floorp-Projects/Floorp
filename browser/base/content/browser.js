# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

let Ci = Components.interfaces;
let Cu = Components.utils;
let Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NotificationDB.jsm");
Cu.import("resource:///modules/RecentWindow.jsm");


XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
                                  "resource://gre/modules/Deprecated.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUITelemetry",
                                  "resource:///modules/BrowserUITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
                                  "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
                                  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CharsetMenu",
                                  "resource://gre/modules/CharsetMenu.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ShortcutUtils",
                                  "resource://gre/modules/ShortcutUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "GMPInstallManager",
                                  "resource://gre/modules/GMPInstallManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ContentSearch",
                                  "resource:///modules/ContentSearch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutHome",
                                  "resource:///modules/AboutHome.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Favicons",
                                   "@mozilla.org/browser/favicon-service;1",
                                   "mozIAsyncFavicons");
XPCOMUtils.defineLazyServiceGetter(this, "gDNSService",
                                   "@mozilla.org/network/dns-service;1",
                                   "nsIDNSService");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Pocket",
                                  "resource:///modules/Pocket.jsm");

// Can't use XPCOMUtils for these because the scripts try to define the variables
// on window, and so the defineProperty inside defineLazyGetter fails.
Object.defineProperty(window, "pktApi", {
  get: function() {
    // Avoid this getter running again:
    delete window.pktApi;
    Services.scriptloader.loadSubScript("chrome://browser/content/pocket/pktApi.js", window);
    return window.pktApi;
  },
  configurable: true,
  enumerable: true
});
Object.defineProperty(window, "pktUI", {
  get: function() {
    // Avoid this getter running again:
    delete window.pktUI;
    Services.scriptloader.loadSubScript("chrome://browser/content/pocket/main.js", window);
    return window.pktUI;
  },
  configurable: true,
  enumerable: true
});

const nsIWebNavigation = Ci.nsIWebNavigation;

var gLastBrowserCharset = null;
var gProxyFavIcon = null;
var gLastValidURLStr = "";
var gInPrintPreviewMode = false;
var gContextMenu = null; // nsContextMenu instance
var gMultiProcessBrowser =
  window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsILoadContext)
        .useRemoteTabs;
var gAppInfo = Cc["@mozilla.org/xre/app-info;1"]
                  .getService(Ci.nsIXULAppInfo)
                  .QueryInterface(Ci.nsIXULRuntime);

#ifndef XP_MACOSX
var gEditUIVisible = true;
#endif

[
  ["gBrowser",            "content"],
  ["gNavToolbox",         "navigator-toolbox"],
  ["gURLBar",             "urlbar"],
  ["gNavigatorBundle",    "bundle_browser"]
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

// Smart getter for the findbar.  If you don't wish to force the creation of
// the findbar, check gFindBarInitialized first.

this.__defineGetter__("gFindBar", function() {
  return window.gBrowser.getFindBar();
});

this.__defineGetter__("gFindBarInitialized", function() {
  return window.gBrowser.isFindBarInitialized();
});

XPCOMUtils.defineLazyGetter(this, "gPrefService", function() {
  return Services.prefs;
});

this.__defineGetter__("AddonManager", function() {
  let tmp = {};
  Cu.import("resource://gre/modules/AddonManager.jsm", tmp);
  return this.AddonManager = tmp.AddonManager;
});
this.__defineSetter__("AddonManager", function (val) {
  delete this.AddonManager;
  return this.AddonManager = val;
});

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
  "resource://gre/modules/TelemetryStopwatch.jsm");

XPCOMUtils.defineLazyGetter(this, "gCustomizeMode", function() {
  let scope = {};
  Cu.import("resource:///modules/CustomizeMode.jsm", scope);
  return new scope.CustomizeMode(window);
});

#ifdef MOZ_SERVICES_SYNC
XPCOMUtils.defineLazyModuleGetter(this, "Weave",
  "resource://services-sync/main.js");
#endif

XPCOMUtils.defineLazyGetter(this, "PopupNotifications", function () {
  let tmp = {};
  Cu.import("resource://gre/modules/PopupNotifications.jsm", tmp);
  try {
    return new tmp.PopupNotifications(gBrowser,
                                      document.getElementById("notification-popup"),
                                      document.getElementById("notification-popup-box"));
  } catch (ex) {
    Cu.reportError(ex);
    return null;
  }
});

XPCOMUtils.defineLazyGetter(this, "DeveloperToolbar", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/DeveloperToolbar.jsm", tmp);
  return new tmp.DeveloperToolbar(window, document.getElementById("developer-toolbar"));
});

XPCOMUtils.defineLazyGetter(this, "BrowserToolboxProcess", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/ToolboxProcess.jsm", tmp);
  return tmp.BrowserToolboxProcess;
});

XPCOMUtils.defineLazyModuleGetter(this, "Social",
  "resource:///modules/Social.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ProcessHangMonitor",
  "resource:///modules/ProcessHangMonitor.jsm");

#ifdef MOZ_SAFE_BROWSING
XPCOMUtils.defineLazyModuleGetter(this, "SafeBrowsing",
  "resource://gre/modules/SafeBrowsing.jsm");
#endif

XPCOMUtils.defineLazyModuleGetter(this, "gCustomizationTabPreloader",
  "resource:///modules/CustomizationTabPreloader.jsm", "CustomizationTabPreloader");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Translation",
  "resource:///modules/translation/Translation.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SitePermissions",
  "resource:///modules/SitePermissions.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TabState",
  "resource:///modules/sessionstore/TabState.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gWebRTCUI",
  "resource:///modules/webrtcUI.jsm", "webrtcUI");

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyModuleGetter(this, "TabCrashReporter",
  "resource:///modules/ContentCrashReporters.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluginCrashReporter",
  "resource:///modules/ContentCrashReporters.jsm");
#endif

XPCOMUtils.defineLazyModuleGetter(this, "FormValidationHandler",
  "resource:///modules/FormValidationHandler.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITour",
  "resource:///modules/UITour.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CastingApps",
  "resource:///modules/CastingApps.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SimpleServiceDiscovery",
  "resource://gre/modules/SimpleServiceDiscovery.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ReaderParent",
  "resource:///modules/ReaderParent.jsm");

let gInitialPages = [
  "about:blank",
  "about:newtab",
  "about:home",
  "about:privatebrowsing",
  "about:welcomeback",
  "about:sessionrestore"
];

#include browser-addons.js
#include browser-ctrlTab.js
#include browser-customization.js
#include browser-devedition.js
#include browser-eme.js
#include browser-feeds.js
#include browser-fullScreen.js
#include browser-fullZoom.js
#include browser-gestureSupport.js
#include browser-loop.js
#include browser-places.js
#include browser-plugins.js
#include browser-readinglist.js
#include browser-safebrowsing.js
#include browser-sidebar.js
#include browser-social.js
#include browser-tabview.js
#include browser-thumbnails.js

#ifdef MOZ_DATA_REPORTING
#include browser-data-submission-info-bar.js
#endif

#ifdef MOZ_SERVICES_SYNC
#include browser-syncui.js
#endif

#include browser-fxaccounts.js

XPCOMUtils.defineLazyGetter(this, "Win7Features", function () {
#ifdef XP_WIN
  // Bug 666808 - AeroPeek support for e10s
  if (gMultiProcessBrowser)
    return null;

  const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
  if (WINTASKBAR_CONTRACTID in Cc &&
      Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available) {
    let AeroPeek = Cu.import("resource:///modules/WindowsPreviewPerTab.jsm", {}).AeroPeek;
    return {
      onOpenWindow: function () {
        AeroPeek.onOpenWindow(window);
      },
      onCloseWindow: function () {
        AeroPeek.onCloseWindow(window);
      }
    };
  }
#endif
  return null;
});

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                   "@mozilla.org/xre/app-info;1",
                                   "nsICrashReporter");
#endif

XPCOMUtils.defineLazyGetter(this, "PageMenuParent", function() {
  let tmp = {};
  Cu.import("resource://gre/modules/PageMenu.jsm", tmp);
  return new tmp.PageMenuParent();
});

function* browserWindows() {
  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements())
    yield windows.getNext();
}

/**
* We can avoid adding multiple load event listeners and save some time by adding
* one listener that calls all real handlers.
*/
function pageShowEventHandlers(persisted) {
  XULBrowserWindow.asyncUpdateUI();
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

/**
 * Click-and-Hold implementation for the Back and Forward buttons
 * XXXmano: should this live in toolbarbutton.xml?
 */
function SetClickAndHoldHandlers() {
  var timer;

  function openMenu(aButton) {
    cancelHold(aButton);
    aButton.firstChild.hidden = false;
    aButton.open = true;
  }

  function mousedownHandler(aEvent) {
    if (aEvent.button != 0 ||
        aEvent.currentTarget.open ||
        aEvent.currentTarget.disabled)
      return;

    // Prevent the menupopup from opening immediately
    aEvent.currentTarget.firstChild.hidden = true;

    aEvent.currentTarget.addEventListener("mouseout", mouseoutHandler, false);
    aEvent.currentTarget.addEventListener("mouseup", mouseupHandler, false);
    timer = setTimeout(openMenu, 500, aEvent.currentTarget);
  }

  function mouseoutHandler(aEvent) {
    let buttonRect = aEvent.currentTarget.getBoundingClientRect();
    if (aEvent.clientX >= buttonRect.left &&
        aEvent.clientX <= buttonRect.right &&
        aEvent.clientY >= buttonRect.bottom)
      openMenu(aEvent.currentTarget);
    else
      cancelHold(aEvent.currentTarget);
  }

  function mouseupHandler(aEvent) {
    cancelHold(aEvent.currentTarget);
  }

  function cancelHold(aButton) {
    clearTimeout(timer);
    aButton.removeEventListener("mouseout", mouseoutHandler, false);
    aButton.removeEventListener("mouseup", mouseupHandler, false);
  }

  function clickHandler(aEvent) {
    if (aEvent.button == 0 &&
        aEvent.target == aEvent.currentTarget &&
        !aEvent.currentTarget.open &&
        !aEvent.currentTarget.disabled) {
      let cmdEvent = document.createEvent("xulcommandevent");
      cmdEvent.initCommandEvent("command", true, true, window, 0,
                                aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKey,
                                aEvent.metaKey, null);
      aEvent.currentTarget.dispatchEvent(cmdEvent);
    }
  }

  function _addClickAndHoldListenersOnElement(aElm) {
    aElm.addEventListener("mousedown", mousedownHandler, true);
    aElm.addEventListener("click", clickHandler, true);
  }

  // Bug 414797: Clone the back/forward buttons' context menu into both buttons.
  let popup = document.getElementById("backForwardMenu").cloneNode(true);
  popup.removeAttribute("id");
  // Prevent the back/forward buttons' context attributes from being inherited.
  popup.setAttribute("context", "");

  let backButton = document.getElementById("back-button");
  backButton.setAttribute("type", "menu");
  backButton.appendChild(popup);
  _addClickAndHoldListenersOnElement(backButton);

  let forwardButton = document.getElementById("forward-button");
  popup = popup.cloneNode(true);
  forwardButton.setAttribute("type", "menu");
  forwardButton.appendChild(popup);
  _addClickAndHoldListenersOnElement(forwardButton);
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

    // Hide session restore button on about:home
    window.messageManager.broadcastAsyncMessage("Browser:HideSessionRestoreButton");

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

  var node = aDocShell.QueryInterface(Components.interfaces.nsIDocShellTreeItem);
  for (var i = 0; i < node.childCount; ++i) {
    var docShell = node.getChildAt(i);
    docShell = findChildShell(aDocument, docShell, aSoughtURI);
    if (docShell)
      return docShell;
  }
  return null;
}

var gPopupBlockerObserver = {
  _reportButton: null,

  onReportButtonClick: function (aEvent)
  {
    if (aEvent.button != 0 || aEvent.target != this._reportButton)
      return;

    document.getElementById("blockedPopupOptions")
            .openPopup(this._reportButton, "after_end", 0, 2, false, false, aEvent);
  },

  handleEvent: function (aEvent)
  {
    if (aEvent.originalTarget != gBrowser.selectedBrowser)
      return;

    if (!this._reportButton && gURLBar)
      this._reportButton = document.getElementById("page-report-button");

    if (!gBrowser.selectedBrowser.blockedPopups) {
      // Hide the icon in the location bar (if the location bar exists)
      if (gURLBar)
        this._reportButton.hidden = true;

      // Hide the notification box (if it's visible).
      var notificationBox = gBrowser.getNotificationBox();
      var notification = notificationBox.getNotificationWithValue("popup-blocked");
      if (notification) {
        notificationBox.removeNotification(notification, false);
      }
      return;
    }

    if (gURLBar)
      this._reportButton.hidden = false;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!gBrowser.selectedBrowser.blockedPopups.reported) {
      if (gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var popupCount = gBrowser.selectedBrowser.blockedPopups.length;
#ifdef XP_WIN
        var popupButtonText = gNavigatorBundle.getString("popupWarningButton");
        var popupButtonAccesskey = gNavigatorBundle.getString("popupWarningButton.accesskey");
#else
        var popupButtonText = gNavigatorBundle.getString("popupWarningButtonUnix");
        var popupButtonAccesskey = gNavigatorBundle.getString("popupWarningButtonUnix.accesskey");
#endif
        var messageBase = gNavigatorBundle.getString("popupWarning.message");
        var message = PluralForm.get(popupCount, messageBase)
                                .replace("#1", brandShortName)
                                .replace("#2", popupCount);

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
      gBrowser.selectedBrowser.blockedPopups.reported = true;
    }
  },

  toggleAllowPopupsForSite: function (aEvent)
  {
    var pm = Services.perms;
    var shouldBlock = aEvent.target.getAttribute("block") == "true";
    var perm = shouldBlock ? pm.DENY_ACTION : pm.ALLOW_ACTION;
    pm.add(gBrowser.currentURI, "popup", perm);

    if (!shouldBlock)
      this.showAllBlockedPopups(gBrowser.selectedBrowser);

    gBrowser.getNotificationBox().removeCurrentNotification();
  },

  fillPopupList: function (aEvent)
  {
    // XXXben - rather than using |currentURI| here, which breaks down on multi-framed sites
    //          we should really walk the blockedPopups and create a list of "allow for <host>"
    //          menuitems for the common subset of hosts present in the report, this will
    //          make us frame-safe.
    //
    // XXXjst - Note that when this is fixed to work with multi-framed sites,
    //          also back out the fix for bug 343772 where
    //          nsGlobalWindow::CheckOpenAllow() was changed to also
    //          check if the top window's location is whitelisted.
    let browser = gBrowser.selectedBrowser;
    var uri = browser.currentURI;
    var blockedPopupAllowSite = document.getElementById("blockedPopupAllowSite");
    try {
      blockedPopupAllowSite.removeAttribute("hidden");

      var pm = Services.perms;
      if (pm.testPermission(uri, "popup") == pm.ALLOW_ACTION) {
        // Offer an item to block popups for this site, if a whitelist entry exists
        // already for it.
        let blockString = gNavigatorBundle.getFormattedString("popupBlock", [uri.host || uri.spec]);
        blockedPopupAllowSite.setAttribute("label", blockString);
        blockedPopupAllowSite.setAttribute("block", "true");
      }
      else {
        // Offer an item to allow popups for this site
        let allowString = gNavigatorBundle.getFormattedString("popupAllow", [uri.host || uri.spec]);
        blockedPopupAllowSite.setAttribute("label", allowString);
        blockedPopupAllowSite.removeAttribute("block");
      }
    }
    catch (e) {
      blockedPopupAllowSite.setAttribute("hidden", "true");
    }

    if (PrivateBrowsingUtils.isWindowPrivate(window))
      blockedPopupAllowSite.setAttribute("disabled", "true");
    else
      blockedPopupAllowSite.removeAttribute("disabled");

    var foundUsablePopupURI = false;
    var blockedPopups = browser.blockedPopups;
    if (blockedPopups) {
      for (let i = 0; i < blockedPopups.length; i++) {
        let blockedPopup = blockedPopups[i];

        // popupWindowURI will be null if the file picker popup is blocked.
        // xxxdz this should make the option say "Show file picker" and do it (Bug 590306)
        if (!blockedPopup.popupWindowURI)
          continue;

        var popupURIspec = blockedPopup.popupWindowURI.spec;

        // Sometimes the popup URI that we get back from the blockedPopup
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
        var label = gNavigatorBundle.getFormattedString("popupShowPopupPrefix",
                                                        [popupURIspec]);
        menuitem.setAttribute("label", label);
        menuitem.setAttribute("oncommand", "gPopupBlockerObserver.showBlockedPopup(event);");
        menuitem.setAttribute("popupReportIndex", i);
        menuitem.popupReportBrowser = browser;
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
    if (aEvent.target.anchorNode.id == "page-report-button") {
      aEvent.target.anchorNode.setAttribute("open", "true");
      blockedPopupDontShowMessage.setAttribute("label", gNavigatorBundle.getString("popupWarningDontShowFromLocationbar"));
    } else
      blockedPopupDontShowMessage.setAttribute("label", gNavigatorBundle.getString("popupWarningDontShowFromMessage"));
  },

  onPopupHiding: function (aEvent) {
    if (aEvent.target.anchorNode.id == "page-report-button")
      aEvent.target.anchorNode.removeAttribute("open");

    let item = aEvent.target.lastChild;
    while (item && item.getAttribute("observes") != "blockedPopupsSeparator") {
      let next = item.previousSibling;
      item.parentNode.removeChild(item);
      item = next;
    }
  },

  showBlockedPopup: function (aEvent)
  {
    var target = aEvent.target;
    var popupReportIndex = target.getAttribute("popupReportIndex");
    let browser = target.popupReportBrowser;
    browser.unblockPopup(popupReportIndex);
  },

  showAllBlockedPopups: function (aBrowser)
  {
    let popups = aBrowser.blockedPopups;
    if (!popups)
      return;

    for (let i = 0; i < popups.length; i++) {
      if (popups[i].popupWindowURI)
        aBrowser.unblockPopup(i);
    }
  },

  editPopupSettings: function ()
  {
    var host = "";
    try {
      host = gBrowser.currentURI.host;
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
    var existingWindow = Services.wm.getMostRecentWindow("Browser:Permissions");
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
    gPrefService.setBoolPref("privacy.popups.showBrowserMessage", !showMessage);
    gBrowser.getNotificationBox().removeCurrentNotification();
  }
};

function gKeywordURIFixup({ target: browser, data: fixupInfo }) {
  let deserializeURI = (spec) => spec ? makeURI(spec) : null;

  // We get called irrespective of whether we did a keyword search, or
  // whether the original input would be vaguely interpretable as a URL,
  // so figure that out first.
  let alternativeURI = deserializeURI(fixupInfo.fixedURI);
  if (!fixupInfo.keywordProviderName  || !alternativeURI || !alternativeURI.host) {
    return;
  }

  // At this point we're still only just about to load this URI.
  // When the async DNS lookup comes back, we may be in any of these states:
  // 1) still on the previous URI, waiting for the preferredURI (keyword
  //    search) to respond;
  // 2) at the keyword search URI (preferredURI)
  // 3) at some other page because the user stopped navigation.
  // We keep track of the currentURI to detect case (1) in the DNS lookup
  // callback.
  let previousURI = browser.currentURI;
  let preferredURI = deserializeURI(fixupInfo.preferredURI);

  // now swap for a weak ref so we don't hang on to browser needlessly
  // even if the DNS query takes forever
  let weakBrowser = Cu.getWeakReference(browser);
  browser = null;

  // Additionally, we need the host of the parsed url
  let hostName = alternativeURI.host;
  // and the ascii-only host for the pref:
  let asciiHost = alternativeURI.asciiHost;
  // Normalize out a single trailing dot - NB: not using endsWith/lastIndexOf
  // because we need to be sure this last dot is the *only* dot, too.
  // More generally, this is used for the pref and should stay in sync with
  // the code in nsDefaultURIFixup::KeywordURIFixup .
  if (asciiHost.indexOf('.') == asciiHost.length - 1) {
    asciiHost = asciiHost.slice(0, -1);
  }

  // Ignore number-only things entirely (no decimal IPs for you!)
  if (/^\d+$/.test(asciiHost))
    return;

  let onLookupComplete = (request, record, status) => {
    let browser = weakBrowser.get();
    if (!Components.isSuccessCode(status) || !browser)
      return;

    let currentURI = browser.currentURI;
    // If we're in case (3) (see above), don't show an info bar.
    if (!currentURI.equals(previousURI) &&
        !currentURI.equals(preferredURI)) {
      return;
    }

    // show infobar offering to visit the host
    let notificationBox = gBrowser.getNotificationBox(browser);
    if (notificationBox.getNotificationWithValue("keyword-uri-fixup"))
      return;

    let message = gNavigatorBundle.getFormattedString(
      "keywordURIFixup.message", [hostName]);
    let yesMessage = gNavigatorBundle.getFormattedString(
      "keywordURIFixup.goTo", [hostName])

    let buttons = [
      {
        label: yesMessage,
        accessKey: gNavigatorBundle.getString("keywordURIFixup.goTo.accesskey"),
        callback: function() {
          // Do not set this preference while in private browsing.
          if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
            let pref = "browser.fixup.domainwhitelist." + asciiHost;
            Services.prefs.setBoolPref(pref, true);
          }
          openUILinkIn(alternativeURI.spec, "current");
        }
      },
      {
        label: gNavigatorBundle.getString("keywordURIFixup.dismiss"),
        accessKey: gNavigatorBundle.getString("keywordURIFixup.dismiss.accesskey"),
        callback: function() {
          let notification = notificationBox.getNotificationWithValue("keyword-uri-fixup");
          notificationBox.removeNotification(notification, true);
        }
      }
    ];
    let notification =
      notificationBox.appendNotification(message,"keyword-uri-fixup", null,
                                         notificationBox.PRIORITY_INFO_HIGH,
                                         buttons);
    notification.persistence = 1;
  };

  try {
    gDNSService.asyncResolve(hostName, 0, onLookupComplete, Services.tm.mainThread);
  } catch (ex) {
    // Do nothing if the URL is invalid (we don't want to show a notification in that case).
    if (ex.result != Cr.NS_ERROR_UNKNOWN_HOST) {
      // ... otherwise, report:
      Cu.reportError(ex);
    }
  }
}

// A shared function used by both remote and non-remote browser XBL bindings to
// load a URI or redirect it to the correct process.
function _loadURIWithFlags(browser, uri, params) {
  if (!uri) {
    uri = "about:blank";
  }
  let flags = params.flags || 0;
  let referrer = params.referrerURI;
  let referrerPolicy = ('referrerPolicy' in params ? params.referrerPolicy :
                        Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT);
  let charset = params.charset;
  let postdata = params.postData;

  if (!(flags & browser.webNavigation.LOAD_FLAGS_FROM_EXTERNAL)) {
    browser.userTypedClear++;
  }

  let process = browser.isRemoteBrowser ? Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT
                                        : Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  let mustChangeProcess = gMultiProcessBrowser &&
                          !E10SUtils.canLoadURIInProcess(uri, process);
  try {
    if (!mustChangeProcess) {
      browser.webNavigation.loadURIWithOptions(uri, flags,
                                               referrer, referrerPolicy,
                                               postdata, null, null);
    } else {
      LoadInOtherProcess(browser, {
        uri: uri,
        flags: flags,
        referrer: referrer ? referrer.spec : null,
        referrerPolicy: referrerPolicy,
      });
    }
  } catch (e) {
    // If anything goes wrong just switch remoteness manually and load the URI.
    // We might lose history that way but at least the browser loaded a page.
    // This might be necessary if SessionStore wasn't initialized yet i.e.
    // when the homepage is a non-remote page.
    gBrowser.updateBrowserRemotenessByURL(browser, uri);
    browser.webNavigation.loadURIWithOptions(uri, flags, referrer, referrerPolicy,
                                             postdata, null, null);
  } finally {
    if (browser.userTypedClear) {
      browser.userTypedClear--;
    }
  }
}

// Starts a new load in the browser first switching the browser to the correct
// process
function LoadInOtherProcess(browser, loadOptions, historyIndex = -1) {
  let tab = gBrowser.getTabForBrowser(browser);
  // Flush the tab state before getting it
  TabState.flush(browser);
  let tabState = JSON.parse(SessionStore.getTabState(tab));

  if (historyIndex < 0) {
    tabState.userTypedValue = null;
    // Tell session history the new page to load
    SessionStore._restoreTabAndLoad(tab, JSON.stringify(tabState), loadOptions);
  }
  else {
    // Update the history state to point to the requested index
    tabState.index = historyIndex + 1;
    // SessionStore takes care of setting the browser remoteness before restoring
    // history into it.
    SessionStore.setTabState(tab, JSON.stringify(tabState));
  }
}

// Called when a docshell has attempted to load a page in an incorrect process.
// This function is responsible for loading the page in the correct process.
function RedirectLoad({ target: browser, data }) {
  // We should only start the redirection if the browser window has finished
  // starting up. Otherwise, we should wait until the startup is done.
  if (gBrowserInit.delayedStartupFinished) {
    LoadInOtherProcess(browser, data.loadOptions, data.historyIndex);
  } else {
    let delayedStartupFinished = (subject, topic) => {
      if (topic == "browser-delayed-startup-finished" &&
          subject == window) {
        Services.obs.removeObserver(delayedStartupFinished, topic);
        LoadInOtherProcess(browser, data.loadOptions, data.historyIndex);
      }
    };
    Services.obs.addObserver(delayedStartupFinished,
                             "browser-delayed-startup-finished",
                             false);
  }
}

var gBrowserInit = {
  delayedStartupFinished: false,

  onLoad: function() {
    gBrowser.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver, false);

    Services.obs.addObserver(gPluginHandler.NPAPIPluginCrashed, "plugin-crashed", false);

    window.addEventListener("AppCommand", HandleAppCommandEvent, true);

    // These routines add message listeners. They must run before
    // loading the frame script to ensure that we don't miss any
    // message sent between when the frame script is loaded and when
    // the listener is registered.
    DOMLinkHandler.init();
    gPageStyleMenu.init();
    LanguageDetectionListener.init();
    BrowserOnClick.init();
    DevEdition.init();
    AboutPrivateBrowsingListener.init();

    let mm = window.getGroupMessageManager("browsers");
    mm.loadFrameScript("chrome://browser/content/tab-content.js", true);
    mm.loadFrameScript("chrome://browser/content/content.js", true);
    mm.loadFrameScript("chrome://browser/content/content-UITour.js", true);
    mm.loadFrameScript("chrome://global/content/manifestMessages.js", true);

    window.messageManager.addMessageListener("Browser:LoadURI", RedirectLoad);

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

    if (!gMultiProcessBrowser) {
      // There is a Content:Click message manually sent from content.
      Cc["@mozilla.org/eventlistenerservice;1"]
        .getService(Ci.nsIEventListenerService)
        .addSystemEventListener(gBrowser, "click", contentAreaClick, true);
    }

    // hook up UI through progress listener
    gBrowser.addProgressListener(window.XULBrowserWindow);
    gBrowser.addTabsProgressListener(window.TabsProgressListener);

    // setup simple gestures support
    gGestureSupport.init(true);

    // setup history swipe animation
    gHistorySwipeAnimation.init();

    SidebarUI.init();

    // Certain kinds of automigration rely on this notification to complete
    // their tasks BEFORE the browser window is shown. SessionStore uses it to
    // restore tabs into windows AFTER important parts like gMultiProcessBrowser
    // have been initialized.
    Services.obs.notifyObservers(window, "browser-window-before-show", "");

    // Set a sane starting width/height for all resolutions on new profiles.
    if (!document.documentElement.hasAttribute("width")) {
      let defaultWidth;
      let defaultHeight;

      // Very small: maximize the window
      // Portrait  : use about full width and 3/4 height, to view entire pages
      //             at once (without being obnoxiously tall)
      // Widescreen: use about half width, to suggest side-by-side page view
      // Otherwise : use 3/4 height and width
      if (screen.availHeight <= 600) {
        document.documentElement.setAttribute("sizemode", "maximized");
        defaultWidth = 610;
        defaultHeight = 450;
      }
      else {
        if (screen.availWidth <= screen.availHeight) {
          defaultWidth = screen.availWidth * .9;
          defaultHeight = screen.availHeight * .75;
        }
        else if (screen.availWidth >= 2048) {
          defaultWidth = (screen.availWidth / 2) - 20;
          defaultHeight = screen.availHeight - 10;
        }
        else {
          defaultWidth = screen.availWidth * .75;
          defaultHeight = screen.availHeight * .75;
        }

#if MOZ_WIDGET_GTK == 2
        // On X, we're not currently able to account for the size of the window
        // border.  Use 28px as a guess (titlebar + bottom window border)
        defaultHeight -= 28;
#endif
      }
      document.documentElement.setAttribute("width", defaultWidth);
      document.documentElement.setAttribute("height", defaultHeight);
    }

    if (!window.toolbar.visible) {
      // adjust browser UI for popups
      if (gURLBar) {
        gURLBar.setAttribute("readonly", "true");
        gURLBar.setAttribute("enablehistory", "false");
      }
      goSetCommandEnabled("cmd_newNavigatorTab", false);
    }

    // Misc. inits.
    CombinedStopReload.init();
    gPrivateBrowsingUI.init();
    TabsInTitlebar.init();

#ifdef XP_WIN
    if (window.matchMedia("(-moz-os-version: windows-win8)").matches &&
        window.matchMedia("(-moz-windows-default-theme)").matches) {
      let windowFrameColor = Cu.import("resource:///modules/Windows8WindowFrameColor.jsm", {})
                               .Windows8WindowFrameColor.get();

      // Formula from W3C's WCAG 2.0 spec's color ratio and relative luminance,
      // section 1.3.4, http://www.w3.org/TR/WCAG20/ .
      windowFrameColor = windowFrameColor.map((color) => {
        if (color <= 10) {
          return color / 255 / 12.92;
        }
        return Math.pow(((color / 255) + 0.055) / 1.055, 2.4);
      });
      let backgroundLuminance = windowFrameColor[0] * 0.2126 +
                                windowFrameColor[1] * 0.7152 +
                                windowFrameColor[2] * 0.0722;
      let foregroundLuminance = 0; // Default to black for foreground text.
      let contrastRatio = (backgroundLuminance + 0.05) / (foregroundLuminance + 0.05);
      if (contrastRatio < 3) {
        document.documentElement.setAttribute("darkwindowframe", "true");
      }
    }
#endif

    ToolbarIconColor.init();

    // Wait until chrome is painted before executing code not critical to making the window visible
    this._boundDelayedStartup = this._delayedStartup.bind(this);
    window.addEventListener("MozAfterPaint", this._boundDelayedStartup);

    this._loadHandled = true;
  },

  _cancelDelayedStartup: function () {
    window.removeEventListener("MozAfterPaint", this._boundDelayedStartup);
    this._boundDelayedStartup = null;
  },

  _delayedStartup: function() {
    let tmp = {};
    Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", tmp);
    let TelemetryTimestamps = tmp.TelemetryTimestamps;
    TelemetryTimestamps.add("delayedStartupStarted");

    this._cancelDelayedStartup();

    // We need to set the MozApplicationManifest event listeners up
    // before we start loading the home pages in case a document has
    // a "manifest" attribute, in which the MozApplicationManifest event
    // will be fired.
    gBrowser.addEventListener("MozApplicationManifest",
                              OfflineApps, false);
    // listen for offline apps on social
    let socialBrowser = document.getElementById("social-sidebar-browser");
    socialBrowser.addEventListener("MozApplicationManifest",
                              OfflineApps, false);

    // This pageshow listener needs to be registered before we may call
    // swapBrowsersAndCloseOther() to receive pageshow events fired by that.
    let mm = window.messageManager;
    mm.addMessageListener("PageVisibility:Show", function(message) {
      if (message.target == gBrowser.selectedBrowser) {
        setTimeout(pageShowEventHandlers, 0, message.data.persisted);
      }
    });

    gBrowser.addEventListener("AboutTabCrashedLoad", function(event) {
#ifdef MOZ_CRASHREPORTER
      TabCrashReporter.onAboutTabCrashedLoad(gBrowser.getBrowserForDocument(event.target));
#endif
    }, false, true);

    gBrowser.addEventListener("AboutTabCrashedMessage", function(event) {
      let ownerDoc = event.originalTarget;

      if (!ownerDoc.documentURI.startsWith("about:tabcrashed")) {
        return;
      }

      let isTopFrame = (ownerDoc.defaultView.parent === ownerDoc.defaultView);
      if (!isTopFrame) {
        return;
      }

      let browser = gBrowser.getBrowserForDocument(ownerDoc);
#ifdef MOZ_CRASHREPORTER
      if (event.detail.sendCrashReport) {
        TabCrashReporter.submitCrashReport(browser);
      }
#endif

      let tab = gBrowser.getTabForBrowser(browser);
      switch (event.detail.message) {
      case "closeTab":
        gBrowser.removeTab(tab, { animate: true });
        break;
      case "restoreTab":
        SessionStore.reviveCrashedTab(tab);
        break;
      case "restoreAll":
        for (let browserWin of browserWindows()) {
          for (let tab of window.gBrowser.tabs) {
            SessionStore.reviveCrashedTab(tab);
          }
        }
        break;
      }
    }, false, true);

    let uriToLoad = this._getUriToLoad();
    if (uriToLoad && uriToLoad != "about:blank") {
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
        let tabToOpen = uriToLoad;

        // Stop the about:blank load
        gBrowser.stop();
        // make sure it has a docshell
        gBrowser.docShell;

        // If the browser that we're swapping in was remote, then we'd better
        // be able to support remote browsers, and then make our selectedTab
        // remote.
        try {
          if (tabToOpen.linkedBrowser.isRemoteBrowser) {
            if (!gMultiProcessBrowser) {
              throw new Error("Cannot drag a remote browser into a window " +
                              "without the remote tabs load context.");
            }

            gBrowser.updateBrowserRemoteness(gBrowser.selectedBrowser, true);
          }
          gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, tabToOpen);
        } catch(e) {
          Cu.reportError(e);
        }
      }
      // window.arguments[2]: referrer (nsIURI | string)
      //                 [3]: postData (nsIInputStream)
      //                 [4]: allowThirdPartyFixup (bool)
      //                 [5]: referrerPolicy (int)
      else if (window.arguments.length >= 3) {
        let referrerURI = window.arguments[2];
        if (typeof(referrerURI) == "string") {
          try {
            referrerURI = makeURI(referrerURI);
          } catch (e) {
            referrerURI = null;
          }
        }
        let referrerPolicy = (window.arguments[5] != undefined ?
            window.arguments[5] : Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT);
        loadURI(uriToLoad, referrerURI, window.arguments[3] || null,
                window.arguments[4] || false, referrerPolicy);
        window.focus();
      }
      // Note: loadOneOrMoreURIs *must not* be called if window.arguments.length >= 3.
      // Such callers expect that window.arguments[0] is handled as a single URI.
      else {
        loadOneOrMoreURIs(uriToLoad);
      }
    }

#ifdef MOZ_SAFE_BROWSING
    // Bug 778855 - Perf regression if we do this here. To be addressed in bug 779008.
    setTimeout(function() { SafeBrowsing.init(); }, 2000);
#endif

    Services.obs.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-disabled", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-started", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-blocked", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-failed", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-confirmation", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-complete", false);
    window.messageManager.addMessageListener("Browser:URIFixup", gKeywordURIFixup);

    BrowserOffline.init();
    OfflineApps.init();
    IndexedDBPromptHelper.init();
#ifdef E10S_TESTING_ONLY
    gRemoteTabsUI.init();
#endif
    ReadingListUI.init();

    // Initialize the full zoom setting.
    // We do this before the session restore service gets initialized so we can
    // apply full zoom settings to tabs restored by the session restore service.
    FullZoom.init();
    PanelUI.init();
    LightweightThemeListener.init();

    Services.telemetry.getHistogramById("E10S_WINDOW").add(gMultiProcessBrowser);

    SidebarUI.startDelayedLoad();

    UpdateUrlbarSearchSplitterState();

    if (!(isBlankPageURL(uriToLoad) || uriToLoad == "about:privatebrowsing") ||
        !focusAndSelectUrlBar()) {
      gBrowser.selectedBrowser.focus();
    }

    // Set up Sanitize Item
    this._initializeSanitizer();

    // Enable/Disable auto-hide tabbar
    gBrowser.tabContainer.updateVisibility();

    BookmarkingUI.init();

    gPrefService.addObserver(gHomeButton.prefDomain, gHomeButton, false);

    var homeButton = document.getElementById("home-button");
    gHomeButton.updateTooltip(homeButton);
    gHomeButton.updatePersonalToolbarStyle(homeButton);

    let safeMode = document.getElementById("helpSafeMode");
    if (Services.appinfo.inSafeMode) {
      safeMode.label = safeMode.getAttribute("stoplabel");
      safeMode.accesskey = safeMode.getAttribute("stopaccesskey");
    }

    // BiDi UI
    gBidiUI = isBidiEnabled();
    if (gBidiUI) {
      document.getElementById("documentDirection-separator").hidden = false;
      document.getElementById("documentDirection-swap").hidden = false;
      document.getElementById("textfieldDirection-separator").hidden = false;
      document.getElementById("textfieldDirection-swap").hidden = false;
    }

    // Setup click-and-hold gestures access to the session history
    // menus if global click-and-hold isn't turned on
    if (!getBoolPref("ui.click_hold_context_menus", false))
      SetClickAndHoldHandlers();

    let NP = {};
    Cu.import("resource:///modules/NetworkPrioritizer.jsm", NP);
    NP.trackBrowserWindow(window);

    PlacesToolbarHelper.init();

    ctrlTab.readPref();
    gPrefService.addObserver(ctrlTab.prefName, ctrlTab, false);

    // Initialize the download manager some time after the app starts so that
    // auto-resume downloads begin (such as after crashing or quitting with
    // active downloads) and speeds up the first-load of the download manager UI.
    // If the user manually opens the download manager before the timeout, the
    // downloads will start right away, and initializing again won't hurt.
    setTimeout(function() {
      try {
        Cu.import("resource:///modules/DownloadsCommon.jsm", {})
          .DownloadsCommon.initializeAllDataLinks();
        Cu.import("resource:///modules/DownloadsTaskbar.jsm", {})
          .DownloadsTaskbar.registerIndicator(window);
      } catch (ex) {
        Cu.reportError(ex);
      }
    }, 10000);

    // Load the Login Manager data from disk off the main thread, some time
    // after startup.  If the data is required before the timeout, for example
    // because a restored page contains a password field, it will be loaded on
    // the main thread, and this initialization request will be ignored.
    setTimeout(function() {
      try {
        Services.logins;
      } catch (ex) {
        Cu.reportError(ex);
      }
    }, 3000);

    // The object handling the downloads indicator is also initialized here in the
    // delayed startup function, but the actual indicator element is not loaded
    // unless there are downloads to be displayed.
    DownloadsButton.initializeIndicator();

#ifndef XP_MACOSX
    updateEditUIVisibility();
    let placesContext = document.getElementById("placesContext");
    placesContext.addEventListener("popupshowing", updateEditUIVisibility, false);
    placesContext.addEventListener("popuphiding", updateEditUIVisibility, false);
#endif

    gBrowser.mPanelContainer.addEventListener("InstallBrowserTheme", LightWeightThemeWebInstaller, false, true);
    gBrowser.mPanelContainer.addEventListener("PreviewBrowserTheme", LightWeightThemeWebInstaller, false, true);
    gBrowser.mPanelContainer.addEventListener("ResetBrowserThemePreview", LightWeightThemeWebInstaller, false, true);

    if (Win7Features)
      Win7Features.onOpenWindow();

    FullScreen.init();

#ifdef MOZ_SERVICES_SYNC
    // initialize the sync UI
    gSyncUI.init();
    gFxAccounts.init();
#endif

#ifdef MOZ_DATA_REPORTING
    gDataNotificationInfoBar.init();
#endif

    LoopUI.init();

    gBrowserThumbnails.init();

    // Add Devtools menuitems and listeners
    gDevToolsBrowser.registerBrowserWindow(window);

    gMenuButtonUpdateBadge.init();

    window.addEventListener("mousemove", MousePosTracker, false);
    window.addEventListener("dragover", MousePosTracker, false);

    gNavToolbox.addEventListener("customizationstarting", CustomizationHandler);
    gNavToolbox.addEventListener("customizationchange", CustomizationHandler);
    gNavToolbox.addEventListener("customizationending", CustomizationHandler);

    // End startup crash tracking after a delay to catch crashes while restoring
    // tabs and to postpone saving the pref to disk.
    try {
      const startupCrashEndDelay = 30 * 1000;
      setTimeout(Services.startup.trackStartupCrashEnd, startupCrashEndDelay);
    } catch (ex) {
      Cu.reportError("Could not end startup crash tracking: " + ex);
    }

    // Delay this a minute because there's no rush
    setTimeout(() => {
      this.gmpInstallManager = new GMPInstallManager();
      // We don't really care about the results, if someone is interested they
      // can check the log.
      this.gmpInstallManager.simpleCheckAndInstall().then(null, () => {});
    }, 1000 * 60);

    SessionStore.promiseInitialized.then(() => {
      // Bail out if the window has been closed in the meantime.
      if (window.closed) {
        return;
      }

      // Enable the Restore Last Session command if needed
      RestoreLastSessionObserver.init();

      SocialUI.init();
      TabView.init();

      // Telemetry for master-password - we do this after 5 seconds as it
      // can cause IO if NSS/PSM has not already initialized.
      setTimeout(() => {
        if (window.closed) {
          return;
        }
        let secmodDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                       .getService(Ci.nsIPKCS11ModuleDB);
        let slot = secmodDB.findSlotByName("");
        let mpEnabled = slot &&
                        slot.status != Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED &&
                        slot.status != Ci.nsIPKCS11Slot.SLOT_READY;
        if (mpEnabled) {
          Services.telemetry.getHistogramById("MASTER_PASSWORD_ENABLED").add(mpEnabled);
        }
      }, 5000);

      // Telemetry for tracking protection.
      let tpEnabled = gPrefService
                      .getBoolPref("privacy.trackingprotection.enabled");
      Services.telemetry.getHistogramById("TRACKING_PROTECTION_ENABLED")
        .add(tpEnabled);

      PanicButtonNotifier.init();
    });
    this.delayedStartupFinished = true;

    Services.obs.notifyObservers(window, "browser-delayed-startup-finished", "");
    TelemetryTimestamps.add("delayedStartupFinished");
  },

  // Returns the URI(s) to load at startup.
  _getUriToLoad: function () {
    // window.arguments[0]: URI to load (string), or an nsISupportsArray of
    //                      nsISupportsStrings to load, or a xul:tab of
    //                      a tabbrowser, which will be replaced by this
    //                      window (for this case, all other arguments are
    //                      ignored).
    if (!window.arguments || !window.arguments[0])
      return null;

    let uri = window.arguments[0];
    let sessionStartup = Cc["@mozilla.org/browser/sessionstartup;1"]
                           .getService(Ci.nsISessionStartup);
    let defaultArgs = Cc["@mozilla.org/browser/clh;1"]
                        .getService(Ci.nsIBrowserHandler)
                        .defaultArgs;

    // If the given URI matches defaultArgs (the default homepage) we want
    // to block its load if we're going to restore a session anyway.
    if (uri == defaultArgs && sessionStartup.willOverrideHomepage)
      return null;

    return uri;
  },

  onUnload: function() {
    // In certain scenarios it's possible for unload to be fired before onload,
    // (e.g. if the window is being closed after browser.js loads but before the
    // load completes). In that case, there's nothing to do here.
    if (!this._loadHandled)
      return;

    gDevToolsBrowser.forgetBrowserWindow(window);

    let desc = Object.getOwnPropertyDescriptor(window, "DeveloperToolbar");
    if (desc && !desc.get) {
      DeveloperToolbar.destroy();
    }

    // First clean up services initialized in gBrowserInit.onLoad (or those whose
    // uninit methods don't depend on the services having been initialized).

    CombinedStopReload.uninit();

    gGestureSupport.init(false);

    gHistorySwipeAnimation.uninit();

    FullScreen.uninit();

#ifdef MOZ_SERVICES_SYNC
    gFxAccounts.uninit();
#endif

    Services.obs.removeObserver(gPluginHandler.NPAPIPluginCrashed, "plugin-crashed");

    try {
      gBrowser.removeProgressListener(window.XULBrowserWindow);
      gBrowser.removeTabsProgressListener(window.TabsProgressListener);
    } catch (ex) {
    }

    PlacesToolbarHelper.uninit();

    BookmarkingUI.uninit();

    TabsInTitlebar.uninit();

    ToolbarIconColor.uninit();

    BrowserOnClick.uninit();

    DevEdition.uninit();

    gMenuButtonUpdateBadge.uninit();

    ReadingListUI.uninit();

    SidebarUI.uninit();

    // Now either cancel delayedStartup, or clean up the services initialized from
    // it.
    if (this._boundDelayedStartup) {
      this._cancelDelayedStartup();
    } else {
      if (Win7Features)
        Win7Features.onCloseWindow();

      gPrefService.removeObserver(ctrlTab.prefName, ctrlTab);
      ctrlTab.uninit();
      TabView.uninit();
      SocialUI.uninit();
      gBrowserThumbnails.uninit();
      LoopUI.uninit();
      FullZoom.destroy();

      Services.obs.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-disabled");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-started");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-blocked");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-failed");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-confirmation");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-complete");
      window.messageManager.removeMessageListener("Browser:URIFixup", gKeywordURIFixup);
      window.messageManager.removeMessageListener("Browser:LoadURI", RedirectLoad);

      try {
        gPrefService.removeObserver(gHomeButton.prefDomain, gHomeButton);
      } catch (ex) {
        Cu.reportError(ex);
      }

      if (this.gmpInstallManager) {
        this.gmpInstallManager.uninit();
      }

      BrowserOffline.uninit();
      OfflineApps.uninit();
      IndexedDBPromptHelper.uninit();
      LightweightThemeListener.uninit();
      PanelUI.uninit();
    }

    // Final window teardown, do this last.
    window.XULBrowserWindow = null;
    window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebNavigation)
          .QueryInterface(Ci.nsIDocShellTreeItem).treeOwner
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIXULWindow)
          .XULBrowserWindow = null;
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = null;
  },

#ifdef XP_MACOSX
  // nonBrowserWindowStartup(), nonBrowserWindowDelayedStartup(), and
  // nonBrowserWindowShutdown() are used for non-browser windows in
  // macBrowserOverlay
  nonBrowserWindowStartup: function() {
    // Disable inappropriate commands / submenus
    var disabledItems = ['Browser:SavePage',
                         'Browser:SendLink', 'cmd_pageSetup', 'cmd_print', 'cmd_find', 'cmd_findAgain',
                         'viewToolbarsMenu', 'viewSidebarMenuMenu', 'Browser:Reload',
                         'viewFullZoomMenu', 'pageStyleMenu', 'charsetMenu', 'View:PageSource', 'View:FullScreen',
                         'viewHistorySidebar', 'Browser:AddBookmarkAs', 'Browser:BookmarkAllTabs',
                         'View:PageInfo', 'Browser:ToggleTabView'];
    var element;

    for (let disabledItem of disabledItems) {
      element = document.getElementById(disabledItem);
      if (element)
        element.setAttribute("disabled", "true");
    }

    // If no windows are active (i.e. we're the hidden window), disable the close, minimize
    // and zoom menu commands as well
    if (window.location.href == "chrome://browser/content/hiddenWindow.xul") {
      var hiddenWindowDisabledItems = ['cmd_close', 'minimizeWindow', 'zoomWindow'];
      for (let hiddenWindowDisabledItem of hiddenWindowDisabledItems) {
        element = document.getElementById(hiddenWindowDisabledItem);
        if (element)
          element.setAttribute("disabled", "true");
      }

      // also hide the window-list separator
      element = document.getElementById("sep-window-list");
      element.setAttribute("hidden", "true");

      // Setup the dock menu.
      let dockMenuElement = document.getElementById("menu_mac_dockmenu");
      if (dockMenuElement != null) {
        let nativeMenu = Cc["@mozilla.org/widget/standalonenativemenu;1"]
                         .createInstance(Ci.nsIStandaloneNativeMenu);

        try {
          nativeMenu.init(dockMenuElement);

          let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
                            .getService(Ci.nsIMacDockSupport);
          dockSupport.dockMenu = nativeMenu;
        }
        catch (e) {
        }
      }
    }

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      document.getElementById("macDockMenuNewWindow").hidden = true;
    }

    this._delayedStartupTimeoutId = setTimeout(this.nonBrowserWindowDelayedStartup.bind(this), 0);
  },

  nonBrowserWindowDelayedStartup: function() {
    this._delayedStartupTimeoutId = null;

    // initialise the offline listener
    BrowserOffline.init();

    // Set up Sanitize Item
    this._initializeSanitizer();

    // initialize the private browsing UI
    gPrivateBrowsingUI.init();

#ifdef MOZ_SERVICES_SYNC
    // initialize the sync UI
    gSyncUI.init();
#endif

#ifdef E10S_TESTING_ONLY
    gRemoteTabsUI.init();
#endif
  },

  nonBrowserWindowShutdown: function() {
    // If nonBrowserWindowDelayedStartup hasn't run yet, we have no work to do -
    // just cancel the pending timeout and return;
    if (this._delayedStartupTimeoutId) {
      clearTimeout(this._delayedStartupTimeoutId);
      return;
    }

    BrowserOffline.uninit();
  },
#endif

  _initializeSanitizer: function() {
    const kDidSanitizeDomain = "privacy.sanitize.didShutdownSanitize";
    if (gPrefService.prefHasUserValue(kDidSanitizeDomain)) {
      gPrefService.clearUserPref(kDidSanitizeDomain);
      // We need to persist this preference change, since we want to
      // check it at next app start even if the browser exits abruptly
      gPrefService.savePrefFile(null);
    }

    /**
     * Migrate Firefox 3.0 privacy.item prefs under one of these conditions:
     *
     * a) User has customized any privacy.item prefs
     * b) privacy.sanitize.sanitizeOnShutdown is set
     */
    if (!gPrefService.getBoolPref("privacy.sanitize.migrateFx3Prefs")) {
      let itemBranch = gPrefService.getBranch("privacy.item.");
      let itemArray = itemBranch.getChildList("");

      // See if any privacy.item prefs are set
      let doMigrate = itemArray.some(function (name) itemBranch.prefHasUserValue(name));
      // Or if sanitizeOnShutdown is set
      if (!doMigrate)
        doMigrate = gPrefService.getBoolPref("privacy.sanitize.sanitizeOnShutdown");

      if (doMigrate) {
        let cpdBranch = gPrefService.getBranch("privacy.cpd.");
        let clearOnShutdownBranch = gPrefService.getBranch("privacy.clearOnShutdown.");
        for (let name of itemArray) {
          try {
            // don't migrate password or offlineApps clearing in the CRH dialog since
            // there's no UI for those anymore. They default to false. bug 497656
            if (name != "passwords" && name != "offlineApps")
              cpdBranch.setBoolPref(name, itemBranch.getBoolPref(name));
            clearOnShutdownBranch.setBoolPref(name, itemBranch.getBoolPref(name));
          }
          catch(e) {
            Cu.reportError("Exception thrown during privacy pref migration: " + e);
          }
        }
      }

      gPrefService.setBoolPref("privacy.sanitize.migrateFx3Prefs", true);
    }
  },
}


/* Legacy global init functions */
var BrowserStartup        = gBrowserInit.onLoad.bind(gBrowserInit);
var BrowserShutdown       = gBrowserInit.onUnload.bind(gBrowserInit);
#ifdef XP_MACOSX
var nonBrowserWindowStartup        = gBrowserInit.nonBrowserWindowStartup.bind(gBrowserInit);
var nonBrowserWindowDelayedStartup = gBrowserInit.nonBrowserWindowDelayedStartup.bind(gBrowserInit);
var nonBrowserWindowShutdown       = gBrowserInit.nonBrowserWindowShutdown.bind(gBrowserInit);
#endif

function HandleAppCommandEvent(evt) {
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
    if (XULBrowserWindow.stopCommand.getAttribute("disabled") != "true")
      BrowserStop();
    break;
  case "Search":
    BrowserSearch.webSearch();
    break;
  case "Bookmarks":
    SidebarUI.toggle("viewBookmarksSidebar");
    break;
  case "Home":
    BrowserHome();
    break;
  case "New":
    BrowserOpenTab();
    break;
  case "Close":
    BrowserCloseTabOrWindow();
    break;
  case "Find":
    gFindBar.onFindCommand();
    break;
  case "Help":
    openHelpLink('firefox-help');
    break;
  case "Open":
    BrowserOpenFileWindow();
    break;
  case "Print":
    PrintUtils.print(gBrowser.selectedBrowser.contentWindowAsCPOW,
                     gBrowser.selectedBrowser);
    break;
  case "Save":
    saveDocument(gBrowser.selectedBrowser.contentDocumentAsCPOW);
    break;
  case "SendMail":
    MailIntegration.sendLinkForBrowser(gBrowser.selectedBrowser);
    break;
  default:
    return;
  }
  evt.stopPropagation();
  evt.preventDefault();
}

function gotoHistoryIndex(aEvent) {
  let index = aEvent.target.getAttribute("index");
  if (!index)
    return false;

  let where = whereToOpenLink(aEvent);

  if (where == "current") {
    // Normal click. Go there in the current tab and update session history.

    try {
      gBrowser.gotoIndex(index);
    }
    catch(ex) {
      return false;
    }
    return true;
  }
  // Modified click. Go there in a new tab/window.

  duplicateTabIn(gBrowser.selectedTab, where, index - gBrowser.sessionHistory.index);
  return true;
}

function BrowserForward(aEvent) {
  let where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      gBrowser.goForward();
    }
    catch(ex) {
    }
  }
  else {
    duplicateTabIn(gBrowser.selectedTab, where, 1);
  }
}

function BrowserBack(aEvent) {
  let where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      gBrowser.goBack();
    }
    catch(ex) {
    }
  }
  else {
    duplicateTabIn(gBrowser.selectedTab, where, -1);
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

function BrowserStop() {
  const stopFlags = nsIWebNavigation.STOP_ALL;
  gBrowser.webNavigation.stop(stopFlags);
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

  let where = whereToOpenLink(aEvent, false, true);
  if (where == "current")
    BrowserReload();
  else
    duplicateTabIn(gBrowser.selectedTab, where);
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

var BrowserHome = BrowserGoHome;
function BrowserGoHome(aEvent) {
  if (aEvent && "button" in aEvent &&
      aEvent.button == 2) // right-click: do nothing
    return;

  var homePage = gHomeButton.getHomePage();
  var where = whereToOpenLink(aEvent, false, true);
  var urls;

  // Home page should open in a new tab when current tab is an app tab
  if (where == "current" &&
      gBrowser &&
      gBrowser.selectedTab.pinned)
    where = "tab";

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

function focusAndSelectUrlBar() {
  if (gURLBar) {
    if (window.fullScreen)
      FullScreen.showNavToolbox();

    gURLBar.select();
    if (document.activeElement == gURLBar.inputField)
      return true;
  }
  return false;
}

function openLocation() {
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
      window.openDialog("chrome://browser/content/", "_blank",
                        "chrome,all,dialog=no", BROWSER_NEW_TAB_URL);
    }
  }
#endif
}

function BrowserOpenTab()
{
  openUILinkIn(BROWSER_NEW_TAB_URL, "tab");
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
  // the callback function would confuse gBrowserInit.onLoad() by making
  // window.arguments[1] be an integer instead of null.
  setTimeout(function() { openDialog(chrome, "_blank", flags, href, null, null, postData); }, 10);
}

/* Required because the tab needs time to set up its content viewers and get the load of
   the URI kicked off before becoming the active content area. */
function delayedOpenTab(aUrl, aReferrer, aCharset, aPostData, aAllowThirdPartyFixup)
{
  gBrowser.loadOneTab(aUrl, {
                      referrerURI: aReferrer,
                      charset: aCharset,
                      postData: aPostData,
                      inBackground: false,
                      allowThirdPartyFixup: aAllowThirdPartyFixup});
}

var gLastOpenDirectory = {
  _lastDir: null,
  get path() {
    if (!this._lastDir || !this._lastDir.exists()) {
      try {
        this._lastDir = gPrefService.getComplexValue("browser.open.lastDir",
                                                     Ci.nsILocalFile);
        if (!this._lastDir.exists())
          this._lastDir = null;
      }
      catch(e) {}
    }
    return this._lastDir;
  },
  set path(val) {
    try {
      if (!val || !val.isDirectory())
        return;
    } catch(e) {
      return;
    }
    this._lastDir = val.clone();

    // Don't save the last open directory pref inside the Private Browsing mode
    if (!PrivateBrowsingUtils.isWindowPrivate(window))
      gPrefService.setComplexValue("browser.open.lastDir", Ci.nsILocalFile,
                                   this._lastDir);
  },
  reset: function() {
    this._lastDir = null;
  }
};

function BrowserOpenFileWindow()
{
  // Get filepicker component.
  try {
    const nsIFilePicker = Ci.nsIFilePicker;
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult == nsIFilePicker.returnOK) {
        try {
          if (fp.file) {
            gLastOpenDirectory.path =
              fp.file.parent.QueryInterface(Ci.nsILocalFile);
          }
        } catch (ex) {
        }
        openUILinkIn(fp.fileURL.spec, "current");
      }
    };

    fp.init(window, gNavigatorBundle.getString("openFile"),
            nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText |
                     nsIFilePicker.filterImages | nsIFilePicker.filterXML |
                     nsIFilePicker.filterHTML);
    fp.displayDirectory = gLastOpenDirectory.path;
    fp.open(fpCallback);
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
  gBrowser.removeCurrentTab({animate: true});
}

function BrowserTryToCloseWindow()
{
  if (WindowIsClosing())
    window.close();     // WindowIsClosing does all the necessary checks
}

function loadURI(uri, referrer, postData, allowThirdPartyFixup, referrerPolicy) {
  try {
    openLinkIn(uri, "current",
               { referrerURI: referrer,
                 referrerPolicy: referrerPolicy,
                 postData: postData,
                 allowThirdPartyFixup: allowThirdPartyFixup });
  } catch (e) {}
}

/**
 * Given a urlbar value, discerns between URIs, keywords and aliases.
 *
 * @param url
 *        The urlbar value.
 * @param callback (optional, deprecated)
 *        The callback function invoked when done. This parameter is
 *        deprecated, please use the Promise that is returned.
 *
 * @return Promise<{ postData, url, mayInheritPrincipal }>
 */
function getShortcutOrURIAndPostData(url, callback = null) {
  if (callback) {
    Deprecated.warning("Please use the Promise returned by " +
                       "getShortcutOrURIAndPostData() instead of passing a " +
                       "callback",
                       "https://bugzilla.mozilla.org/show_bug.cgi?id=1100294");
  }

  return Task.spawn(function* () {
    let mayInheritPrincipal = false;
    let postData = null;
    let shortcutURL = null;
    let keyword = url;
    let param = "";

    let offset = url.indexOf(" ");
    if (offset > 0) {
      keyword = url.substr(0, offset);
      param = url.substr(offset + 1);
    }

    let engine = Services.search.getEngineByAlias(keyword);
    if (engine) {
      let submission = engine.getSubmission(param, null, "keyword");
      postData = submission.postData;
      return { postData: submission.postData, url: submission.uri.spec,
               mayInheritPrincipal };
    }

    let entry = yield PlacesUtils.keywords.fetch(keyword);
    if (entry) {
      shortcutURL = entry.url.href;
      postData = entry.postData;
    }

    if (!shortcutURL) {
      return { postData, url, mayInheritPrincipal };
    }

    let escapedPostData = "";
    if (postData)
      escapedPostData = unescape(postData);

    if (/%s/i.test(shortcutURL) || /%s/i.test(escapedPostData)) {
      let charset = "";
      const re = /^(.*)\&mozcharset=([a-zA-Z][_\-a-zA-Z0-9]+)\s*$/;
      let matches = shortcutURL.match(re);

      if (matches) {
        [, shortcutURL, charset] = matches;
      } else {
        let uri;
        try {
          // makeURI() throws if URI is invalid.
          uri = makeURI(shortcutURL);
        } catch (ex) {}

        if (uri) {
          // Try to get the saved character-set.
          // Will return an empty string if character-set is not found.
          charset = yield PlacesUtils.getCharsetForURI(uri);
        }
      }

      // encodeURIComponent produces UTF-8, and cannot be used for other charsets.
      // escape() works in those cases, but it doesn't uri-encode +, @, and /.
      // Therefore we need to manually replace these ASCII characters by their
      // encodeURIComponent result, to match the behavior of nsEscape() with
      // url_XPAlphas
      let encodedParam = "";
      if (charset && charset != "UTF-8")
        encodedParam = escape(convertFromUnicode(charset, param)).
                       replace(/[+@\/]+/g, encodeURIComponent);
      else // Default charset is UTF-8
        encodedParam = encodeURIComponent(param);

      shortcutURL = shortcutURL.replace(/%s/g, encodedParam).replace(/%S/g, param);

      if (/%s/i.test(escapedPostData)) // POST keyword
        postData = getPostDataStream(escapedPostData, param, encodedParam,
                                               "application/x-www-form-urlencoded");

      // This URL came from a bookmark, so it's safe to let it inherit the current
      // document's principal.
      mayInheritPrincipal = true;

      return { postData, url: shortcutURL, mayInheritPrincipal };
    }

    if (param) {
      // This keyword doesn't take a parameter, but one was provided. Just return
      // the original URL.
      postData = null;

      return { postData, url, mayInheritPrincipal };
    }

    // This URL came from a bookmark, so it's safe to let it inherit the current
    // document's principal.
    mayInheritPrincipal = true;

    return { postData, url: shortcutURL, mayInheritPrincipal };
  }).then(data => {
    if (callback) {
      callback(data);
    }

    return data;
  });
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

function getLoadContext() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsILoadContext);
}

function readFromClipboard()
{
  var url;

  try {
    // Create transferable that will transfer the text.
    var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);
    trans.init(getLoadContext());

    trans.addDataFlavor("text/unicode");

    // If available, use selection clipboard, otherwise global one
    if (Services.clipboard.supportsSelectionClipboard())
      Services.clipboard.getData(trans, Services.clipboard.kSelectionClipboard);
    else
      Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);

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

/**
 * Open the View Source dialog.
 *
 * @param aArgsOrDocument
 *        Either an object or a Document. Passing a Document is deprecated,
 *        and is not supported with e10s. This function will throw if
 *        aArgsOrDocument is a CPOW.
 *
 *        If aArgsOrDocument is an object, that object can take the
 *        following properties:
 *
 *        browser:
 *          The browser containing the document that we would like to view the
 *          source of.
 *        URL:
 *          A string URL for the page we'd like to view the source of.
 *        outerWindowID (optional):
 *          The outerWindowID of the content window containing the document that
 *          we want to view the source of. You only need to provide this if you
 *          want to attempt to retrieve the document source from the network
 *          cache.
 *        lineNumber (optional):
 *          The line number to focus on once the source is loaded.
 */
function BrowserViewSourceOfDocument(aArgsOrDocument) {
  let args;

  if (aArgsOrDocument instanceof Document) {
    let doc = aArgsOrDocument;
    // Deprecated API - callers should pass args object instead.
    if (Cu.isCrossProcessWrapper(doc)) {
      throw new Error("BrowserViewSourceOfDocument cannot accept a CPOW " +
                      "as a document.");
    }

    let requestor = doc.defaultView
                       .QueryInterface(Ci.nsIInterfaceRequestor);
    let browser = requestor.getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler;
    let outerWindowID = requestor.getInterface(Ci.nsIDOMWindowUtils)
                                 .outerWindowID;
    let URL = browser.currentURI.spec;
    args = { browser, outerWindowID, URL };
  } else {
    args = aArgsOrDocument;
  }

  top.gViewSourceUtils.viewSource(args);
}

/**
 * Opens the View Source dialog for the source loaded in the root
 * top-level document of the browser. This is really just a
 * convenience wrapper around BrowserViewSourceOfDocument.
 *
 * @param browser
 *        The browser that we want to load the source of.
 */
function BrowserViewSource(browser) {
  BrowserViewSourceOfDocument({
    browser: browser,
    outerWindowID: browser.outerWindowID,
    URL: browser.currentURI.spec,
  });
}

// doc - document to use for source, or null for this window's document
// initialTab - name of the initial tab to display, or null for the first tab
// imageElement - image to load in the Media Tab of the Page Info window; can be null/omitted
function BrowserPageInfo(doc, initialTab, imageElement) {
  var args = {doc: doc, initialTab: initialTab, imageElement: imageElement};
  var windows = Services.wm.getEnumerator("Browser:page-info");

  var documentURL = doc ? doc.location : window.gBrowser.selectedBrowser.contentDocumentAsCPOW.location;

  // Check for windows matching the url
  while (windows.hasMoreElements()) {
    var currentWindow = windows.getNext();
    if (currentWindow.closed) {
      continue;
    }
    if (currentWindow.document.documentElement.getAttribute("relatedUrl") == documentURL) {
      currentWindow.focus();
      currentWindow.resetPageInfo(args);
      return currentWindow;
    }
  }

  // We didn't find a matching window, so open a new one.
  return openDialog("chrome://browser/content/pageinfo/pageInfo.xul", "",
                    "chrome,toolbar,dialog=no,resizable", args);
}

function URLBarSetURI(aURI) {
  var value = gBrowser.userTypedValue;
  var valid = false;

  if (value == null) {
    let uri = aURI || gBrowser.currentURI;
    // Strip off "wyciwyg://" and passwords for the location bar
    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}

    // Replace initial page URIs with an empty string
    // only if there's no opener (bug 370555).
    if (gInitialPages.indexOf(uri.spec) != -1)
      value = gBrowser.selectedBrowser.hasContentOpener ? uri.spec : "";
    else
      value = losslessDecodeURI(uri);

    valid = !isBlankPageURL(uri.spec);
  }

  gURLBar.value = value;
  gURLBar.valueIsTyped = !valid;
  SetPageProxyState(valid ? "valid" : "invalid");
}

function losslessDecodeURI(aURI) {
  if (aURI.schemeIs("moz-action"))
    throw new Error("losslessDecodeURI should never get a moz-action URI");

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

  // Encode invisible characters (C0/C1 control characters, U+007F [DEL],
  // U+00A0 [no-break space], line and paragraph separator,
  // object replacement character) (bug 452979, bug 909264)
  value = value.replace(/[\u0000-\u001f\u007f-\u00a0\u2028\u2029\ufffc]/g,
                        encodeURIComponent);

  // Encode default ignorable characters (bug 546013)
  // except ZWNJ (U+200C) and ZWJ (U+200D) (bug 582186).
  // This includes all bidirectional formatting characters.
  // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
  value = value.replace(/[\u00ad\u034f\u061c\u115f-\u1160\u17b4-\u17b5\u180b-\u180d\u200b\u200e-\u200f\u202a-\u202e\u2060-\u206f\u3164\ufe00-\ufe0f\ufeff\uffa0\ufff0-\ufff8]|\ud834[\udd73-\udd7a]|[\udb40-\udb43][\udc00-\udfff]/g,
                        encodeURIComponent);
  return value;
}

function UpdateUrlbarSearchSplitterState()
{
  var splitter = document.getElementById("urlbar-search-splitter");
  var urlbar = document.getElementById("urlbar-container");
  var searchbar = document.getElementById("search-container");

  if (document.documentElement.getAttribute("customizing") == "true") {
    if (splitter) {
      splitter.remove();
    }
    return;
  }

  // If the splitter is already in the right place, we don't need to do anything:
  if (splitter &&
      ((splitter.nextSibling == searchbar && splitter.previousSibling == urlbar) ||
       (splitter.nextSibling == urlbar && splitter.previousSibling == searchbar))) {
    return;
  }

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
      splitter.setAttribute("skipintoolbarset", "true");
      splitter.setAttribute("overflows", "false");
      splitter.className = "chromeclass-toolbar-additional";
    }
    urlbar.parentNode.insertBefore(splitter, ibefore);
  } else if (splitter)
    splitter.parentNode.removeChild(splitter);
}

function UpdatePageProxyState()
{
  if (gURLBar && gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid");
}

function SetPageProxyState(aState)
{
  BookmarkingUI.onPageProxyStateChanged(aState);
  ReadingListUI.onPageProxyStateChanged(aState);

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
  } else if (aState == "invalid") {
    gURLBar.removeEventListener("input", UpdatePageProxyState, false);
  }
}

function PageProxyClickHandler(aEvent)
{
  if (aEvent.button == 1 && gPrefService.getBoolPref("middlemouse.paste"))
    middleMousePaste(aEvent);
}

// Setup the hamburger button badges for updates, if enabled.
let gMenuButtonUpdateBadge = {
  enabled: false,

  init: function () {
    try {
      this.enabled = Services.prefs.getBoolPref("app.update.badge");
    } catch (e) {}
    if (this.enabled) {
      PanelUI.menuButton.classList.add("badged-button");
      Services.obs.addObserver(this, "update-staged", false);
    }
  },

  uninit: function () {
    if (this.enabled) {
      Services.obs.removeObserver(this, "update-staged");
      PanelUI.panel.removeEventListener("popupshowing", this, true);
      this.enabled = false;
    }
  },

  onMenuPanelCommand: function(event) {
    if (event.originalTarget.getAttribute("update-status") === "succeeded") {
      // restart the app
      let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                       .createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

      if (!cancelQuit.data) {
        Services.startup.quit(Services.startup.eAttemptQuit | Services.startup.eRestart);
      }
    } else {
      // open the page for manual update
      let url = Services.urlFormatter.formatURLPref("app.update.url.manual");
      openUILinkIn(url, "tab");
    }
  },

  observe: function (subject, topic, status) {
    const STATE_DOWNLOADING     = "downloading";
    const STATE_PENDING         = "pending";
    const STATE_PENDING_SVC     = "pending-service";
    const STATE_APPLIED         = "applied";
    const STATE_APPLIED_SVC     = "applied-service";
    const STATE_FAILED          = "failed";

    let updateButton = document.getElementById("PanelUI-update-status");

    let updateButtonText;
    let stringId;

    // Update the UI when the background updater is finished.
    switch (status) {
      case STATE_APPLIED:
      case STATE_APPLIED_SVC:
      case STATE_PENDING:
      case STATE_PENDING_SVC:
        // If the update is successfully applied, or if the updater has fallen back
        // to non-staged updates, add a badge to the hamburger menu to indicate an
        // update will be applied once the browser restarts.
        PanelUI.menuButton.setAttribute("update-status", "succeeded");

        let brandBundle = document.getElementById("bundle_brand");
        let brandShortName = brandBundle.getString("brandShortName");
        stringId = "appmenu.restartNeeded.description";
        updateButtonText = gNavigatorBundle.getFormattedString(stringId,
                                                               [brandShortName]);

        updateButton.setAttribute("label", updateButtonText);
        updateButton.setAttribute("update-status", "succeeded");
        updateButton.hidden = false;

        PanelUI.panel.addEventListener("popupshowing", this, true);

        break;
      case STATE_FAILED:
        // Background update has failed, let's show the UI responsible for
        // prompting the user to update manually.
        PanelUI.menuButton.setAttribute("update-status", "failed");
        PanelUI.menuButton.setAttribute("badge", "!");

        stringId = "appmenu.updateFailed.description";
        updateButtonText = gNavigatorBundle.getString(stringId);

        updateButton.setAttribute("label", updateButtonText);
        updateButton.setAttribute("update-status", "failed");
        updateButton.hidden = false;

        PanelUI.panel.addEventListener("popupshowing", this, true);

        break;
      case STATE_DOWNLOADING:
        // We've fallen back to downloading the full update because the partial
        // update failed to get staged in the background. Therefore we need to keep
        // our observer.
        return;
    }
    this.uninit();
  },

  handleEvent: function(e) {
    if (e.type === "popupshowing") {
      PanelUI.menuButton.removeAttribute("badge");
      PanelUI.panel.removeEventListener("popupshowing", this, true);
    }
  }
};

/**
 * Handle command events bubbling up from error page content
 * or from about:newtab or from remote error pages that invoke
 * us via async messaging.
 */
let BrowserOnClick = {
  init: function () {
    let mm = window.messageManager;
    mm.addMessageListener("Browser:CertExceptionError", this);
    mm.addMessageListener("Browser:SiteBlockedError", this);
    mm.addMessageListener("Browser:EnableOnlineMode", this);
    mm.addMessageListener("Browser:SendSSLErrorReport", this);
    mm.addMessageListener("Browser:SetSSLErrorReportAuto", this);
  },

  uninit: function () {
    let mm = window.messageManager;
    mm.removeMessageListener("Browser:CertExceptionError", this);
    mm.removeMessageListener("Browser:SiteBlockedError", this);
    mm.removeMessageListener("Browser:EnableOnlineMode", this);
    mm.removeMessageListener("Browser:SendSSLErrorReport", this);
    mm.removeMessageListener("Browser:SetSSLErrorReportAuto", this);
  },

  handleEvent: function (event) {
    if (!event.isTrusted || // Don't trust synthetic events
        event.button == 2) {
      return;
    }

    let originalTarget = event.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    if (gMultiProcessBrowser &&
        ownerDoc.documentURI.toLowerCase() == "about:newtab") {
      this.onE10sAboutNewTab(event, ownerDoc);
    }
  },

  receiveMessage: function (msg) {
    switch (msg.name) {
      case "Browser:CertExceptionError":
        this.onAboutCertError(msg.target, msg.data.elementId,
                              msg.data.isTopFrame, msg.data.location,
                              msg.data.sslStatusAsString);
      break;
      case "Browser:SiteBlockedError":
        this.onAboutBlocked(msg.data.elementId, msg.data.reason,
                            msg.data.isTopFrame, msg.data.location);
      break;
      case "Browser:EnableOnlineMode":
        if (Services.io.offline) {
          // Reset network state and refresh the page.
          Services.io.offline = false;
          msg.target.reload();
        }
      break;
      case "Browser:SendSSLErrorReport":
        this.onSSLErrorReport(msg.target, msg.data.elementId,
                              msg.data.documentURI,
                              msg.data.location,
                              msg.data.securityInfo);
      break;
      case "Browser:SetSSLErrorReportAuto":
        Services.prefs.setBoolPref("security.ssl.errorReporting.automatic", msg.json.automatic);
      break;
    }
  },

  onSSLErrorReport: function(browser, elementId, documentURI, location, securityInfo) {
    function showReportStatus(reportStatus) {
      gBrowser.selectedBrowser
          .messageManager
          .sendAsyncMessage("Browser:SSLErrorReportStatus",
                            {
                              reportStatus: reportStatus,
                              documentURI: documentURI
                            });
    }

    if (!Services.prefs.getBoolPref("security.ssl.errorReporting.enabled")) {
      showReportStatus("error");
      Cu.reportError("User requested certificate error report sending, but certificate error reporting is disabled");
      return;
    }

    let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                           .getService(Ci.nsISerializationHelper);
    let transportSecurityInfo = serhelper.deserializeObject(securityInfo);
    transportSecurityInfo.QueryInterface(Ci.nsITransportSecurityInfo)

    showReportStatus("activity");

    /*
     * Requested info for the report:
     * - Domain of bad connection
     * - Error type (e.g. Pinning, domain mismatch, etc)
     * - Cert chain (at minimum, same data to distrust each cert in the
     *   chain)
     * - Request data (e.g. User Agent, IP, Timestamp)
     *
     * The request data should be added to the report by the receiving server.
     */

    // TODO: can we pull this in from pippki.js isntead of duplicating it
    // here?
    function getDERString(cert)
    {
      var length = {};
      var derArray = cert.getRawDER(length);
      var derString = '';
      for (var i = 0; i < derArray.length; i++) {
        derString += String.fromCharCode(derArray[i]);
      }
      return derString;
    }

    // Convert the nsIX509CertList into a format that can be parsed into
    // JSON
    let asciiCertChain = [];

    if (transportSecurityInfo.failedCertChain) {
      let certs = transportSecurityInfo.failedCertChain.getEnumerator();
      while (certs.hasMoreElements()) {
        let cert = certs.getNext();
        cert.QueryInterface(Ci.nsIX509Cert);
        asciiCertChain.push(btoa(getDERString(cert)));
      }
    }

    let report = {
      hostname: location.hostname,
      port: location.port,
      timestamp: Math.round(Date.now() / 1000),
      errorCode: transportSecurityInfo.errorCode,
      failedCertChain: asciiCertChain,
      userAgent: window.navigator.userAgent,
      version: 1,
      build: gAppInfo.appBuildID,
      product: gAppInfo.name,
      channel: UpdateChannel.get()
    }

    let reportURL = Services.prefs.getCharPref("security.ssl.errorReporting.url");

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
        .createInstance(Ci.nsIXMLHttpRequest);
    try {
      xhr.open("POST", reportURL);
    } catch (e) {
      Cu.reportError("xhr.open exception", e);
      showReportStatus("error");
    }

    xhr.onerror = function (e) {
      // error making request to reportURL
      Cu.reportError("xhr onerror", e);
      showReportStatus("error");
    };

    xhr.onload = function (event) {
      if (xhr.status !== 201 && xhr.status !== 0) {
        // request returned non-success status
        Cu.reportError("xhr returned failure code", xhr.status);
        showReportStatus("error");
      } else {
        showReportStatus("complete");
      }
    };

    xhr.send(JSON.stringify(report));
  },

  onAboutCertError: function (browser, elementId, isTopFrame, location, sslStatusAsString) {
    let secHistogram = Services.telemetry.getHistogramById("SECURITY_UI");

    switch (elementId) {
      case "exceptionDialogButton":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_CLICK_ADD_EXCEPTION);
        }

        let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                           .getService(Ci.nsISerializationHelper);
        let sslStatus = serhelper.deserializeObject(sslStatusAsString);
        sslStatus.QueryInterface(Components.interfaces.nsISSLStatus);
        let params = { exceptionAdded : false,
                       sslStatus : sslStatus };

        try {
          switch (Services.prefs.getIntPref("browser.ssl_override_behavior")) {
            case 2 : // Pre-fetch & pre-populate
              params.prefetchCert = true;
            case 1 : // Pre-populate
              params.location = location;
          }
        } catch (e) {
          Components.utils.reportError("Couldn't get ssl_override pref: " + e);
        }

        window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                          '','chrome,centerscreen,modal', params);

        // If the user added the exception cert, attempt to reload the page
        if (params.exceptionAdded) {
          browser.reload();
        }
        break;

      case "getMeOutOfHereButton":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_GET_ME_OUT_OF_HERE);
        }
        getMeOutOfHere();
        break;

      case "technicalContent":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_TECHNICAL_DETAILS);
        }
        break;

      case "expertContent":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_UNDERSTAND_RISKS);
        }
        break;

    }
  },

  onAboutBlocked: function (elementId, reason, isTopFrame, location) {
    // Depending on what page we are displaying here (malware/phishing/unwanted)
    // use the right strings and links for each.
    let bucketName = "WARNING_PHISHING_PAGE_";
    if (reason === 'malware') {
      bucketName = "WARNING_MALWARE_PAGE_";
    } else if (reason === 'unwanted') {
      bucketName = "WARNING_UNWANTED_PAGE_";
    }
    let secHistogram = Services.telemetry.getHistogramById("SECURITY_UI");
    let nsISecTel = Ci.nsISecurityUITelemetry;
    bucketName += isTopFrame ? "TOP_" : "FRAME_";
    switch (elementId) {
      case "getMeOutButton":
        secHistogram.add(nsISecTel[bucketName + "GET_ME_OUT_OF_HERE"]);
        getMeOutOfHere();
        break;

      case "reportButton":
        // This is the "Why is this site blocked" button. We redirect
        // to the generic page describing phishing/malware protection.

        // We log even if malware/phishing/unwanted info URL couldn't be found:
        // the measurement is for how many users clicked the WHY BLOCKED button
        secHistogram.add(nsISecTel[bucketName + "WHY_BLOCKED"]);

        openHelpLink("phishing-malware", false, "current");
        break;

      case "ignoreWarningButton":
        secHistogram.add(nsISecTel[bucketName + "IGNORE_WARNING"]);
        this.ignoreWarningButton(reason);
        break;
    }
  },

  /**
   * This functions prevents navigation from happening directly through the <a>
   * link in about:newtab (which is loaded in the parent and therefore would load
   * the next page also in the parent) and instructs the browser to open the url
   * in the current tab which will make it update the remoteness of the tab.
   */
  onE10sAboutNewTab: function(event, ownerDoc) {
    let isTopFrame = (ownerDoc.defaultView.parent === ownerDoc.defaultView);
    if (!isTopFrame) {
      return;
    }

    let anchorTarget = event.originalTarget.parentNode;

    if (anchorTarget instanceof HTMLAnchorElement &&
        anchorTarget.classList.contains("newtab-link")) {
      event.preventDefault();
      let where = whereToOpenLink(event, false, false);
      openLinkIn(anchorTarget.href, where, { charset: ownerDoc.characterSet });
    }
  },

  ignoreWarningButton: function (reason) {
    // Allow users to override and continue through to the site,
    // but add a notify bar as a reminder, so that they don't lose
    // track after, e.g., tab switching.
    gBrowser.loadURIWithFlags(gBrowser.currentURI.spec,
                              nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
                              null, null, null);

    Services.perms.add(gBrowser.currentURI, "safe-browsing",
                       Ci.nsIPermissionManager.ALLOW_ACTION,
                       Ci.nsIPermissionManager.EXPIRE_SESSION);

    let buttons = [{
      label: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.label"),
      accessKey: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.accessKey"),
      callback: function() { getMeOutOfHere(); }
    }];

    let title;
    if (reason === 'malware') {
      title = gNavigatorBundle.getString("safebrowsing.reportedAttackSite");
      buttons[1] = {
        label: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.label"),
        accessKey: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.accessKey"),
        callback: function() {
          openUILinkIn(gSafeBrowsing.getReportURL('MalwareError'), 'tab');
        }
      };
    } else if (reason === 'phishing') {
      title = gNavigatorBundle.getString("safebrowsing.reportedWebForgery");
      buttons[1] = {
        label: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.label"),
        accessKey: gNavigatorBundle.getString("safebrowsing.notAForgeryButton.accessKey"),
        callback: function() {
          openUILinkIn(gSafeBrowsing.getReportURL('Error'), 'tab');
        }
      };
    } else if (reason === 'unwanted') {
      title = gNavigatorBundle.getString("safebrowsing.reportedUnwantedSite");
      // There is no button for reporting errors since Google doesn't currently
      // provide a URL endpoint for these reports.
    }

    let notificationBox = gBrowser.getNotificationBox();
    let value = "blocked-badware-page";

    let previousNotification = notificationBox.getNotificationWithValue(value);
    if (previousNotification) {
      notificationBox.removeNotification(previousNotification);
    }

    let notification = notificationBox.appendNotification(
      title,
      value,
      "chrome://global/skin/icons/blacklist_favicon.png",
      notificationBox.PRIORITY_CRITICAL_HIGH,
      buttons
    );
    // Persist the notification until the user removes so it
    // doesn't get removed on redirects.
    notification.persistence = -1;
  },
};

/**
 * Re-direct the browser to a known-safe page.  This function is
 * used when, for example, the user browses to a known malware page
 * and is presented with about:blocked.  The "Get me out of here!"
 * button should take the user to the default start page so that even
 * when their own homepage is infected, we can get them somewhere safe.
 */
function getMeOutOfHere() {
  // Get the start page from the *default* pref branch, not the user's
  var prefs = Services.prefs.getDefaultBranch(null);
  var url = BROWSER_NEW_TAB_URL;
  try {
    url = prefs.getComplexValue("browser.startup.homepage",
                                Ci.nsIPrefLocalizedString).data;
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.includes("|"))
      url = url.split("|")[0];
  } catch(e) {
    Components.utils.reportError("Couldn't get homepage pref: " + e);
  }
  gBrowser.loadURI(url);
}

function BrowserFullScreen()
{
  window.fullScreen = !window.fullScreen;
}

function mirrorShow(popup) {
  let services = [];
  if (Services.prefs.getBoolPref("browser.casting.enabled")) {
    services = CastingApps.getServicesForMirroring();
  }
  popup.ownerDocument.getElementById("menu_mirrorTabCmd").hidden = !services.length;
}

function mirrorMenuItemClicked(event) {
  gBrowser.selectedBrowser.messageManager.sendAsyncMessage("SecondScreen:tab-mirror",
                                                           {service: event.originalTarget._service});
}

function populateMirrorTabMenu(popup) {
  popup.innerHTML = null;
  if (!Services.prefs.getBoolPref("browser.casting.enabled")) {
    return;
  }
  let videoEl = this.target;
  let doc = popup.ownerDocument;
  let services = CastingApps.getServicesForMirroring();
  services.forEach(service => {
    let item = doc.createElement("menuitem");
    item.setAttribute("label", service.friendlyName);
    item._service = service;
    item.addEventListener("command", mirrorMenuItemClicked);
    popup.appendChild(item);
  });
};

function getWebNavigation()
{
  return gBrowser.webNavigation;
}

function BrowserReloadWithFlags(reloadFlags) {
  let url = gBrowser.currentURI.spec;
  if (gBrowser.updateBrowserRemotenessByURL(gBrowser.selectedBrowser, url)) {
    // If the remoteness has changed, the new browser doesn't have any
    // information of what was loaded before, so we need to load the previous
    // URL again.
    gBrowser.loadURIWithFlags(url, reloadFlags);
    return;
  }

  let windowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);

  gBrowser.selectedBrowser
          .messageManager
          .sendAsyncMessage("Browser:Reload",
                            { flags: reloadFlags,
                              handlingUserInput: windowUtils.isHandlingUserInput });
}

var PrintPreviewListener = {
  _printPreviewTab: null,
  _tabBeforePrintPreview: null,

  getPrintPreviewBrowser: function () {
    if (!this._printPreviewTab) {
      this._tabBeforePrintPreview = gBrowser.selectedTab;
      this._printPreviewTab = gBrowser.loadOneTab("about:blank",
                                                  { inBackground: false });
      gBrowser.selectedTab = this._printPreviewTab;
    }
    return gBrowser.getBrowserForTab(this._printPreviewTab);
  },
  getSourceBrowser: function () {
    return this._tabBeforePrintPreview ?
      this._tabBeforePrintPreview.linkedBrowser : gBrowser.selectedBrowser;
  },
  getNavToolbox: function () {
    return gNavToolbox;
  },
  onEnter: function () {
    gInPrintPreviewMode = true;
    this._toggleAffectedChrome();
  },
  onExit: function () {
    gBrowser.selectedTab = this._tabBeforePrintPreview;
    this._tabBeforePrintPreview = null;
    gInPrintPreviewMode = false;
    this._toggleAffectedChrome();
    gBrowser.removeTab(this._printPreviewTab);
    this._printPreviewTab = null;
  },
  _toggleAffectedChrome: function () {
    gNavToolbox.collapsed = gInPrintPreviewMode;

    if (gInPrintPreviewMode)
      this._hideChrome();
    else
      this._showChrome();

    TabsInTitlebar.allowedBy("print-preview", !gInPrintPreviewMode);
  },
  _hideChrome: function () {
    this._chromeState = {};

    this._chromeState.sidebarOpen = SidebarUI.isOpen;
    this._sidebarCommand = SidebarUI.currentID;
    SidebarUI.hide();

    var notificationBox = gBrowser.getNotificationBox();
    this._chromeState.notificationsOpen = !notificationBox.notificationsHidden;
    notificationBox.notificationsHidden = true;

    gBrowser.updateWindowResizers();

    this._chromeState.findOpen = gFindBarInitialized && !gFindBar.hidden;
    if (gFindBarInitialized)
      gFindBar.close();

    var globalNotificationBox = document.getElementById("global-notificationbox");
    this._chromeState.globalNotificationsOpen = !globalNotificationBox.notificationsHidden;
    globalNotificationBox.notificationsHidden = true;

    this._chromeState.syncNotificationsOpen = false;
    var syncNotifications = document.getElementById("sync-notifications");
    if (syncNotifications) {
      this._chromeState.syncNotificationsOpen = !syncNotifications.notificationsHidden;
      syncNotifications.notificationsHidden = true;
    }
  },
  _showChrome: function () {
    if (this._chromeState.notificationsOpen)
      gBrowser.getNotificationBox().notificationsHidden = false;

    if (this._chromeState.findOpen)
      gFindBar.open();

    if (this._chromeState.globalNotificationsOpen)
      document.getElementById("global-notificationbox").notificationsHidden = false;

    if (this._chromeState.syncNotificationsOpen)
      document.getElementById("sync-notifications").notificationsHidden = false;

    if (this._chromeState.sidebarOpen)
      SidebarUI.show(this._sidebarCommand);
  }
}

function getMarkupDocumentViewer()
{
  return gBrowser.markupDocumentViewer;
}

// This function is obsolete. Newer code should use <tooltip page="true"/> instead.
function FillInHTMLTooltip(tipElement)
{
  document.getElementById("aHTMLTooltip").fillInPageTooltip(tipElement);
}

var browserDragAndDrop = {
  canDropLink: function (aEvent) Services.droppedLinkHandler.canDropLink(aEvent, true),

  dragOver: function (aEvent)
  {
    if (this.canDropLink(aEvent)) {
      aEvent.preventDefault();
    }
  },

  drop: function (aEvent, aName, aDisallowInherit) {
    return Services.droppedLinkHandler.dropLink(aEvent, aName, aDisallowInherit);
  }
};

var homeButtonObserver = {
  onDrop: function (aEvent)
    {
      // disallow setting home pages that inherit the principal
      let url = browserDragAndDrop.drop(aEvent, {}, true);
      setTimeout(openHomeDialog, 0, url);
    },

  onDragOver: function (aEvent)
    {
      browserDragAndDrop.dragOver(aEvent);
      aEvent.dropEffect = "link";
    },
  onDragExit: function (aEvent)
    {
    }
}

function openHomeDialog(aURL)
{
  var promptTitle = gNavigatorBundle.getString("droponhometitle");
  var promptMsg   = gNavigatorBundle.getString("droponhomemsg");
  var pressedVal  = Services.prompt.confirmEx(window, promptTitle, promptMsg,
                          Services.prompt.STD_YES_NO_BUTTONS,
                          null, null, null, null, {value:0});

  if (pressedVal == 0) {
    try {
      var str = Components.classes["@mozilla.org/supports-string;1"]
                          .createInstance(Components.interfaces.nsISupportsString);
      str.data = aURL;
      gPrefService.setComplexValue("browser.startup.homepage",
                                   Components.interfaces.nsISupportsString, str);
    } catch (ex) {
      dump("Failed to set the home page.\n"+ex+"\n");
    }
  }
}

var newTabButtonObserver = {
  onDragOver: function (aEvent)
  {
    browserDragAndDrop.dragOver(aEvent);
  },

  onDragExit: function (aEvent)
  {
  },

  onDrop: function (aEvent)
  {
    let url = browserDragAndDrop.drop(aEvent, { });
    getShortcutOrURIAndPostData(url).then(data => {
      if (data.url) {
        // allow third-party services to fixup this URL
        openNewTabWith(data.url, null, data.postData, aEvent, true);
      }
    });
  }
}

var newWindowButtonObserver = {
  onDragOver: function (aEvent)
  {
    browserDragAndDrop.dragOver(aEvent);
  },
  onDragExit: function (aEvent)
  {
  },
  onDrop: function (aEvent)
  {
    let url = browserDragAndDrop.drop(aEvent, { });
    getShortcutOrURIAndPostData(url).then(data => {
      if (data.url) {
        // allow third-party services to fixup this URL
        openNewWindowWith(data.url, null, data.postData, true);
      }
    });
  }
}

const DOMLinkHandler = {
  init: function() {
    let mm = window.messageManager;
    mm.addMessageListener("Link:AddFeed", this);
    mm.addMessageListener("Link:SetIcon", this);
    mm.addMessageListener("Link:AddSearch", this);
  },

  receiveMessage: function (aMsg) {
    switch (aMsg.name) {
      case "Link:AddFeed":
        let link = {type: aMsg.data.type, href: aMsg.data.href, title: aMsg.data.title};
        FeedHandler.addFeed(link, aMsg.target);
        break;

      case "Link:SetIcon":
        return this.setIcon(aMsg.target, aMsg.data.url);
        break;

      case "Link:AddSearch":
        this.addSearch(aMsg.target, aMsg.data.engine, aMsg.data.url);
        break;
    }
  },

  setIcon: function(aBrowser, aURL) {
    if (gBrowser.isFailedIcon(aURL))
      return false;

    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab)
      return false;

    gBrowser.setIcon(tab, aURL);
    return true;
  },

  addSearch: function(aBrowser, aEngine, aURL) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab)
      return false;

    BrowserSearch.addEngine(aBrowser, aEngine, makeURI(aURL));
  },
}

const BrowserSearch = {
  addEngine: function(browser, engine, uri) {
    if (!this.searchBar)
      return;

    // Check to see whether we've already added an engine with this title
    if (browser.engines) {
      if (browser.engines.some(function (e) e.title == engine.title))
        return;
    }

    var hidden = false;
    // If this engine (identified by title) is already in the list, add it
    // to the list of hidden engines rather than to the main list.
    // XXX This will need to be changed when engines are identified by URL;
    // see bug 335102.
    if (Services.search.getEngineByName(engine.title))
      hidden = true;

    var engines = (hidden ? browser.hiddenEngines : browser.engines) || [];

    engines.push({ uri: engine.href,
                   title: engine.title,
                   get icon() { return browser.mIconURL; }
                 });

    if (hidden)
      browser.hiddenEngines = engines;
    else {
      browser.engines = engines;
      if (browser == gBrowser.selectedBrowser)
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

    var engines = gBrowser.selectedBrowser.engines;
    if (engines && engines.length > 0)
      searchBar.setAttribute("addengines", "true");
    else
      searchBar.removeAttribute("addengines");
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
        win.focus();
        win.BrowserSearch.webSearch();
      } else {
        // If there are no open browser windows, open a new one
        var observer = function observer(subject, topic, data) {
          if (subject == win) {
            BrowserSearch.webSearch();
            Services.obs.removeObserver(observer, "browser-delayed-startup-finished");
          }
        }
        win = window.openDialog(getBrowserURL(), "_blank",
                                "chrome,all,dialog=no", "about:blank");
        Services.obs.addObserver(observer, "browser-delayed-startup-finished", false);
      }
      return;
    }
#endif
    let openSearchPageIfFieldIsNotActive = function(aSearchBar) {
      if (!aSearchBar || document.activeElement != aSearchBar.textbox.inputField) {
        let url = gBrowser.currentURI.spec.toLowerCase();
        let mm = gBrowser.selectedBrowser.messageManager;
        if (url === "about:home") {
          AboutHome.focusInput(mm);
        } else if (url === "about:newtab" && NewTabUtils.allPages.enabled) {
          ContentSearch.focusInput(mm);
        } else {
          openUILinkIn("about:home", "current");
        }
      }
    };

    let searchBar = this.searchBar;
    let placement = CustomizableUI.getPlacementOfWidget("search-container");
    let focusSearchBar = () => {
      searchBar = this.searchBar;
      searchBar.select();
      openSearchPageIfFieldIsNotActive(searchBar);
    };
    if (placement && placement.area == CustomizableUI.AREA_PANEL) {
      // The panel is not constructed until the first time it is shown.
      PanelUI.show().then(focusSearchBar);
      return;
    }
    if (placement && placement.area == CustomizableUI.AREA_NAVBAR && searchBar &&
        searchBar.parentNode.getAttribute("overflowedItem") == "true") {
      let navBar = document.getElementById(CustomizableUI.AREA_NAVBAR);
      navBar.overflowable.show().then(() => {
        focusSearchBar();
      });
      return;
    }
    if (searchBar) {
      if (window.fullScreen)
        FullScreen.showNavToolbox();
      searchBar.select();
    }
    openSearchPageIfFieldIsNotActive(searchBar);
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
   *
   * @param purpose [optional]
   *        A string meant to indicate the context of the search request. This
   *        allows the search service to provide a different nsISearchSubmission
   *        depending on e.g. where the search is triggered in the UI.
   *
   * @return engine The search engine used to perform a search, or null if no
   *                search was performed.
   */
  _loadSearch: function (searchText, useNewTab, purpose) {
    let engine;

    // If the search bar is visible, use the current engine, otherwise, fall
    // back to the default engine.
    if (isElementVisible(this.searchBar))
      engine = Services.search.currentEngine;
    else
      engine = Services.search.defaultEngine;

    let submission = engine.getSubmission(searchText, null, purpose); // HTML response

    // getSubmission can return null if the engine doesn't have a URL
    // with a text/html response type.  This is unlikely (since
    // SearchService._addEngineToStore() should fail for such an engine),
    // but let's be on the safe side.
    if (!submission) {
      return null;
    }

    let inBackground = Services.prefs.getBoolPref("browser.search.context.loadInBackground");
    openLinkIn(submission.uri.spec,
               useNewTab ? "tab" : "current",
               { postData: submission.postData,
                 inBackground: inBackground,
                 relatedToCurrent: true });

    return engine;
  },

  /**
   * Just like _loadSearch, but preserving an old API.
   *
   * @return string Name of the search engine used to perform a search or null
   *         if a search was not performed.
   */
  loadSearch: function BrowserSearch_search(searchText, useNewTab, purpose) {
    let engine = BrowserSearch._loadSearch(searchText, useNewTab, purpose);
    if (!engine) {
      return null;
    }
    return engine.name;
  },

  /**
   * Perform a search initiated from the context menu.
   *
   * This should only be called from the context menu. See
   * BrowserSearch.loadSearch for the preferred API.
   */
  loadSearchFromContext: function (terms) {
    let engine = BrowserSearch._loadSearch(terms, true, "contextmenu");
    if (engine) {
      BrowserSearch.recordSearchInHealthReport(engine, "contextmenu");
    }
  },

  pasteAndSearch: function (event) {
    BrowserSearch.searchBar.select();
    goDoCommand("cmd_paste");
    BrowserSearch.searchBar.handleSearchCommand(event);
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
    var searchEnginesURL = formatURL("browser.search.searchEnginesURL", true);
    openUILinkIn(searchEnginesURL, where);
  },

  /**
   * Helper to record a search with Firefox Health Report.
   *
   * FHR records only search counts and nothing pertaining to the search itself.
   *
   * @param engine
   *        (nsISearchEngine) The engine handling the search.
   * @param source
   *        (string) Where the search originated from. See the FHR
   *        SearchesProvider for allowed values.
   * @param selection [optional]
   *        ({index: The selected index, kind: "key" or "mouse"}) If
   *        the search was a suggested search, this indicates where the
   *        item was in the suggestion list and how the user selected it.
   */
  recordSearchInHealthReport: function (engine, source, selection) {
    BrowserUITelemetry.countSearchEvent(source, null, selection);
    this.recordSearchInTelemetry(engine, source);
#ifdef MOZ_SERVICES_HEALTHREPORT
    let reporter = Cc["@mozilla.org/datareporting/service;1"]
                     .getService()
                     .wrappedJSObject
                     .healthReporter;

    // This can happen if the FHR component of the data reporting service is
    // disabled. This is controlled by a pref that most will never use.
    if (!reporter) {
      return;
    }

    reporter.onInit().then(function record() {
      try {
        reporter.getProvider("org.mozilla.searches").recordSearch(engine, source);
      } catch (ex) {
        Cu.reportError(ex);
      }
    });
#endif
  },

  _getSearchEngineId: function (engine) {
    if (!engine) {
      return "other";
    }

    if (engine.identifier) {
      return engine.identifier;
    }

    return "other-" + engine.name;
  },

  recordSearchInTelemetry: function (engine, source) {
    const SOURCES = [
      "abouthome",
      "contextmenu",
      "newtab",
      "searchbar",
      "urlbar",
    ];

    if (SOURCES.indexOf(source) == -1) {
      Cu.reportError("Unknown source for search: " + source);
      return;
    }

    let countId = this._getSearchEngineId(engine) + "." + source;

    let count = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
    count.add(countId);
  },

  recordOneoffSearchInTelemetry: function (engine, source, type, where) {
    let id = this._getSearchEngineId(engine) + "." + source;
    BrowserUITelemetry.countOneoffSearchEvent(id, type, where);
  }
};

function FillHistoryMenu(aParent) {
  // Lazily add the hover listeners on first showing and never remove them
  if (!aParent.hasStatusListener) {
    // Show history item's uri in the status bar when hovering, and clear on exit
    aParent.addEventListener("DOMMenuItemActive", function(aEvent) {
      // Only the current page should have the checked attribute, so skip it
      if (!aEvent.target.hasAttribute("checked"))
        XULBrowserWindow.setOverLink(aEvent.target.getAttribute("uri"));
    }, false);
    aParent.addEventListener("DOMMenuItemInactive", function() {
      XULBrowserWindow.setOverLink("");
    }, false);

    aParent.hasStatusListener = true;
  }

  // Remove old entries if any
  var children = aParent.childNodes;
  for (var i = children.length - 1; i >= 0; --i) {
    if (children[i].hasAttribute("index"))
      aParent.removeChild(children[i]);
  }

  var webNav = gBrowser.webNavigation;
  var sessionHistory = webNav.sessionHistory;

  var count = sessionHistory.count;
  if (count <= 1) // don't display the popup for a single item
    return false;

  const MAX_HISTORY_MENU_ITEMS = 15;
  var index = sessionHistory.index;
  var half_length = Math.floor(MAX_HISTORY_MENU_ITEMS / 2);
  var start = Math.max(index - half_length, 0);
  var end = Math.min(start == 0 ? MAX_HISTORY_MENU_ITEMS : index + half_length + 1, count);
  if (end == count)
    start = Math.max(count - MAX_HISTORY_MENU_ITEMS, 0);

  var tooltipBack = gNavigatorBundle.getString("tabHistory.goBack");
  var tooltipCurrent = gNavigatorBundle.getString("tabHistory.current");
  var tooltipForward = gNavigatorBundle.getString("tabHistory.goForward");

  for (var j = end - 1; j >= start; j--) {
    let item = document.createElement("menuitem");
    let entry = sessionHistory.getEntryAtIndex(j, false);
    let uri = entry.URI.spec;
    let entryURI = BrowserUtils.makeURIFromCPOW(entry.URI);

    item.setAttribute("uri", uri);
    item.setAttribute("label", entry.title || uri);
    item.setAttribute("index", j);

    if (j != index) {
      PlacesUtils.favicons.getFaviconURLForPage(entryURI, function (aURI) {
        if (aURI) {
          let iconURL = PlacesUtils.favicons.getFaviconLinkForIcon(aURI).spec;
          iconURL = PlacesUtils.getImageURLForResolution(window, iconURL);
          item.style.listStyleImage = "url(" + iconURL + ")";
        }
      });
    }

    if (j < index) {
      item.className = "unified-nav-back menuitem-iconic menuitem-with-favicon";
      item.setAttribute("tooltiptext", tooltipBack);
    } else if (j == index) {
      item.setAttribute("type", "radio");
      item.setAttribute("checked", "true");
      item.className = "unified-nav-current";
      item.setAttribute("tooltiptext", tooltipCurrent);
    } else {
      item.className = "unified-nav-forward menuitem-iconic menuitem-with-favicon";
      item.setAttribute("tooltiptext", tooltipForward);
    }

    aParent.appendChild(item);
  }
  return true;
}

function addToUrlbarHistory(aUrlToAdd) {
  if (!PrivateBrowsingUtils.isWindowPrivate(window) &&
      aUrlToAdd &&
      !aUrlToAdd.includes(" ") &&
      !/[\x00-\x1F]/.test(aUrlToAdd))
    PlacesUIUtils.markPageAsTyped(aUrlToAdd);
}

function toJavaScriptConsole()
{
  toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

function BrowserDownloadsUI()
{
  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    openUILinkIn("about:downloads", "tab");
  } else {
    PlacesCommandHook.showPlacesOrganizer("Downloads");
  }
}

function toOpenWindowByType(inType, uri, features)
{
  var topWindow = Services.wm.getMostRecentWindow(inType);

  if (topWindow)
    topWindow.focus();
  else if (features)
    window.open(uri, "_blank", features);
  else
    window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}

function OpenBrowserWindow(options)
{
  var telemetryObj = {};
  TelemetryStopwatch.start("FX_NEW_WINDOW_MS", telemetryObj);

  function newDocumentShown(doc, topic, data) {
    if (topic == "document-shown" &&
        doc != document &&
        doc.defaultView == win) {
      Services.obs.removeObserver(newDocumentShown, "document-shown");
      Services.obs.removeObserver(windowClosed, "domwindowclosed");
      TelemetryStopwatch.finish("FX_NEW_WINDOW_MS", telemetryObj);
    }
  }

  function windowClosed(subject) {
    if (subject == win) {
      Services.obs.removeObserver(newDocumentShown, "document-shown");
      Services.obs.removeObserver(windowClosed, "domwindowclosed");
    }
  }

  // Make sure to remove the 'document-shown' observer in case the window
  // is being closed right after it was opened to avoid leaking.
  Services.obs.addObserver(newDocumentShown, "document-shown", false);
  Services.obs.addObserver(windowClosed, "domwindowclosed", false);

  var charsetArg = new String();
  var handler = Components.classes["@mozilla.org/browser/clh;1"]
                          .getService(Components.interfaces.nsIBrowserHandler);
  var defaultArgs = handler.defaultArgs;
  var wintype = document.documentElement.getAttribute('windowtype');

  var extraFeatures = "";
  if (options && options.private) {
    extraFeatures = ",private";
    if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
      // Force the new window to load about:privatebrowsing instead of the default home page
      defaultArgs = "about:privatebrowsing";
    }
  } else {
    extraFeatures = ",non-private";
  }

  if (options && options.remote) {
    // If we're using remote tabs by default, then OMTC will be force-enabled,
    // despite the preference returning as false.
    let omtcEnabled = gPrefService.getBoolPref("layers.offmainthreadcomposition.enabled")
                      || Services.appinfo.browserTabsRemoteAutostart;
    if (!omtcEnabled) {
      alert("To use out-of-process tabs, you must set the layers.offmainthreadcomposition.enabled preference and restart. Opening a normal window instead.");
    } else {
      extraFeatures += ",remote";
    }
  } else if (options && options.remote === false) {
    extraFeatures += ",non-remote";
  }

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  var win;
  if (window && (wintype == "navigator:browser") && window.content && window.content.document)
  {
    var DocCharset = window.content.document.characterSet;
    charsetArg = "charset="+DocCharset;

    //we should "inherit" the charset menu setting in a new window
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no" + extraFeatures, defaultArgs, charsetArg);
  }
  else // forget about the charset information.
  {
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no" + extraFeatures, defaultArgs);
  }

  return win;
}

// Only here for backwards compat, we should remove this soon
function BrowserCustomizeToolbar() {
  gCustomizeMode.enter();
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
                   document.getElementById("edit-controls") ? true : false;

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

/**
 * Makes the Character Encoding menu enabled or disabled as appropriate.
 * To be called when the View menu or the app menu is opened.
 */
function updateCharacterEncodingMenuState()
{
  let charsetMenu = document.getElementById("charsetMenu");
  // gBrowser is null on Mac when the menubar shows in the context of
  // non-browser windows. The above elements may be null depending on
  // what parts of the menubar are present. E.g. no app menu on Mac.
  if (gBrowser && gBrowser.selectedBrowser.mayEnableCharacterEncodingMenu) {
    if (charsetMenu) {
      charsetMenu.removeAttribute("disabled");
    }
  } else {
    if (charsetMenu) {
      charsetMenu.setAttribute("disabled", "true");
    }
  }
}

var XULBrowserWindow = {
  // Stored Status, Link and Loading values
  status: "",
  defaultStatus: "",
  overLink: "",
  startTime: 0,
  statusText: "",
  isBusy: false,
  // Left here for add-on compatibility, see bug 752434
  inContentWhitelist: [],

  QueryInterface: function (aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsIXULBrowserWindow) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
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
    return gBrowser.getStatusPanel();
  },
  get isImage () {
    delete this.isImage;
    return this.isImage = document.getElementById("isImage");
  },

  init: function () {
    // Initialize the security button's state and tooltip text.
    var securityUI = gBrowser.securityUI;
    this.onSecurityChange(null, null, securityUI.state);
  },

  setJSStatus: function () {
    // unsupported
  },

  forceInitialBrowserRemote: function() {
    let initBrowser =
      document.getAnonymousElementByAttribute(gBrowser, "anonid", "initialBrowser");
    gBrowser.updateBrowserRemoteness(initBrowser, true);
    return initBrowser.frameLoader.tabParent;
  },

  setDefaultStatus: function (status) {
    this.defaultStatus = status;
    this.updateStatusField();
  },

  setOverLink: function (url, anchorElt) {
    // Encode bidirectional formatting characters.
    // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
    url = url.replace(/[\u200e\u200f\u202a\u202b\u202c\u202d\u202e]/g,
                      encodeURIComponent);

    if (gURLBar && gURLBar._mayTrimURLs /* corresponds to browser.urlbar.trimURLs */)
      url = trimURL(url);

    this.overLink = url;
    LinkTargetDisplay.update();
  },

  showTooltip: function (x, y, tooltip) {
    if (Cc["@mozilla.org/widget/dragservice;1"].getService(Ci.nsIDragService).
        getCurrentSession()) {
      return;
    }

    // The x,y coordinates are relative to the <browser> element using
    // the chrome zoom level.
    let elt = document.getElementById("remoteBrowserTooltip");
    elt.label = tooltip;

    let anchor = gBrowser.selectedBrowser;
    elt.openPopupAtScreen(anchor.boxObject.screenX + x, anchor.boxObject.screenY + y, false, null);
  },

  hideTooltip: function () {
    let elt = document.getElementById("remoteBrowserTooltip");
    elt.hidePopup();
  },

  updateStatusField: function () {
    var text, type, types = ["overLink"];
    if (this._busyUI)
      types.push("status");
    types.push("defaultStatus");
    for (type of types) {
      text = this[type];
      if (text)
        break;
    }

    // check the current value so we don't trigger an attribute change
    // and cause needless (slow!) UI updates
    if (this.statusText != text) {
      let field = this.statusTextField;
      field.setAttribute("previoustype", field.getAttribute("type"));
      field.setAttribute("type", type);
      field.label = text;
      field.setAttribute("crop", type == "overLink" ? "center" : "end");
      this.statusText = text;
    }
  },

  // Called before links are navigated to to allow us to retarget them if needed.
  onBeforeLinkTraversal: function(originalTarget, linkURI, linkNode, isAppTab) {
    let target = BrowserUtils.onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab);
    SocialUI.closeSocialPanelForLinkTraversal(target, linkNode);
    return target;
  },

  // Check whether this URI should load in the current process
  shouldLoadURI: function(aDocShell, aURI, aReferrer) {
    if (!gMultiProcessBrowser)
      return true;

    let browser = aDocShell.QueryInterface(Ci.nsIDocShellTreeItem)
                           .sameTypeRootTreeItem
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler;

    // Ignore loads that aren't in the main tabbrowser
    if (browser.localName != "browser" || !browser.getTabBrowser || browser.getTabBrowser() != gBrowser)
      return true;

    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aReferrer)) {
      E10SUtils.redirectLoad(aDocShell, aURI, aReferrer);
      return false;
    }

    return true;
  },

  onProgressChange: function (aWebProgress, aRequest,
                              aCurSelfProgress, aMaxSelfProgress,
                              aCurTotalProgress, aMaxTotalProgress) {
    // Do nothing.
  },

  onProgressChange64: function (aWebProgress, aRequest,
                                aCurSelfProgress, aMaxSelfProgress,
                                aCurTotalProgress, aMaxTotalProgress) {
    return this.onProgressChange(aWebProgress, aRequest,
      aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress,
      aMaxTotalProgress);
  },

  // This function fires only for the currently selected tab.
  onStateChange: function (aWebProgress, aRequest, aStateFlags, aStatus) {
    const nsIWebProgressListener = Ci.nsIWebProgressListener;
    const nsIChannel = Ci.nsIChannel;

    let browser = gBrowser.selectedBrowser;

    if (aStateFlags & nsIWebProgressListener.STATE_START &&
        aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {

      if (aRequest && aWebProgress.isTopLevel) {
        // clear out feed data
        browser.feeds = null;

        // clear out search-engine data
        browser.engines = null;
      }

      this.isBusy = true;

      if (!(aStateFlags & nsIWebProgressListener.STATE_RESTORING)) {
        this._busyUI = true;

        // XXX: This needs to be based on window activity...
        this.stopCommand.removeAttribute("disabled");
        CombinedStopReload.switchToStop();
      }
    }
    else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
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
          if (location.scheme == "keyword" && aWebProgress.isTopLevel)
            gBrowser.userTypedValue = null;

          if (location.spec != "about:blank") {
            switch (aStatus) {
              case Components.results.NS_ERROR_NET_TIMEOUT:
                msg = gNavigatorBundle.getString("nv_timeout");
                break;
            }
          }
        }

        this.status = "";
        this.setDefaultStatus(msg);

        // Disable menu entries for images, enable otherwise
        if (browser.documentContentType && BrowserUtils.mimeTypeIsTextBased(browser.documentContentType))
          this.isImage.removeAttribute('disabled');
        else
          this.isImage.setAttribute('disabled', 'true');
      }

      this.isBusy = false;

      if (this._busyUI) {
        this._busyUI = false;

        this.stopCommand.setAttribute("disabled", "true");
        CombinedStopReload.switchToReload(aRequest instanceof Ci.nsIRequest);
      }
    }
  },

  onLocationChange: function (aWebProgress, aRequest, aLocationURI, aFlags) {
    var location = aLocationURI ? aLocationURI.spec : "";

    // If displayed, hide the form validation popup.
    FormValidationHandler.hidePopup();

    let pageTooltip = document.getElementById("aHTMLTooltip");
    let tooltipNode = pageTooltip.triggerNode;
    if (tooltipNode) {
      // Optimise for the common case
      if (aWebProgress.isTopLevel) {
        pageTooltip.hidePopup();
      }
      else {
        for (let tooltipWindow = tooltipNode.ownerDocument.defaultView;
             tooltipWindow != tooltipWindow.parent;
             tooltipWindow = tooltipWindow.parent) {
          if (tooltipWindow == aWebProgress.DOMWindow) {
            pageTooltip.hidePopup();
            break;
          }
        }
      }
    }

    let browser = gBrowser.selectedBrowser;

    // Disable menu entries for images, enable otherwise
    if (browser.documentContentType && BrowserUtils.mimeTypeIsTextBased(browser.documentContentType))
      this.isImage.removeAttribute('disabled');
    else
      this.isImage.setAttribute('disabled', 'true');

    this.hideOverLinkImmediately = true;
    this.setOverLink("", null);
    this.hideOverLinkImmediately = false;

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation

    if (aWebProgress.isTopLevel) {
      if ((location == "about:blank" && !gBrowser.selectedBrowser.hasContentOpener) ||
          location == "") {  // Second condition is for new tabs, otherwise
                             // reload function is enabled until tab is refreshed.
        this.reloadCommand.setAttribute("disabled", "true");
      } else {
        this.reloadCommand.removeAttribute("disabled");
      }

      if (gURLBar) {
        URLBarSetURI(aLocationURI);

        BookmarkingUI.onLocationChange();
        SocialUI.updateState(location);
        UITour.onLocationChange(location);
      }

      // Utility functions for disabling find
      var shouldDisableFind = function shouldDisableFind(aDocument) {
        let docElt = aDocument.documentElement;
        return docElt && docElt.getAttribute("disablefastfind") == "true";
      }

      var disableFindCommands = function disableFindCommands(aDisable) {
        let findCommands = [document.getElementById("cmd_find"),
                            document.getElementById("cmd_findAgain"),
                            document.getElementById("cmd_findPrevious")];
        for (let elt of findCommands) {
          if (aDisable)
            elt.setAttribute("disabled", "true");
          else
            elt.removeAttribute("disabled");
        }
      }

      var onContentRSChange = function onContentRSChange(e) {
        if (e.target.readyState != "interactive" && e.target.readyState != "complete")
          return;

        e.target.removeEventListener("readystatechange", onContentRSChange);
        disableFindCommands(shouldDisableFind(e.target));
      }

      // Disable find commands in documents that ask for them to be disabled.
      if (!gMultiProcessBrowser && aLocationURI &&
          (aLocationURI.schemeIs("about") || aLocationURI.schemeIs("chrome"))) {
        // Don't need to re-enable/disable find commands for same-document location changes
        // (e.g. the replaceStates in about:addons)
        if (!(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) {
          if (content.document.readyState == "interactive" || content.document.readyState == "complete")
            disableFindCommands(shouldDisableFind(content.document));
          else {
            content.document.addEventListener("readystatechange", onContentRSChange);
          }
        }
      } else
        disableFindCommands(false);

      // Try not to instantiate gCustomizeMode as much as possible,
      // so don't use CustomizeMode.jsm to check for URI or customizing.
      let customizingURI = "about:customizing";
      if (location == customizingURI) {
        gCustomizeMode.enter();
      } else if (location != customizingURI &&
                 (CustomizationHandler.isEnteringCustomizeMode ||
                  CustomizationHandler.isCustomizing())) {
        gCustomizeMode.exit();
      }
    }
    UpdateBackForwardCommands(gBrowser.webNavigation);
    ReaderParent.updateReaderButton(gBrowser.selectedBrowser);

    gGestureSupport.restoreRotationState();

    // See bug 358202, when tabs are switched during a drag operation,
    // timers don't fire on windows (bug 203573)
    if (aRequest)
      setTimeout(function () { XULBrowserWindow.asyncUpdateUI(); }, 0);
    else
      this.asyncUpdateUI();

#ifdef MOZ_CRASHREPORTER
    if (aLocationURI) {
      let uri = aLocationURI.clone();
      try {
        // If the current URI contains a username/password, remove it.
        uri.userPass = "";
      } catch (ex) { /* Ignore failures on about: URIs. */ }

      try {
        gCrashReporter.annotateCrashReport("URL", uri.spec);
      } catch (ex if ex.result == Components.results.NS_ERROR_NOT_INITIALIZED) {
        // Don't make noise when the crash reporter is built but not enabled.
      }
    }
#endif
  },

  asyncUpdateUI: function () {
    FeedHandler.updateFeeds();
    BrowserSearch.updateSearchButton();
  },

  // Left here for add-on compatibility, see bug 752434
  hideChromeForLocation: function() {},

  onStatusChange: function (aWebProgress, aRequest, aStatus, aMessage) {
    this.status = aMessage;
    this.updateStatusField();
  },

  // Properties used to cache security state used to update the UI
  _state: null,
  _lastLocation: null,

  onSecurityChange: function (aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    let uri = gBrowser.currentURI;
    let spec = uri.spec;
    if (this._state == aState &&
        this._lastLocation == spec)
      return;
    this._state = aState;
    this._lastLocation = spec;

    // aState is defined as a bitmask that may be extended in the future.
    // We filter out any unknown bits before testing for known values.
    const wpl = Components.interfaces.nsIWebProgressListener;
    const wpl_security_bits = wpl.STATE_IS_SECURE |
                              wpl.STATE_IS_BROKEN |
                              wpl.STATE_IS_INSECURE;
    var level;

    switch (this._state & wpl_security_bits) {
      case wpl.STATE_IS_SECURE:
        level = "high";
        break;
      case wpl.STATE_IS_BROKEN:
        level = "broken";
        break;
    }

    if (level) {
      // We don't style the Location Bar based on the the 'level' attribute
      // anymore, but still set it for third-party themes.
      if (gURLBar)
        gURLBar.setAttribute("level", level);
    } else {
      if (gURLBar)
        gURLBar.removeAttribute("level");
    }

    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}
    gIdentityHandler.checkIdentity(this._state, uri);
  },

  // simulate all change notifications after switching tabs
  onUpdateCurrentBrowser: function XWB_onUpdateCurrentBrowser(aStateFlags, aStatus, aMessage, aTotalProgress) {
    if (FullZoom.updateBackgroundTabs)
      FullZoom.onLocationChange(gBrowser.currentURI, true);
    var nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    var loadingDone = aStateFlags & nsIWebProgressListener.STATE_STOP;
    // use a pseudo-object instead of a (potentially nonexistent) channel for getting
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
  }
};

var LinkTargetDisplay = {
  get DELAY_SHOW() {
     delete this.DELAY_SHOW;
     return this.DELAY_SHOW = Services.prefs.getIntPref("browser.overlink-delay");
  },

  DELAY_HIDE: 250,
  _timer: 0,

  get _isVisible () XULBrowserWindow.statusTextField.label != "",

  update: function () {
    clearTimeout(this._timer);
    window.removeEventListener("mousemove", this, true);

    if (!XULBrowserWindow.overLink) {
      if (XULBrowserWindow.hideOverLinkImmediately)
        this._hide();
      else
        this._timer = setTimeout(this._hide.bind(this), this.DELAY_HIDE);
      return;
    }

    if (this._isVisible) {
      XULBrowserWindow.updateStatusField();
    } else {
      // Let the display appear when the mouse doesn't move within the delay
      this._showDelayed();
      window.addEventListener("mousemove", this, true);
    }
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "mousemove":
        // Restart the delay since the mouse was moved
        clearTimeout(this._timer);
        this._showDelayed();
        break;
    }
  },

  _showDelayed: function () {
    this._timer = setTimeout(function (self) {
      XULBrowserWindow.updateStatusField();
      window.removeEventListener("mousemove", self, true);
    }, this.DELAY_SHOW, this);
  },

  _hide: function () {
    clearTimeout(this._timer);

    XULBrowserWindow.updateStatusField();
  }
};

var CombinedStopReload = {
  init: function () {
    if (this._initialized)
      return;

    let reload = document.getElementById("urlbar-reload-button");
    let stop = document.getElementById("urlbar-stop-button");
    if (!stop || !reload || reload.nextSibling != stop)
      return;

    this._initialized = true;
    if (XULBrowserWindow.stopCommand.getAttribute("disabled") != "true")
      reload.setAttribute("displaystop", "true");
    stop.addEventListener("click", this, false);
    this.reload = reload;
    this.stop = stop;
  },

  uninit: function () {
    if (!this._initialized)
      return;

    this._cancelTransition();
    this._initialized = false;
    this.stop.removeEventListener("click", this, false);
    this.reload = null;
    this.stop = null;
  },

  handleEvent: function (event) {
    // the only event we listen to is "click" on the stop button
    if (event.button == 0 &&
        !this.stop.disabled)
      this._stopClicked = true;
  },

  switchToStop: function () {
    if (!this._initialized)
      return;

    this._cancelTransition();
    this.reload.setAttribute("displaystop", "true");
  },

  switchToReload: function (aDelay) {
    if (!this._initialized)
      return;

    this.reload.removeAttribute("displaystop");

    if (!aDelay || this._stopClicked) {
      this._stopClicked = false;
      this._cancelTransition();
      this.reload.disabled = XULBrowserWindow.reloadCommand
                                             .getAttribute("disabled") == "true";
      return;
    }

    if (this._timer)
      return;

    // Temporarily disable the reload button to prevent the user from
    // accidentally reloading the page when intending to click the stop button
    this.reload.disabled = true;
    this._timer = setTimeout(function (self) {
      self._timer = 0;
      self.reload.disabled = XULBrowserWindow.reloadCommand
                                             .getAttribute("disabled") == "true";
    }, 650, this);
  },

  _cancelTransition: function () {
    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = 0;
    }
  }
};

var TabsProgressListener = {
  onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    // Collect telemetry data about tab load times.
    if (aWebProgress.isTopLevel) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
          TelemetryStopwatch.start("FX_PAGE_LOAD_MS", aBrowser);
          Services.telemetry.getHistogramById("FX_TOTAL_TOP_VISITS").add(true);
        } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
          TelemetryStopwatch.finish("FX_PAGE_LOAD_MS", aBrowser);
        }
      } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                 aStatus == Cr.NS_BINDING_ABORTED) {
        TelemetryStopwatch.cancel("FX_PAGE_LOAD_MS", aBrowser);
      }
    }

    // Attach a listener to watch for "click" events bubbling up from error
    // pages and other similar pages (like about:newtab). This lets us fix bugs
    // like 401575 which require error page UI to do privileged things, without
    // letting error pages have any privilege themselves.
    // We can't look for this during onLocationChange since at that point the
    // document URI is not yet the about:-uri of the error page.

    let isRemoteBrowser = aBrowser.isRemoteBrowser;
    // We check isRemoteBrowser here to avoid requesting the doc CPOW
    let doc = isRemoteBrowser ? null : aWebProgress.DOMWindow.document;

    if (!isRemoteBrowser &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        Components.isSuccessCode(aStatus) &&
        doc.documentURI.startsWith("about:") &&
        !doc.documentURI.toLowerCase().startsWith("about:blank") &&
        !doc.documentURI.toLowerCase().startsWith("about:home") &&
        !doc.documentElement.hasAttribute("hasBrowserHandlers")) {
      // STATE_STOP may be received twice for documents, thus store an
      // attribute to ensure handling it just once.
      doc.documentElement.setAttribute("hasBrowserHandlers", "true");
      aBrowser.addEventListener("click", BrowserOnClick, true);
      aBrowser.addEventListener("pagehide", function onPageHide(event) {
        if (event.target.defaultView.frameElement)
          return;
        aBrowser.removeEventListener("click", BrowserOnClick, true);
        aBrowser.removeEventListener("pagehide", onPageHide, true);
        if (event.target.documentElement)
          event.target.documentElement.removeAttribute("hasBrowserHandlers");
      }, true);
    }
  },

  onLocationChange: function (aBrowser, aWebProgress, aRequest, aLocationURI,
                              aFlags) {
    // Filter out location changes caused by anchor navigation
    // or history.push/pop/replaceState.
    if (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      // Reader mode actually cares about these:
      let mm = gBrowser.selectedBrowser.messageManager;
      mm.sendAsyncMessage("Reader:PushState");
      return;
    }

    // Filter out location changes in sub documents.
    if (!aWebProgress.isTopLevel)
      return;

    // Only need to call locationChange if the PopupNotifications object
    // for this window has already been initialized (i.e. its getter no
    // longer exists)
    if (!Object.getOwnPropertyDescriptor(window, "PopupNotifications").get)
      PopupNotifications.locationChange(aBrowser);

    gBrowser.getNotificationBox(aBrowser).removeTransientNotifications();

    FullZoom.onLocationChange(aLocationURI, false, aBrowser);
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
  }
}

function nsBrowserAccess() { }

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow, Ci.nsISupports]),

  _openURIInNewTab: function(aURI, aReferrer, aReferrerPolicy, aIsPrivate,
                             aIsExternal, aForceNotRemote=false) {
    let win, needToFocusWin;

    // try the current window.  if we're in a popup, fall back on the most recent browser window
    if (window.toolbar.visible)
      win = window;
    else {
      win = RecentWindow.getMostRecentBrowserWindow({private: aIsPrivate});
      needToFocusWin = true;
    }

    if (!win) {
      // we couldn't find a suitable window, a new one needs to be opened.
      return null;
    }

    if (aIsExternal && (!aURI || aURI.spec == "about:blank")) {
      win.BrowserOpenTab(); // this also focuses the location bar
      win.focus();
      return win.gBrowser.selectedBrowser;
    }

    let loadInBackground = gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground");

    let tab = win.gBrowser.loadOneTab(aURI ? aURI.spec : "about:blank", {
                                      referrerURI: aReferrer,
                                      referrerPolicy: aReferrerPolicy,
                                      fromExternal: aIsExternal,
                                      inBackground: loadInBackground,
                                      forceNotRemote: aForceNotRemote});
    let browser = win.gBrowser.getBrowserForTab(tab);

    if (needToFocusWin || (!loadInBackground && aIsExternal))
      win.focus();

    return browser;
  },

  openURI: function (aURI, aOpener, aWhere, aContext) {
    // This function should only ever be called if we're opening a URI
    // from a non-remote browser window (via nsContentTreeOwner).
    if (aOpener && Cu.isCrossProcessWrapper(aOpener)) {
      Cu.reportError("nsBrowserAccess.openURI was passed a CPOW for aOpener. " +
                     "openURI should only ever be called from non-remote browsers.");
      throw Cr.NS_ERROR_FAILURE;
    }

    var newWindow = null;
    var isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    if (aOpener && isExternal) {
      Cu.reportError("nsBrowserAccess.openURI did not expect an opener to be " +
                     "passed if the context is OPEN_EXTERNAL.");
      throw Cr.NS_ERROR_FAILURE;
    }

    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      dump("use --chrome command-line option to load external chrome urls\n");
      return null;
    }

    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      if (isExternal &&
          gPrefService.prefHasUserValue("browser.link.open_newwindow.override.external"))
        aWhere = gPrefService.getIntPref("browser.link.open_newwindow.override.external");
      else
        aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
    }

    let referrer = aOpener ? makeURI(aOpener.location.href) : null;
    let referrerPolicy = Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT;
    if (aOpener && aOpener.document) {
      referrerPolicy = aOpener.document.referrerPolicy;
    }
    let isPrivate = PrivateBrowsingUtils.isWindowPrivate(aOpener || window);

    switch (aWhere) {
      case Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW :
        // FIXME: Bug 408379. So how come this doesn't send the
        // referrer like the other loads do?
        var url = aURI ? aURI.spec : "about:blank";
        let features = "all,dialog=no";
        if (isPrivate) {
          features += ",private";
        }
        // Pass all params to openDialog to ensure that "url" isn't passed through
        // loadOneOrMoreURIs, which splits based on "|"
        newWindow = openDialog(getBrowserURL(), "_blank", features, url, null, null, null);
        break;
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB :
        // If we have an opener, that means that the caller is expecting access
        // to the nsIDOMWindow of the opened tab right away. For e10s windows,
        // this means forcing the newly opened browser to be non-remote so that
        // we can hand back the nsIDOMWindow. The XULBrowserWindow.shouldLoadURI
        // will do the job of shuttling off the newly opened browser to run in
        // the right process once it starts loading a URI.
        let forceNotRemote = !!aOpener;
        let browser = this._openURIInNewTab(aURI, referrer, referrerPolicy,
                                            isPrivate, isExternal,
                                            forceNotRemote);
        if (browser)
          newWindow = browser.contentWindow;
        break;
      default : // OPEN_CURRENTWINDOW or an illegal value
        newWindow = content;
        if (aURI) {
          let loadflags = isExternal ?
                            Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                            Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          gBrowser.loadURIWithFlags(aURI.spec, {
                                    flags: loadflags,
                                    referrerURI: referrer,
                                    referrerPolicy: referrerPolicy,
                                    });
        }
        if (!gPrefService.getBoolPref("browser.tabs.loadDivertedInBackground"))
          window.focus();
    }
    return newWindow;
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aParams, aWhere, aContext) {
    if (aWhere != Ci.nsIBrowserDOMWindow.OPEN_NEWTAB) {
      dump("Error: openURIInFrame can only open in new tabs");
      return null;
    }

    var isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
    let browser = this._openURIInNewTab(aURI, aParams.referrer,
                                        aParams.referrerPolicy,
                                        aParams.isPrivate, isExternal);
    if (browser)
      return browser.QueryInterface(Ci.nsIFrameLoaderOwner);

    return null;
  },

  isTabContentWindow: function (aWindow) {
    return gBrowser.browsers.some(function (browser) browser.contentWindow == aWindow);
  },
}

function getTogglableToolbars() {
  let toolbarNodes = Array.slice(gNavToolbox.childNodes);
  toolbarNodes = toolbarNodes.concat(gNavToolbox.externalToolbars);
  toolbarNodes = toolbarNodes.filter(node => node.getAttribute("toolbarname"));
  return toolbarNodes;
}

function onViewToolbarsPopupShowing(aEvent, aInsertPoint) {
  var popup = aEvent.target;
  if (popup != aEvent.currentTarget)
    return;

  // Empty the menu
  for (var i = popup.childNodes.length-1; i >= 0; --i) {
    var deadItem = popup.childNodes[i];
    if (deadItem.hasAttribute("toolbarId"))
      popup.removeChild(deadItem);
  }

  var firstMenuItem = aInsertPoint || popup.firstChild;

  let toolbarNodes = getTogglableToolbars();

  for (let toolbar of toolbarNodes) {
    let menuItem = document.createElement("menuitem");
    let hidingAttribute = toolbar.getAttribute("type") == "menubar" ?
                          "autohide" : "collapsed";
    menuItem.setAttribute("id", "toggle_" + toolbar.id);
    menuItem.setAttribute("toolbarId", toolbar.id);
    menuItem.setAttribute("type", "checkbox");
    menuItem.setAttribute("label", toolbar.getAttribute("toolbarname"));
    menuItem.setAttribute("checked", toolbar.getAttribute(hidingAttribute) != "true");
    menuItem.setAttribute("accesskey", toolbar.getAttribute("accesskey"));
    if (popup.id != "toolbar-context-menu")
      menuItem.setAttribute("key", toolbar.getAttribute("key"));

    popup.insertBefore(menuItem, firstMenuItem);

    menuItem.addEventListener("command", onViewToolbarCommand, false);
  }


  let moveToPanel = popup.querySelector(".customize-context-moveToPanel");
  let removeFromToolbar = popup.querySelector(".customize-context-removeFromToolbar");
  // View -> Toolbars menu doesn't have the moveToPanel or removeFromToolbar items.
  if (!moveToPanel || !removeFromToolbar) {
    return;
  }

  // triggerNode can be a nested child element of a toolbaritem.
  let toolbarItem = popup.triggerNode;

  if (toolbarItem && toolbarItem.localName == "toolbarpaletteitem") {
    toolbarItem = toolbarItem.firstChild;
  } else if (toolbarItem && toolbarItem.localName != "toolbar") {
    while (toolbarItem && toolbarItem.parentNode) {
      let parent = toolbarItem.parentNode;
      if ((parent.classList && parent.classList.contains("customization-target")) ||
          parent.getAttribute("overflowfortoolbar") || // Needs to work in the overflow list as well.
          parent.localName == "toolbarpaletteitem" ||
          parent.localName == "toolbar")
        break;
      toolbarItem = parent;
    }
  } else {
    toolbarItem = null;
  }

  let showTabStripItems = toolbarItem && toolbarItem.id == "tabbrowser-tabs";
  for (let node of popup.querySelectorAll('menuitem[contexttype="toolbaritem"]')) {
    node.hidden = showTabStripItems;
  }

  for (let node of popup.querySelectorAll('menuitem[contexttype="tabbar"]')) {
    node.hidden = !showTabStripItems;
  }

  if (showTabStripItems) {
    PlacesCommandHook.updateBookmarkAllTabsCommand();

    let haveMultipleTabs = gBrowser.visibleTabs.length > 1;
    document.getElementById("toolbar-context-reloadAllTabs").disabled = !haveMultipleTabs;

    document.getElementById("toolbar-context-undoCloseTab").disabled =
      SessionStore.getClosedTabCount(window) == 0;
    return;
  }

  // In some cases, we will exit the above loop with toolbarItem being the
  // xul:document. That has no parentNode, and we should disable the items in
  // this case.
  let movable = toolbarItem && toolbarItem.parentNode &&
                CustomizableUI.isWidgetRemovable(toolbarItem);
  if (movable) {
    moveToPanel.removeAttribute("disabled");
    removeFromToolbar.removeAttribute("disabled");
  } else {
    moveToPanel.setAttribute("disabled", true);
    removeFromToolbar.setAttribute("disabled", true);
  }
}

function onViewToolbarCommand(aEvent) {
  var toolbarId = aEvent.originalTarget.getAttribute("toolbarId");
  var isVisible = aEvent.originalTarget.getAttribute("checked") == "true";
  CustomizableUI.setToolbarVisibility(toolbarId, isVisible);
}

function setToolbarVisibility(toolbar, isVisible, persist=true) {
  let hidingAttribute;
  if (toolbar.getAttribute("type") == "menubar") {
    hidingAttribute = "autohide";
#ifdef MOZ_WIDGET_GTK
    Services.prefs.setBoolPref("ui.key.menuAccessKeyFocuses", !isVisible);
#endif
  } else {
    hidingAttribute = "collapsed";
  }

  toolbar.setAttribute(hidingAttribute, !isVisible);
  if (persist) {
    document.persist(toolbar.id, hidingAttribute);
  }

  let eventParams = {
    detail: {
      visible: isVisible
    },
    bubbles: true
  };
  let event = new CustomEvent("toolbarvisibilitychange", eventParams);
  toolbar.dispatchEvent(event);

  PlacesToolbarHelper.init();
  BookmarkingUI.onToolbarVisibilityChange();
  gBrowser.updateWindowResizers();
  if (isVisible)
    ToolbarIconColor.inferFromText();
}

var TabsInTitlebar = {
  init: function () {
#ifdef CAN_DRAW_IN_TITLEBAR
    this._readPref();
    Services.prefs.addObserver(this._prefName, this, false);

    // We need to update the appearance of the titlebar when the menu changes
    // from the active to the inactive state. We can't, however, rely on
    // DOMMenuBarInactive, because the menu fires this event and then removes
    // the inactive attribute after an event-loop spin.
    //
    // Because updating the appearance involves sampling the heights and margins
    // of various elements, it's important that the layout be more or less
    // settled before updating the titlebar. So instead of listening to
    // DOMMenuBarActive and DOMMenuBarInactive, we use a MutationObserver to
    // watch the "invalid" attribute directly.
    let menu = document.getElementById("toolbar-menubar");
    this._menuObserver = new MutationObserver(this._onMenuMutate);
    this._menuObserver.observe(menu, {attributes: true});

    this.onAreaReset = function(aArea) {
      if (aArea == CustomizableUI.AREA_TABSTRIP || aArea == CustomizableUI.AREA_MENUBAR)
        this._update(true);
    };
    this.onWidgetAdded = this.onWidgetRemoved = function(aWidgetId, aArea) {
      if (aArea == CustomizableUI.AREA_TABSTRIP || aArea == CustomizableUI.AREA_MENUBAR)
        this._update(true);
    };
    CustomizableUI.addListener(this);

    this._initialized = true;
#endif
  },

  allowedBy: function (condition, allow) {
#ifdef CAN_DRAW_IN_TITLEBAR
    if (allow) {
      if (condition in this._disallowed) {
        delete this._disallowed[condition];
        this._update(true);
      }
    } else {
      if (!(condition in this._disallowed)) {
        this._disallowed[condition] = null;
        this._update(true);
      }
    }
#endif
  },

  updateAppearance: function updateAppearance(aForce) {
#ifdef CAN_DRAW_IN_TITLEBAR
    this._update(aForce);
#endif
  },

  get enabled() {
    return document.documentElement.getAttribute("tabsintitlebar") == "true";
  },

#ifdef CAN_DRAW_IN_TITLEBAR
  observe: function (subject, topic, data) {
    if (topic == "nsPref:changed")
      this._readPref();
  },

  _onMenuMutate: function (aMutations) {
    for (let mutation of aMutations) {
      if (mutation.attributeName == "inactive" ||
          mutation.attributeName == "autohide") {
        TabsInTitlebar._update(true);
        return;
      }
    }
  },

  _initialized: false,
  _disallowed: {},
  _prefName: "browser.tabs.drawInTitlebar",
  _lastSizeMode: null,

  _readPref: function () {
    this.allowedBy("pref",
                   Services.prefs.getBoolPref(this._prefName));
  },

  _update: function (aForce=false) {
    function $(id) document.getElementById(id);
    function rect(ele) ele.getBoundingClientRect();
    function verticalMargins(cstyle) parseFloat(cstyle.marginBottom) + parseFloat(cstyle.marginTop);

    if (!this._initialized || window.fullScreen)
      return;

    let allowed = true;

    if (!aForce) {
      // _update is called on resize events, because the window is not ready
      // after sizemode events. However, we only care about the event when the
      // sizemode is different from the last time we updated the appearance of
      // the tabs in the titlebar.
      let sizemode = document.documentElement.getAttribute("sizemode");
      if (this._lastSizeMode == sizemode) {
        return;
      }
      this._lastSizeMode = sizemode;
    }

    for (let something in this._disallowed) {
      allowed = false;
      break;
    }

    let titlebar = $("titlebar");
    let titlebarContent = $("titlebar-content");
    let menubar = $("toolbar-menubar");

    if (allowed) {
      // We set the tabsintitlebar attribute first so that our CSS for
      // tabsintitlebar manifests before we do our measurements.
      document.documentElement.setAttribute("tabsintitlebar", "true");
      updateTitlebarDisplay();

      // Try to avoid reflows in this code by calculating dimensions first and
      // then later set the properties affecting layout together in a batch.

      // Get the full height of the tabs toolbar:
      let tabsToolbar = $("TabsToolbar");
      let tabsStyles = window.getComputedStyle(tabsToolbar);
      let fullTabsHeight = rect(tabsToolbar).height + verticalMargins(tabsStyles);
      // Buttons first:
      let captionButtonsBoxWidth = rect($("titlebar-buttonbox-container")).width;

#ifdef XP_MACOSX
      let secondaryButtonsWidth = rect($("titlebar-secondary-buttonbox")).width;
      // No need to look up the menubar stuff on OS X:
      let menuHeight = 0;
      let fullMenuHeight = 0;
#else
      // Otherwise, get the height and margins separately for the menubar
      let menuHeight = rect(menubar).height;
      let menuStyles = window.getComputedStyle(menubar);
      let fullMenuHeight = verticalMargins(menuStyles) + menuHeight;
#endif

      // And get the height of what's in the titlebar:
      let titlebarContentHeight = rect(titlebarContent).height;

      // Begin setting CSS properties which will cause a reflow

      // If the menubar is around (menuHeight is non-zero), try to adjust
      // its full height (i.e. including margins) to match the titlebar,
      // by changing the menubar's bottom padding
      if (menuHeight) {
        // Calculate the difference between the titlebar's height and that of the menubar
        let menuTitlebarDelta = titlebarContentHeight - fullMenuHeight;
        let paddingBottom;
        // The titlebar is bigger:
        if (menuTitlebarDelta > 0) {
          fullMenuHeight += menuTitlebarDelta;
          // If there is already padding on the menubar, we need to add that
          // to the difference so the total padding is correct:
          if ((paddingBottom = menuStyles.paddingBottom)) {
            menuTitlebarDelta += parseFloat(paddingBottom);
          }
          menubar.style.paddingBottom = menuTitlebarDelta + "px";
        // The menubar is bigger, but has bottom padding we can remove:
        } else if (menuTitlebarDelta < 0 && (paddingBottom = menuStyles.paddingBottom)) {
          let existingPadding = parseFloat(paddingBottom);
          // menuTitlebarDelta is negative; work out what's left, but don't set negative padding:
          let desiredPadding = Math.max(0, existingPadding + menuTitlebarDelta);
          menubar.style.paddingBottom = desiredPadding + "px";
          // We've changed the menu height now:
          fullMenuHeight += desiredPadding - existingPadding;
        }
      }

      // Next, we calculate how much we need to stretch the titlebar down to
      // go all the way to the bottom of the tab strip, if necessary.
      let tabAndMenuHeight = fullTabsHeight + fullMenuHeight;

      if (tabAndMenuHeight > titlebarContentHeight) {
        // We need to increase the titlebar content's outer height (ie including margins)
        // to match the tab and menu height:
        let extraMargin = tabAndMenuHeight - titlebarContentHeight;
#ifndef XP_MACOSX
        titlebarContent.style.marginBottom = extraMargin + "px";
#endif
        titlebarContentHeight += extraMargin;
      }

      // Then we bring up the titlebar by the same amount, but we add any negative margin:
      titlebar.style.marginBottom = "-" + titlebarContentHeight + "px";


      // Finally, size the placeholders:
#ifdef XP_MACOSX
      this._sizePlaceholder("fullscreen-button", secondaryButtonsWidth);
#endif
      this._sizePlaceholder("caption-buttons", captionButtonsBoxWidth);

      if (!this._draghandles) {
        this._draghandles = {};
        let tmp = {};
        Components.utils.import("resource://gre/modules/WindowDraggingUtils.jsm", tmp);

        let mouseDownCheck = function () {
          return !this._dragBindingAlive && TabsInTitlebar.enabled;
        };

        this._draghandles.tabsToolbar = new tmp.WindowDraggingElement(tabsToolbar);
        this._draghandles.tabsToolbar.mouseDownCheck = mouseDownCheck;

        this._draghandles.navToolbox = new tmp.WindowDraggingElement(gNavToolbox);
        this._draghandles.navToolbox.mouseDownCheck = mouseDownCheck;
      }
    } else {
      document.documentElement.removeAttribute("tabsintitlebar");
      updateTitlebarDisplay();

#ifdef XP_MACOSX
      let secondaryButtonsWidth = rect($("titlebar-secondary-buttonbox")).width;
      this._sizePlaceholder("fullscreen-button", secondaryButtonsWidth);
#endif
      // Reset the margins and padding that might have been modified:
      titlebarContent.style.marginTop = "";
      titlebarContent.style.marginBottom = "";
      titlebar.style.marginBottom = "";
      menubar.style.paddingBottom = "";
    }

    ToolbarIconColor.inferFromText();
    if (CustomizationHandler.isCustomizing()) {
      gCustomizeMode.updateLWTStyling();
    }
  },

  _sizePlaceholder: function (type, width) {
    Array.forEach(document.querySelectorAll(".titlebar-placeholder[type='"+ type +"']"),
                  function (node) { node.width = width; });
  },
#endif

  uninit: function () {
#ifdef CAN_DRAW_IN_TITLEBAR
    this._initialized = false;
    Services.prefs.removeObserver(this._prefName, this);
    this._menuObserver.disconnect();
    CustomizableUI.removeListener(this);
#endif
  }
};

#ifdef CAN_DRAW_IN_TITLEBAR
function updateTitlebarDisplay() {

#ifdef XP_MACOSX
  // OS X and the other platforms differ enough to necessitate this kind of
  // special-casing. Like the other platforms where we CAN_DRAW_IN_TITLEBAR,
  // we draw in the OS X titlebar when putting the tabs up there. However, OS X
  // also draws in the titlebar when a lightweight theme is applied, regardless
  // of whether or not the tabs are drawn in the titlebar.
  if (TabsInTitlebar.enabled) {
    document.documentElement.setAttribute("chromemargin-nonlwtheme", "0,-1,-1,-1");
    document.documentElement.setAttribute("chromemargin", "0,-1,-1,-1");
    document.documentElement.removeAttribute("drawtitle");
  } else {
    // We set chromemargin-nonlwtheme to "" instead of removing it as a way of
    // making sure that LightweightThemeConsumer doesn't take it upon itself to
    // detect this value again if and when we do a lwtheme state change.
    document.documentElement.setAttribute("chromemargin-nonlwtheme", "");
    let isCustomizing = document.documentElement.hasAttribute("customizing");
    let hasLWTheme = document.documentElement.hasAttribute("lwtheme");
    let isPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
    if ((!hasLWTheme || isCustomizing) && !isPrivate) {
      document.documentElement.removeAttribute("chromemargin");
    }
    document.documentElement.setAttribute("drawtitle", "true");
  }

#else

  if (TabsInTitlebar.enabled)
    document.documentElement.setAttribute("chromemargin", "0,2,2,2");
  else
    document.documentElement.removeAttribute("chromemargin");
#endif
}
#endif

#ifdef CAN_DRAW_IN_TITLEBAR
function onTitlebarMaxClick() {
  if (window.windowState == window.STATE_MAXIMIZED)
    window.restore();
  else
    window.maximize();
}
#endif

function displaySecurityInfo()
{
  BrowserPageInfo(null, "securityTab");
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
      if (homePage.toLowerCase() == "about:home")
        homeButton.setAttribute("tooltiptext", homeButton.getAttribute("aboutHomeOverrideTooltip"));
      else
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
      var configBundle = Services.strings
                                 .createBundle("chrome://branding/locale/browserconfig.properties");
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
  },
};

const nodeToTooltipMap = {
  "bookmarks-menu-button": "bookmarksMenuButton.tooltip",
#ifdef XP_MACOSX
  "print-button": "printButton.tooltip",
#endif
  "new-window-button": "newWindowButton.tooltip",
  "new-tab-button": "newTabButton.tooltip",
  "tabs-newtab-button": "newTabButton.tooltip",
  "fullscreen-button": "fullscreenButton.tooltip",
  "tabview-button": "tabviewButton.tooltip",
  "downloads-button": "downloads.tooltip",
};
const nodeToShortcutMap = {
  "bookmarks-menu-button": "manBookmarkKb",
#ifdef XP_MACOSX
  "print-button": "printKb",
#endif
  "new-window-button": "key_newNavigator",
  "new-tab-button": "key_newNavigatorTab",
  "tabs-newtab-button": "key_newNavigatorTab",
  "fullscreen-button": "key_fullScreen",
  "tabview-button": "key_tabview",
  "downloads-button": "key_openDownloads"
};
const gDynamicTooltipCache = new Map();
function UpdateDynamicShortcutTooltipText(aTooltip) {
  let nodeId = aTooltip.triggerNode.id || aTooltip.triggerNode.getAttribute("anonid");
  if (!gDynamicTooltipCache.has(nodeId) && nodeId in nodeToTooltipMap) {
    let strId = nodeToTooltipMap[nodeId];
    let args = [];
    if (nodeId in nodeToShortcutMap) {
      let shortcutId = nodeToShortcutMap[nodeId];
      let shortcut = document.getElementById(shortcutId);
      if (shortcut) {
        args.push(ShortcutUtils.prettifyShortcut(shortcut));
      }
    }
    gDynamicTooltipCache.set(nodeId, gNavigatorBundle.getFormattedString(strId, args));
  }
  aTooltip.setAttribute("label", gDynamicTooltipCache.get(nodeId));
}

function getBrowserSelection(aCharLen) {
  Deprecated.warning("getBrowserSelection",
                     "https://bugzilla.mozilla.org/show_bug.cgi?id=1134769");

  let focusedElement = document.activeElement;
  if (focusedElement && focusedElement.localName == "browser" &&
      focusedElement.isRemoteBrowser) {
    throw "getBrowserSelection doesn't support child process windows";
  }

  return BrowserUtils.getSelectionDetails(window, aCharLen).text;
}

var gWebPanelURI;
function openWebPanel(title, uri) {
  // Ensure that the web panels sidebar is open.
  SidebarUI.show("viewWebPanelsSidebar");

  // Set the title of the panel.
  SidebarUI.title = title;

  // Tell the Web Panels sidebar to load the bookmark.
  if (SidebarUI.browser.docShell && SidebarUI.browser.contentDocument &&
      SidebarUI.browser.contentDocument.getElementById("web-panels-browser")) {
    SidebarUI.browser.contentWindow.loadWebPanel(uri);
    if (gWebPanelURI) {
      gWebPanelURI = "";
      SidebarUI.browser.removeEventListener("load", asyncOpenWebPanel, true);
    }
  } else {
    // The panel is still being constructed.  Attach an onload handler.
    if (!gWebPanelURI) {
      SidebarUI.browser.addEventListener("load", asyncOpenWebPanel, true);
    }
    gWebPanelURI = uri;
  }
}

function asyncOpenWebPanel(event) {
  if (gWebPanelURI && SidebarUI.browser.contentDocument &&
      SidebarUI.browser.contentDocument.getElementById("web-panels-browser")) {
    SidebarUI.browser.contentWindow.loadWebPanel(gWebPanelURI);
  }
  gWebPanelURI = "";
  SidebarUI.browser.removeEventListener("load", asyncOpenWebPanel, true);
}

/*
 * - [ Dependencies ] ---------------------------------------------------------
 *  utilityOverlay.js:
 *    - gatherTextUnder
 */

/**
 * Extracts linkNode and href for the current click target.
 *
 * @param event
 *        The click event.
 * @return [href, linkNode].
 *
 * @note linkNode will be null if the click wasn't on an anchor
 *       element (or XLink).
 */
function hrefAndLinkNodeForClickEvent(event)
{
  function isHTMLLink(aNode)
  {
    // Be consistent with what nsContextMenu.js does.
    return ((aNode instanceof HTMLAnchorElement && aNode.href) ||
            (aNode instanceof HTMLAreaElement && aNode.href) ||
            aNode instanceof HTMLLinkElement);
  }

  let node = event.target;
  while (node && !isHTMLLink(node)) {
    node = node.parentNode;
  }

  if (node)
    return [node.href, node];

  // If there is no linkNode, try simple XLink.
  let href, baseURI;
  node = event.target;
  while (node && !href) {
    if (node.nodeType == Node.ELEMENT_NODE) {
      href = node.getAttributeNS("http://www.w3.org/1999/xlink", "href");
      if (href)
        baseURI = node.baseURI;
    }
    node = node.parentNode;
  }

  // In case of XLink, we don't return the node we got href from since
  // callers expect <a>-like elements.
  return [href ? makeURLAbsolute(baseURI, href) : null, null];
}

/**
 * Called whenever the user clicks in the content area.
 *
 * @param event
 *        The click event.
 * @param isPanelClick
 *        Whether the event comes from a web panel.
 * @note default event is prevented if the click is handled.
 */
function contentAreaClick(event, isPanelClick)
{
  if (!event.isTrusted || event.defaultPrevented || event.button == 2)
    return;

  let [href, linkNode] = hrefAndLinkNodeForClickEvent(event);
  if (!href) {
    // Not a link, handle middle mouse navigation.
    if (event.button == 1 &&
        gPrefService.getBoolPref("middlemouse.contentLoadURL") &&
        !gPrefService.getBoolPref("general.autoScroll")) {
      middleMousePaste(event);
      event.preventDefault();
    }
    return;
  }

  // This code only applies if we have a linkNode (i.e. clicks on real anchor
  // elements, as opposed to XLink).
  if (linkNode && event.button == 0 &&
      !event.ctrlKey && !event.shiftKey && !event.altKey && !event.metaKey) {
    // A Web panel's links should target the main content area.  Do this
    // if no modifier keys are down and if there's no target or the target
    // equals _main (the IE convention) or _content (the Mozilla convention).
    let target = linkNode.target;
    let mainTarget = !target || target == "_content" || target  == "_main";
    if (isPanelClick && mainTarget) {
      // javascript and data links should be executed in the current browser.
      if (linkNode.getAttribute("onclick") ||
          href.startsWith("javascript:") ||
          href.startsWith("data:"))
        return;

      try {
        urlSecurityCheck(href, linkNode.ownerDocument.nodePrincipal);
      }
      catch(ex) {
        // Prevent loading unsecure destinations.
        event.preventDefault();
        return;
      }

      loadURI(href, null, null, false);
      event.preventDefault();
      return;
    }

    if (linkNode.getAttribute("rel") == "sidebar") {
      // This is the Opera convention for a special link that, when clicked,
      // allows to add a sidebar panel.  The link's title attribute contains
      // the title that should be used for the sidebar panel.
      PlacesUIUtils.showBookmarkDialog({ action: "add"
                                       , type: "bookmark"
                                       , uri: makeURI(href)
                                       , title: linkNode.getAttribute("title")
                                       , loadBookmarkInSidebar: true
                                       , hiddenRows: [ "description"
                                                     , "location"
                                                     , "keyword" ]
                                       }, window);
      event.preventDefault();
      return;
    }
  }

  handleLinkClick(event, href, linkNode);

  // Mark the page as a user followed link.  This is done so that history can
  // distinguish automatic embed visits from user activated ones.  For example
  // pages loaded in frames are embed visits and lost with the session, while
  // visits across frames should be preserved.
  try {
    if (!PrivateBrowsingUtils.isWindowPrivate(window))
      PlacesUIUtils.markPageAsFollowedLink(href);
  } catch (ex) { /* Skip invalid URIs. */ }
}

/**
 * Handles clicks on links.
 *
 * @return true if the click event was handled, false otherwise.
 */
function handleLinkClick(event, href, linkNode) {
  if (event.button == 2) // right click
    return false;

  var where = whereToOpenLink(event);
  if (where == "current")
    return false;

  var doc = event.target.ownerDocument;

  if (where == "save") {
    saveURL(href, linkNode ? gatherTextUnder(linkNode) : "", null, true,
            true, doc.documentURIObject, doc);
    event.preventDefault();
    return true;
  }

  var referrerURI = doc.documentURIObject;
  // if the mixedContentChannel is present and the referring URI passes
  // a same origin check with the target URI, we can preserve the users
  // decision of disabling MCB on a page for it's child tabs.
  var persistAllowMixedContentInChildTab = false;

  if (where == "tab" && gBrowser.docShell.mixedContentChannel) {
    const sm = Services.scriptSecurityManager;
    try {
      var targetURI = makeURI(href);
      sm.checkSameOriginURI(referrerURI, targetURI, false);
      persistAllowMixedContentInChildTab = true;
    }
    catch (e) { }
  }

  urlSecurityCheck(href, doc.nodePrincipal);
  let params = { charset: doc.characterSet,
                 allowMixedContent: persistAllowMixedContentInChildTab,
                 referrerURI: referrerURI,
                 referrerPolicy: doc.referrerPolicy,
                 noReferrer: BrowserUtils.linkHasNoReferrer(linkNode) };
  openLinkIn(href, where, params);
  event.preventDefault();
  return true;
}

function middleMousePaste(event) {
  let clipboard = readFromClipboard();
  if (!clipboard)
    return;

  // Strip embedded newlines and surrounding whitespace, to match the URL
  // bar's behavior (stripsurroundingwhitespace)
  clipboard = clipboard.replace(/\s*\n\s*/g, "");

  clipboard = stripUnsafeProtocolOnPaste(clipboard);

  // if it's not the current tab, we don't need to do anything because the 
  // browser doesn't exist.
  let where = whereToOpenLink(event, true, false);
  let lastLocationChange;
  if (where == "current") {
    lastLocationChange = gBrowser.selectedBrowser.lastLocationChange;
  }

  getShortcutOrURIAndPostData(clipboard).then(data => {
    try {
      makeURI(data.url);
    } catch (ex) {
      // Not a valid URI.
      return;
    }

    try {
      addToUrlbarHistory(data.url);
    } catch (ex) {
      // Things may go wrong when adding url to session history,
      // but don't let that interfere with the loading of the url.
      Cu.reportError(ex);
    }

    if (where != "current" ||
        lastLocationChange == gBrowser.selectedBrowser.lastLocationChange) {
      openUILink(data.url, event,
                 { ignoreButton: true,
                   disallowInheritPrincipal: !data.mayInheritPrincipal });
    }
  });

  event.stopPropagation();
}

function stripUnsafeProtocolOnPaste(pasteData) {
  // Don't allow pasting javascript URIs since we don't support
  // LOAD_FLAGS_DISALLOW_INHERIT_OWNER for those.
  return pasteData.replace(/^(?:\s*javascript:)+/i, "");
}

function handleDroppedLink(event, url, name)
{
  let lastLocationChange = gBrowser.selectedBrowser.lastLocationChange;

  getShortcutOrURIAndPostData(url).then(data => {
    if (data.url &&
        lastLocationChange == gBrowser.selectedBrowser.lastLocationChange)
      loadURI(data.url, null, data.postData, false);
  });

  // Keep the event from being handled by the dragDrop listeners
  // built-in to gecko if they happen to be above us.
  event.preventDefault();
};

function BrowserSetForcedCharacterSet(aCharset)
{
  if (aCharset) {
    gBrowser.selectedBrowser.characterSet = aCharset;
    // Save the forced character-set
    if (!PrivateBrowsingUtils.isWindowPrivate(window))
      PlacesUtils.setCharsetForURI(getWebNavigation().currentURI, aCharset);
  }
  BrowserCharsetReload();
}

function BrowserCharsetReload()
{
  BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function UpdateCurrentCharset(target) {
  for (let menuItem of target.getElementsByTagName("menuitem")) {
    let isSelected = menuItem.getAttribute("charset") ===
                     CharsetMenu.foldCharset(gBrowser.selectedBrowser.characterSet);
    menuItem.setAttribute("checked", isSelected);
  }
}

var gPageStyleMenu = {

  // This maps from a <browser> element (or, more specifically, a
  // browser's permanentKey) to a CPOW that gives synchronous access
  // to the list of style sheets in a content window. The use of the
  // permanentKey is to avoid issues with docshell swapping.
  _pageStyleSyncHandlers: new WeakMap(),

  init: function() {
    let mm = window.messageManager;
    mm.addMessageListener("PageStyle:SetSyncHandler", (msg) => {
      this._pageStyleSyncHandlers.set(msg.target.permanentKey, msg.objects.syncHandler);
    });
  },

  getAllStyleSheets: function () {
    let handler = this._pageStyleSyncHandlers.get(gBrowser.selectedBrowser.permanentKey);
    try {
      return handler.getAllStyleSheets();
    } catch (ex) {
      // In case the child died or timed out.
      return [];
    }
  },

  _getStyleSheetInfo: function (browser) {
    let handler = this._pageStyleSyncHandlers.get(gBrowser.selectedBrowser.permanentKey);
    try {
      return handler.getStyleSheetInfo();
    } catch (ex) {
      // In case the child died or timed out.
      return {styleSheets: [], authorStyleDisabled: false, preferredStyleSheetSet: true};
    }
  },

  fillPopup: function (menuPopup) {
    let styleSheetInfo = this._getStyleSheetInfo(gBrowser.selectedBrowser);
    var noStyle = menuPopup.firstChild;
    var persistentOnly = noStyle.nextSibling;
    var sep = persistentOnly.nextSibling;
    while (sep.nextSibling)
      menuPopup.removeChild(sep.nextSibling);

    let styleSheets = styleSheetInfo.styleSheets;
    var currentStyleSheets = {};
    var styleDisabled = styleSheetInfo.authorStyleDisabled;
    var haveAltSheets = false;
    var altStyleSelected = false;

    for (let currentStyleSheet of styleSheets) {
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
        menuItem.setAttribute("oncommand", "gPageStyleMenu.switchStyleSheet(this.getAttribute('data'));");
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else if (currentStyleSheet.disabled) {
        lastWithSameTitle.removeAttribute("checked");
      }
    }

    noStyle.setAttribute("checked", styleDisabled);
    persistentOnly.setAttribute("checked", !altStyleSelected && !styleDisabled);
    persistentOnly.hidden = styleSheetInfo.preferredStyleSheetSet ? haveAltSheets : false;
    sep.hidden = (noStyle.hidden && persistentOnly.hidden) || !haveAltSheets;
  },

  switchStyleSheet: function (title) {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.sendAsyncMessage("PageStyle:Switch", {title: title});
  },

  disableStyle: function () {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.sendAsyncMessage("PageStyle:Disable");
  },
};

/* Legacy global page-style functions */
var getAllStyleSheets   = gPageStyleMenu.getAllStyleSheets.bind(gPageStyleMenu);
var stylesheetFillPopup = gPageStyleMenu.fillPopup.bind(gPageStyleMenu);
function stylesheetSwitchAll(contentWindow, title) {
  // We ignore the contentWindow param. Add-ons don't appear to use
  // it, and it's difficult to support in e10s (where it will be a
  // CPOW).
  gPageStyleMenu.switchStyleSheet(title);
}
function setStyleDisabled(disabled) {
  if (disabled)
    gPageStyleMenu.disableStyle();
}


var LanguageDetectionListener = {
  init: function() {
    window.messageManager.addMessageListener("Translation:DocumentState", msg => {
      Translation.documentStateReceived(msg.target, msg.data);
    });
  }
};


var BrowserOffline = {
  _inited: false,

  /////////////////////////////////////////////////////////////////////////////
  // BrowserOffline Public Methods
  init: function ()
  {
    if (!this._uiElement)
      this._uiElement = document.getElementById("workOfflineMenuitemState");

    Services.obs.addObserver(this, "network:offline-status-changed", false);

    this._updateOfflineUI(Services.io.offline);

    this._inited = true;
  },

  uninit: function ()
  {
    if (this._inited) {
      Services.obs.removeObserver(this, "network:offline-status-changed");
    }
  },

  toggleOfflineStatus: function ()
  {
    var ioService = Services.io;

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
    try {
      var cancelGoOffline = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelGoOffline, "offline-requested", null);

      // Something aborted the quit process.
      if (cancelGoOffline.data)
        return false;
    }
    catch (ex) {
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
    Services.obs.addObserver(this, "offline-cache-update-completed", false);
  },

  uninit: function ()
  {
    Services.obs.removeObserver(this, "offline-cache-update-completed");
  },

  handleEvent: function(event) {
    if (event.type == "MozApplicationManifest") {
      this.offlineAppRequested(event.originalTarget.defaultView);
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // OfflineApps Implementation Methods

  // XXX: _getBrowserWindowForContentWindow and _getBrowserForContentWindow
  // were taken from browser/components/feeds/WebContentConverter.
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
    var browsers = aBrowserWindow.gBrowser.browsers;
    for (let browser of browsers) {
      if (browser.contentWindow == aContentWindow)
        return browser;
    }
    // handle other browser/iframe elements that may need popupnotifications
    let browser = aContentWindow
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell)
                          .chromeEventHandler;
    if (browser.getAttribute("popupnotificationanchor"))
      return browser;
    return null;
  },

  _getManifestURI: function(aWindow) {
    if (!aWindow.document.documentElement)
      return null;

    var attr = aWindow.document.documentElement.getAttribute("manifest");
    if (!attr)
      return null;

    try {
      var contentURI = makeURI(aWindow.location.href, null, null);
      return makeURI(attr, aWindow.document.characterSet, contentURI);
    } catch (e) {
      return null;
    }
  },

  // A cache update isn't tied to a specific window.  Try to find
  // the best browser in which to warn the user about space usage
  _getBrowserForCacheUpdate: function(aCacheUpdate) {
    // Prefer the current browser
    var uri = this._getManifestURI(content);
    if (uri && uri.equals(aCacheUpdate.manifestURI)) {
      return gBrowser.selectedBrowser;
    }

    var browsers = gBrowser.browsers;
    for (let browser of browsers) {
      uri = this._getManifestURI(browser.contentWindow);
      if (uri && uri.equals(aCacheUpdate.manifestURI)) {
        return browser;
      }
    }

    // is this from a non-tab browser/iframe?
    browsers = document.querySelectorAll("iframe[popupnotificationanchor] | browser[popupnotificationanchor]");
    for (let browser of browsers) {
      uri = this._getManifestURI(browser.contentWindow);
      if (uri && uri.equals(aCacheUpdate.manifestURI)) {
        return browser;
      }
    }

    return null;
  },

  _warnUsage: function(aBrowser, aURI) {
    if (!aBrowser)
      return;

    let mainAction = {
      label: gNavigatorBundle.getString("offlineApps.manageUsage"),
      accessKey: gNavigatorBundle.getString("offlineApps.manageUsageAccessKey"),
      callback: OfflineApps.manage
    };

    let warnQuota = gPrefService.getIntPref("offline-apps.quota.warn");
    let message = gNavigatorBundle.getFormattedString("offlineApps.usage",
                                                      [ aURI.host,
                                                        warnQuota / 1024 ]);

    let anchorID = "indexedDB-notification-icon";
    PopupNotifications.show(aBrowser, "offline-app-usage", message,
                            anchorID, mainAction);

    // Now that we've warned once, prevent the warning from showing up
    // again.
    Services.perms.add(aURI, "offline-app",
                       Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);
  },

  // XXX: duplicated in preferences/advanced.js
  _getOfflineAppUsage: function (host, groups)
  {
    var cacheService = Cc["@mozilla.org/network/application-cache-service;1"].
                       getService(Ci.nsIApplicationCacheService);
    if (!groups)
      groups = cacheService.getGroups();

    var usage = 0;
    for (let group of groups) {
      var uri = Services.io.newURI(group, null, null);
      if (uri.asciiHost == host) {
        var cache = cacheService.getActiveCache(group);
        usage += cache.usage;
      }
    }

    return usage;
  },

  _checkUsage: function(aURI) {
    // if the user has already allowed excessive usage, don't bother checking
    if (Services.perms.testExactPermission(aURI, "offline-app") !=
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

    let browserWindow = this._getBrowserWindowForContentWindow(aContentWindow);
    let browser = this._getBrowserForContentWindow(browserWindow,
                                                   aContentWindow);

    let currentURI = aContentWindow.document.documentURIObject;

    // don't bother showing UI if the user has already made a decision
    if (Services.perms.testExactPermission(currentURI, "offline-app") != Services.perms.UNKNOWN_ACTION)
      return;

    try {
      if (gPrefService.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch(e) {
      // this pref isn't set by default, ignore failures
    }

    let host = currentURI.asciiHost;
    let notificationID = "offline-app-requested-" + host;
    let notification = PopupNotifications.getNotification(notificationID, browser);

    if (notification) {
      notification.options.documents.push(aContentWindow.document);
    } else {
      let mainAction = {
        label: gNavigatorBundle.getString("offlineApps.allow"),
        accessKey: gNavigatorBundle.getString("offlineApps.allowAccessKey"),
        callback: function() {
          for (let document of notification.options.documents) {
            OfflineApps.allowSite(document);
          }
        }
      };
      let secondaryActions = [{
        label: gNavigatorBundle.getString("offlineApps.never"),
        accessKey: gNavigatorBundle.getString("offlineApps.neverAccessKey"),
        callback: function() {
          for (let document of notification.options.documents) {
            OfflineApps.disallowSite(document);
          }
        }
      }];
      let message = gNavigatorBundle.getFormattedString("offlineApps.available",
                                                        [ host ]);
      let anchorID = "indexedDB-notification-icon";
      let options= {
        documents : [ aContentWindow.document ]
      };
      notification = PopupNotifications.show(browser, notificationID, message,
                                             anchorID, mainAction,
                                             secondaryActions, options);
    }
  },

  allowSite: function(aDocument) {
    Services.perms.add(aDocument.documentURIObject, "offline-app", Services.perms.ALLOW_ACTION);

    // When a site is enabled while loading, manifest resources will
    // start fetching immediately.  This one time we need to do it
    // ourselves.
    this._startFetching(aDocument);
  },

  disallowSite: function(aDocument) {
    Services.perms.add(aDocument.documentURIObject, "offline-app", Services.perms.DENY_ACTION);
  },

  manage: function() {
    openAdvancedPreferences("networkTab");
  },

  _startFetching: function(aDocument) {
    if (!aDocument.documentElement)
      return;

    var manifest = aDocument.documentElement.getAttribute("manifest");
    if (!manifest)
      return;

    var manifestURI = makeURI(manifest, aDocument.characterSet,
                              aDocument.documentURIObject);

    var updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].
                        getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, aDocument.documentURIObject, window);
  },

  /////////////////////////////////////////////////////////////////////////////
  // nsIObserver
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic == "offline-cache-update-completed") {
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

var IndexedDBPromptHelper = {
  _permissionsPrompt: "indexedDB-permissions-prompt",
  _permissionsResponse: "indexedDB-permissions-response",

  _notificationIcon: "indexedDB-notification-icon",

  init:
  function IndexedDBPromptHelper_init() {
    Services.obs.addObserver(this, this._permissionsPrompt, false);
  },

  uninit:
  function IndexedDBPromptHelper_uninit() {
    Services.obs.removeObserver(this, this._permissionsPrompt);
  },

  observe:
  function IndexedDBPromptHelper_observe(subject, topic, data) {
    if (topic != this._permissionsPrompt) {
      throw new Error("Unexpected topic!");
    }

    var requestor = subject.QueryInterface(Ci.nsIInterfaceRequestor);

    var browser = requestor.getInterface(Ci.nsIDOMNode);
    if (browser.ownerDocument.defaultView != window) {
      // Only listen for notifications for browsers in our chrome window.
      return;
    }

    var host = browser.currentURI.asciiHost;

    var message;
    var responseTopic;
    if (topic == this._permissionsPrompt) {
      message = gNavigatorBundle.getFormattedString("offlineApps.available",
                                                    [ host ]);
      responseTopic = this._permissionsResponse;
    }

    const hiddenTimeoutDuration = 30000; // 30 seconds
    const firstTimeoutDuration = 300000; // 5 minutes

    var timeoutId;

    var observer = requestor.getInterface(Ci.nsIObserver);

    var mainAction = {
      label: gNavigatorBundle.getString("offlineApps.allow"),
      accessKey: gNavigatorBundle.getString("offlineApps.allowAccessKey"),
      callback: function() {
        clearTimeout(timeoutId);
        observer.observe(null, responseTopic,
                         Ci.nsIPermissionManager.ALLOW_ACTION);
      }
    };

    var secondaryActions = [
      {
        label: gNavigatorBundle.getString("offlineApps.never"),
        accessKey: gNavigatorBundle.getString("offlineApps.neverAccessKey"),
        callback: function() {
          clearTimeout(timeoutId);
          observer.observe(null, responseTopic,
                           Ci.nsIPermissionManager.DENY_ACTION);
        }
      }
    ];

    // This will be set to the result of PopupNotifications.show().
    var notification;

    function timeoutNotification() {
      // Remove the notification.
      if (notification) {
        notification.remove();
      }

      // Clear all of our timeout stuff. We may be called directly, not just
      // when the timeout actually elapses.
      clearTimeout(timeoutId);

      // And tell the page that the popup timed out.
      observer.observe(null, responseTopic,
                       Ci.nsIPermissionManager.UNKNOWN_ACTION);
    }

    var options = {
      eventCallback: function(state) {
        // Don't do anything if the timeout has not been set yet.
        if (!timeoutId) {
          return;
        }

        // If the popup is being dismissed start the short timeout.
        if (state == "dismissed") {
          clearTimeout(timeoutId);
          timeoutId = setTimeout(timeoutNotification, hiddenTimeoutDuration);
          return;
        }

        // If the popup is being re-shown then clear the timeout allowing
        // unlimited waiting.
        if (state == "shown") {
          clearTimeout(timeoutId);
        }
      }
    };

    notification = PopupNotifications.show(browser, topic, message,
                                           this._notificationIcon, mainAction,
                                           secondaryActions, options);

    // Set the timeoutId after the popup has been created, and use the long
    // timeout value. If the user doesn't notice the popup after this amount of
    // time then it is most likely not visible and we want to alert the page.
    timeoutId = setTimeout(timeoutNotification, firstTimeoutDuration);
  }
};

function WindowIsClosing()
{
  if (TabView.isVisible()) {
    TabView.hide();
    return false;
  }

  if (!closeWindow(false, warnAboutClosingWindow))
    return false;

  // Bug 967873 - Proxy nsDocumentViewer::PermitUnload to the child process
  if (gMultiProcessBrowser)
    return true;

  for (let browser of gBrowser.browsers) {
    let ds = browser.docShell;
    // Passing true to permitUnload indicates we plan on closing the window.
    // This means that once unload is permitted, all further calls to
    // permitUnload will be ignored. This avoids getting multiple prompts
    // to unload the page.
    if (ds.contentViewer && !ds.contentViewer.permitUnload(true)) {
      // ... however, if the user aborts closing, we need to undo that,
      // to ensure they get prompted again when we next try to close the window.
      // We do this on the window's toplevel docshell instead of on the tab, so
      // that all tabs we iterated before will get this reset.
      window.getInterface(Ci.nsIDocShell).contentViewer.resetCloseWindow();
      return false;
    }
  }

  return true;
}

/**
 * Checks if this is the last full *browser* window around. If it is, this will
 * be communicated like quitting. Otherwise, we warn about closing multiple tabs.
 * @returns true if closing can proceed, false if it got cancelled.
 */
function warnAboutClosingWindow() {
  // Popups aren't considered full browser windows; we also ignore private windows.
  let isPBWindow = PrivateBrowsingUtils.isWindowPrivate(window) &&
        !PrivateBrowsingUtils.permanentPrivateBrowsing;
  if (!isPBWindow && !toolbar.visible)
    return gBrowser.warnAboutClosingTabs(gBrowser.closingTabsEnum.ALL);

  // Figure out if there's at least one other browser window around.
  let otherPBWindowExists = false;
  let nonPopupPresent = false;
  for (let win of browserWindows()) {
    if (!win.closed && win != window) {
      if (isPBWindow && PrivateBrowsingUtils.isWindowPrivate(win))
        otherPBWindowExists = true;
      if (win.toolbar.visible)
        nonPopupPresent = true;
      // If the current window is not in private browsing mode we don't need to 
      // look for other pb windows, we can leave the loop when finding the 
      // first non-popup window. If however the current window is in private 
      // browsing mode then we need at least one other pb and one non-popup 
      // window to break out early.
      if ((!isPBWindow || otherPBWindowExists) && nonPopupPresent)
        break;
    }
  }

  if (isPBWindow && !otherPBWindowExists) {
    let exitingCanceled = Cc["@mozilla.org/supports-PRBool;1"].
                          createInstance(Ci.nsISupportsPRBool);
    exitingCanceled.data = false;
    Services.obs.notifyObservers(exitingCanceled,
                                 "last-pb-context-exiting",
                                 null);
    if (exitingCanceled.data)
      return false;
  }

  if (nonPopupPresent) {
    return isPBWindow || gBrowser.warnAboutClosingTabs(gBrowser.closingTabsEnum.ALL);
  }

  let os = Services.obs;

  let closingCanceled = Cc["@mozilla.org/supports-PRBool;1"].
                        createInstance(Ci.nsISupportsPRBool);
  os.notifyObservers(closingCanceled,
                     "browser-lastwindow-close-requested", null);
  if (closingCanceled.data)
    return false;

  os.notifyObservers(null, "browser-lastwindow-close-granted", null);

#ifdef XP_MACOSX
  // OS X doesn't quit the application when the last window is closed, but keeps
  // the session alive. Hence don't prompt users to save tabs, but warn about
  // closing multiple tabs.
  return isPBWindow || gBrowser.warnAboutClosingTabs(gBrowser.closingTabsEnum.ALL);
#else
  return true;
#endif
}

var MailIntegration = {
  sendLinkForBrowser: function (aBrowser) {
    this.sendMessage(aBrowser.currentURI.spec, aBrowser.contentTitle);
  },

  sendMessage: function (aBody, aSubject) {
    // generate a mailto url based on the url and the url's title
    var mailtoUrl = "mailto:";
    if (aBody) {
      mailtoUrl += "?body=" + encodeURIComponent(aBody);
      mailtoUrl += "&subject=" + encodeURIComponent(aSubject);
    }

    var uri = makeURI(mailtoUrl);

    // now pass this uri to the operating system
    this._launchExternalUrl(uri);
  },

  // a generic method which can be used to pass arbitrary urls to the operating
  // system.
  // aURL --> a nsIURI which represents the url to launch
  _launchExternalUrl: function (aURL) {
    var extProtocolSvc =
       Cc["@mozilla.org/uriloader/external-protocol-service;1"]
         .getService(Ci.nsIExternalProtocolService);
    if (extProtocolSvc)
      extProtocolSvc.loadUrl(aURL);
  }
};

function BrowserOpenAddonsMgr(aView) {
  if (aView) {
    let emWindow;
    let browserWindow;

    var receivePong = function receivePong(aSubject, aTopic, aData) {
      let browserWin = aSubject.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShellTreeItem)
                               .rootTreeItem
                               .QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIDOMWindow);
      if (!emWindow || browserWin == window /* favor the current window */) {
        emWindow = aSubject;
        browserWindow = browserWin;
      }
    }
    Services.obs.addObserver(receivePong, "EM-pong", false);
    Services.obs.notifyObservers(null, "EM-ping", "");
    Services.obs.removeObserver(receivePong, "EM-pong");

    if (emWindow) {
      emWindow.loadView(aView);
      browserWindow.gBrowser.selectedTab =
        browserWindow.gBrowser._getTabForContentWindow(emWindow);
      emWindow.focus();
      return;
    }
  }

  var newLoad = !switchToTabHavingURI("about:addons", true);

  if (aView) {
    // This must be a new load, else the ping/pong would have
    // found the window above.
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      aSubject.loadView(aView);
    }, "EM-loaded", false);
  }
}

function BrowserOpenApps() {
  let appsURL = Services.urlFormatter.formatURLPref("browser.apps.URL");
  switchToTabHavingURI(appsURL, true)
}

function GetSearchFieldBookmarkData(node) {
  var charset = node.ownerDocument.characterSet;

  var formBaseURI = makeURI(node.form.baseURI,
                            charset);

  var formURI = makeURI(node.form.getAttribute("action"),
                        charset,
                        formBaseURI);

  var spec = formURI.spec;

  var isURLEncoded =
               (node.form.method.toUpperCase() == "POST"
                && (node.form.enctype == "application/x-www-form-urlencoded" ||
                    node.form.enctype == ""));

  var title = gNavigatorBundle.getFormattedString("addKeywordTitleAutoFill",
                                                  [node.ownerDocument.title]);
  var description = PlacesUIUtils.getDescriptionFromDocument(node.ownerDocument);

  var formData = [];

  function escapeNameValuePair(aName, aValue, aIsFormUrlEncoded) {
    if (aIsFormUrlEncoded)
      return escape(aName + "=" + aValue);
    else
      return escape(aName) + "=" + escape(aValue);
  }

  for (let el of node.form.elements) {
    if (!el.type) // happens with fieldsets
      continue;

    if (el == node) {
      formData.push((isURLEncoded) ? escapeNameValuePair(el.name, "%s", true) :
                                     // Don't escape "%s", just append
                                     escapeNameValuePair(el.name, "", false) + "%s");
      continue;
    }

    let type = el.type.toLowerCase();

    if (((el instanceof HTMLInputElement && el.mozIsTextField(true)) ||
        type == "hidden" || type == "textarea") ||
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
  else {
    let separator = spec.includes("?") ? "&" : "?";
    spec += separator + formData.join("&");
  }

  return {
    spec: spec,
    title: title,
    description: description,
    postData: postData,
    charSet: charset
  };
}


function AddKeywordForSearchField() {
  let bookmarkData = GetSearchFieldBookmarkData(gContextMenu.target);

  PlacesUIUtils.showBookmarkDialog({ action: "add"
                                   , type: "bookmark"
                                   , uri: makeURI(bookmarkData.spec)
                                   , title: bookmarkData.title
                                   , description: bookmarkData.description
                                   , keyword: ""
                                   , postData: bookmarkData.postData
                                   , charSet: bookmarkData.charset
                                   , hiddenRows: [ "location"
                                                 , "description"
                                                 , "tags"
                                                 , "loadInSidebar" ]
                                   }, window);
}

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
 * Re-open a closed tab.
 * @param aIndex
 *        The index of the tab (via SessionStore.getClosedTabData)
 * @returns a reference to the reopened tab.
 */
function undoCloseTab(aIndex) {
  // wallpaper patch to prevent an unnecessary blank tab (bug 343895)
  var blankTabToRemove = null;
  if (gBrowser.tabs.length == 1 && isTabEmpty(gBrowser.selectedTab))
    blankTabToRemove = gBrowser.selectedTab;

  var tab = null;
  if (SessionStore.getClosedTabCount(window) > (aIndex || 0)) {
    TabView.prepareUndoCloseTab(blankTabToRemove);
    tab = SessionStore.undoCloseTab(window, aIndex || 0);
    TabView.afterUndoCloseTab();

    if (blankTabToRemove)
      gBrowser.removeTab(blankTabToRemove);
  }

  return tab;
}

/**
 * Re-open a closed window.
 * @param aIndex
 *        The index of the window (via SessionStore.getClosedWindowData)
 * @returns a reference to the reopened window.
 */
function undoCloseWindow(aIndex) {
  let window = null;
  if (SessionStore.getClosedWindowCount() > (aIndex || 0))
    window = SessionStore.undoCloseWindow(aIndex || 0);

  return window;
}

/*
 * Determines if a tab is "empty", usually used in the context of determining
 * if it's ok to close the tab.
 */
function isTabEmpty(aTab) {
  if (aTab.hasAttribute("busy"))
    return false;

  let browser = aTab.linkedBrowser;
  if (!isBlankPageURL(browser.currentURI.spec))
    return false;

  if (browser.hasContentOpener)
    return false;

  if (browser.canGoForward || browser.canGoBack)
    return false;

  return true;
}

#ifdef MOZ_SERVICES_SYNC
function BrowserOpenSyncTabs() {
  switchToTabHavingURI("about:sync-tabs", true);
}
#endif

/**
 * Format a URL
 * eg:
 * echo formatURL("https://addons.mozilla.org/%LOCALE%/%APP%/%VERSION%/");
 * > https://addons.mozilla.org/en-US/firefox/3.0a1/
 *
 * Currently supported built-ins are LOCALE, APP, and any value from nsIXULAppInfo, uppercased.
 */
function formatURL(aFormat, aIsPref) {
  var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
  return aIsPref ? formatter.formatURLPref(aFormat) : formatter.formatURL(aFormat);
}

/**
 * Utility object to handle manipulations of the identity indicators in the UI
 */
var gIdentityHandler = {
  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED                             : "verifiedIdentity", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED                        : "verifiedDomain",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN                                : "unknownIdentity",  // No trusted identity information
  IDENTITY_MODE_MIXED_DISPLAY_LOADED                   : "unknownIdentity mixedContent mixedDisplayContent",  // SSL with unauthenticated display content
  IDENTITY_MODE_MIXED_ACTIVE_LOADED                    : "unknownIdentity mixedContent mixedActiveContent",  // SSL with unauthenticated active (and perhaps also display) content
  IDENTITY_MODE_MIXED_DISPLAY_LOADED_ACTIVE_BLOCKED    : "unknownIdentity mixedContent mixedDisplayContentLoadedActiveBlocked",  // SSL with unauthenticated display content; unauthenticated active content is blocked.
  IDENTITY_MODE_CHROMEUI                               : "chromeUI",         // Part of the product's UI

  // Cache the most recent SSLStatus and Location seen in checkIdentity
  _lastStatus : null,
  _lastUri : null,
  _mode : "unknownIdentity",

  // smart getters
  get _encryptionLabel () {
    delete this._encryptionLabel;
    this._encryptionLabel = {};
    this._encryptionLabel[this.IDENTITY_MODE_DOMAIN_VERIFIED] =
      gNavigatorBundle.getString("identity.encrypted2");
    this._encryptionLabel[this.IDENTITY_MODE_IDENTIFIED] =
      gNavigatorBundle.getString("identity.encrypted2");
    this._encryptionLabel[this.IDENTITY_MODE_UNKNOWN] =
      gNavigatorBundle.getString("identity.unencrypted");
    this._encryptionLabel[this.IDENTITY_MODE_MIXED_DISPLAY_LOADED] =
      gNavigatorBundle.getString("identity.broken_loaded");
    this._encryptionLabel[this.IDENTITY_MODE_MIXED_ACTIVE_LOADED] =
      gNavigatorBundle.getString("identity.mixed_active_loaded2");
    this._encryptionLabel[this.IDENTITY_MODE_MIXED_DISPLAY_LOADED_ACTIVE_BLOCKED] =
      gNavigatorBundle.getString("identity.broken_loaded");
    return this._encryptionLabel;
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
  get _identityPopupChromeLabel () {
    delete this._identityPopupChromeLabel;
    return this._identityPopupChromeLabel =
      document.getElementById("identity-popup-chromeLabel");
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
  get _overrideService () {
    delete this._overrideService;
    return this._overrideService = Cc["@mozilla.org/security/certoverride;1"]
                                     .getService(Ci.nsICertOverrideService);
  },
  get _identityIconCountryLabel () {
    delete this._identityIconCountryLabel;
    return this._identityIconCountryLabel = document.getElementById("identity-icon-country-label");
  },
  get _identityIcon () {
    delete this._identityIcon;
    return this._identityIcon = document.getElementById("page-proxy-favicon");
  },
  get _permissionsContainer () {
    delete this._permissionsContainer;
    return this._permissionsContainer = document.getElementById("identity-popup-permissions");
  },
  get _permissionList () {
    delete this._permissionList;
    return this._permissionList = document.getElementById("identity-popup-permission-list");
  },

  /**
   * Rebuild cache of the elements that may or may not exist depending
   * on whether there's a location bar.
   */
  _cacheElements : function() {
    delete this._identityBox;
    delete this._identityIconLabel;
    delete this._identityIconCountryLabel;
    delete this._identityIcon;
    delete this._permissionsContainer;
    delete this._permissionList;
    this._identityBox = document.getElementById("identity-box");
    this._identityIconLabel = document.getElementById("identity-icon-label");
    this._identityIconCountryLabel = document.getElementById("identity-icon-country-label");
    this._identityIcon = document.getElementById("page-proxy-favicon");
    this._permissionsContainer = document.getElementById("identity-popup-permissions");
    this._permissionList = document.getElementById("identity-popup-permission-list");
  },

  /**
   * Handler for commands on the help button in the "identity-popup" panel.
   */
  handleHelpCommand : function(event) {
    openHelpLink("secure-connection");
    this._identityPopup.hidePopup();
  },

  /**
   * Handler for mouseclicks on the "More Information" button in the
   * "identity-popup" panel.
   */
  handleMoreInfoClick : function(event) {
    displaySecurityInfo();
    event.stopPropagation();
    this._identityPopup.hidePopup();
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
   * @param nsIURI uri The address for which the UI should be updated.
   */
  checkIdentity : function(state, uri) {
    var currentStatus = gBrowser.securityUI
                                .QueryInterface(Components.interfaces.nsISSLStatusProvider)
                                .SSLStatus;
    this._lastStatus = currentStatus;
    this._lastUri = uri;

    let nsIWebProgressListener = Ci.nsIWebProgressListener;

    // For some URIs like data: we can't get a host and so can't do
    // anything useful here.
    let unknown = false;
    try {
      uri.host;
    } catch (e) { unknown = true; }

    // Chrome URIs however get special treatment. Some chrome URIs are
    // whitelisted to provide a positive security signal to the user.
    let whitelist = /^about:(accounts|addons|app-manager|config|crashes|customizing|downloads|healthreport|home|license|newaddon|permissions|preferences|privatebrowsing|rights|sessionrestore|support|welcomeback)/i;
    let isChromeUI = uri.schemeIs("about") && whitelist.test(uri.spec);
    if (isChromeUI) {
      this.setMode(this.IDENTITY_MODE_CHROMEUI);
    } else if (unknown) {
      this.setMode(this.IDENTITY_MODE_UNKNOWN);
    } else if (state & nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL) {
      this.setMode(this.IDENTITY_MODE_IDENTIFIED);
    } else if (state & nsIWebProgressListener.STATE_IS_SECURE) {
      this.setMode(this.IDENTITY_MODE_DOMAIN_VERIFIED);
    } else if (state & nsIWebProgressListener.STATE_IS_BROKEN) {
      if (state & nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT) {
        this.setMode(this.IDENTITY_MODE_MIXED_ACTIVE_LOADED);
      } else if (state & nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT) {
        this.setMode(this.IDENTITY_MODE_MIXED_DISPLAY_LOADED_ACTIVE_BLOCKED);
      } else {
        this.setMode(this.IDENTITY_MODE_MIXED_DISPLAY_LOADED);
      }
    } else {
      this.setMode(this.IDENTITY_MODE_UNKNOWN);
    }

    // Show the doorhanger when:
    // - mixed active content is blocked
    // - mixed active content is loaded (detected but not blocked)
    // - tracking content is blocked
    // - tracking content is not blocked
    if (state &
        (nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT |
         nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT  |
         nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT     |
         nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT)) {
      this.showBadContentDoorhanger(state);
    } else if (gPrefService.getBoolPref("privacy.trackingprotection.enabled")) {
      // We didn't show the shield
      Services.telemetry.getHistogramById("TRACKING_PROTECTION_SHIELD")
        .add(0);
    }
  },

  showBadContentDoorhanger : function(state) {
    var currentNotification =
      PopupNotifications.getNotification("bad-content",
        gBrowser.selectedBrowser);

    // Avoid showing the same notification (same state) repeatedly.
    if (currentNotification && currentNotification.options.state == state)
      return;

    let options = {
      /* keep doorhanger collapsed */
      dismissed: true,
      state: state
    };

    // default
    let iconState = "bad-content-blocked-notification-icon";

    // Telemetry for whether the shield was due to tracking protection or not
    let histogram = Services.telemetry.getHistogramById
                      ("TRACKING_PROTECTION_SHIELD");
    if (state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) {
      histogram.add(1);
    } else if (state &
               Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT) {
      histogram.add(2);
    } else if (gPrefService.getBoolPref("privacy.trackingprotection.enabled")) {
      // Tracking protection is enabled but no tracking elements are loaded,
      // the shield is due to mixed content.
      histogram.add(3);
    }
    if (state &
        (Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT |
         Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT)) {
      iconState = "bad-content-unblocked-notification-icon";
    }

    PopupNotifications.show(gBrowser.selectedBrowser, "bad-content",
                            "", iconState, null, null, options);
  },

  /**
   * Return the eTLD+1 version of the current hostname
   */
  getEffectiveHost : function() {
    if (!this._IDNService)
      this._IDNService = Cc["@mozilla.org/network/idn-service;1"]
                         .getService(Ci.nsIIDNService);
    try {
      let baseDomain =
        Services.eTLD.getBaseDomainFromHost(this._lastUri.host);
      return this._IDNService.convertToDisplayIDN(baseDomain, {});
    } catch (e) {
      // If something goes wrong (e.g. host is an IP address) just fail back
      // to the full domain.
      return this._lastUri.host;
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

    this._identityPopup.className = newMode;
    this._identityBox.className = newMode;
    this.setIdentityMessages(newMode);

    // Update the popup too, if it's open
    if (this._identityPopup.state == "open")
      this.setPopupMessages(newMode);

    this._mode = newMode;
  },

  /**
   * Set up the messages for the primary identity UI based on the specified mode,
   * and the details of the SSL cert, where applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setIdentityMessages : function(newMode) {
    let icon_label = "";
    let tooltip = "";
    let icon_country_label = "";
    let icon_labels_dir = "ltr";

    switch (newMode) {
    case this.IDENTITY_MODE_DOMAIN_VERIFIED: {
      let iData = this.getIdentityData();

      // Verifier is either the CA Org, for a normal cert, or a special string
      // for certs that are trusted because of a security exception.
      tooltip = gNavigatorBundle.getFormattedString("identity.identified.verifier",
                                                    [iData.caOrg]);

      // This can't throw, because URI's with a host that throw don't end up in this case.
      let host = this._lastUri.host;
      let port = 443;
      try {
        if (this._lastUri.port > 0)
          port = this._lastUri.port;
      } catch (e) {}

      if (this._overrideService.hasMatchingOverride(host, port, iData.cert, {}, {}))
        tooltip = gNavigatorBundle.getString("identity.identified.verified_by_you");

      break; }
    case this.IDENTITY_MODE_IDENTIFIED: {
      // If it's identified, then we can populate the dialog with credentials
      let iData = this.getIdentityData();
      tooltip = gNavigatorBundle.getFormattedString("identity.identified.verifier",
                                                    [iData.caOrg]);
      icon_label = iData.subjectOrg;
      if (iData.country)
        icon_country_label = "(" + iData.country + ")";

      // If the organization name starts with an RTL character, then
      // swap the positions of the organization and country code labels.
      // The Unicode ranges reflect the definition of the UCS2_CHAR_IS_BIDI
      // macro in intl/unicharutil/util/nsBidiUtils.h. When bug 218823 gets
      // fixed, this test should be replaced by one adhering to the
      // Unicode Bidirectional Algorithm proper (at the paragraph level).
      icon_labels_dir = /^[\u0590-\u08ff\ufb1d-\ufdff\ufe70-\ufefc]/.test(icon_label) ?
                        "rtl" : "ltr";
      break; }
    case this.IDENTITY_MODE_CHROMEUI:
      let brandBundle = document.getElementById("bundle_brand");
      icon_label = brandBundle.getString("brandShorterName");
      break;
    default:
      tooltip = gNavigatorBundle.getString("identity.unknown.tooltip");
    }

    // Push the appropriate strings out to the UI
    this._identityBox.tooltipText = tooltip;
    this._identityIconLabel.value = icon_label;
    this._identityIconCountryLabel.value = icon_country_label;
    // Set cropping and direction
    this._identityIconLabel.crop = icon_country_label ? "end" : "center";
    this._identityIconLabel.parentNode.style.direction = icon_labels_dir;
    // Hide completely if the organization label is empty
    this._identityIconLabel.parentNode.collapsed = icon_label ? false : true;
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
    this._identityPopupEncLabel.textContent = this._encryptionLabel[newMode];

    // Initialize the optional strings to empty values
    let supplemental = "";
    let verifier = "";
    let host = "";
    let owner = "";

    switch (newMode) {
    case this.IDENTITY_MODE_DOMAIN_VERIFIED:
      host = this.getEffectiveHost();
      verifier = this._identityBox.tooltipText;
      break;
    case this.IDENTITY_MODE_IDENTIFIED: {
      // If it's identified, then we can populate the dialog with credentials
      let iData = this.getIdentityData();
      host = this.getEffectiveHost();
      owner = iData.subjectOrg;
      verifier = this._identityBox.tooltipText;

      // Build an appropriate supplemental block out of whatever location data we have
      if (iData.city)
        supplemental += iData.city + "\n";
      if (iData.state && iData.country)
        supplemental += gNavigatorBundle.getFormattedString("identity.identified.state_and_country",
                                                            [iData.state, iData.country]);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
      break; }
    case this.IDENTITY_MODE_CHROMEUI: {
      let brandBundle = document.getElementById("bundle_brand");
      let brandShortName = brandBundle.getString("brandShortName");
      this._identityPopupChromeLabel.textContent = gNavigatorBundle.getFormattedString("identity.chrome",
                                                                                       [brandShortName]);
      break; }
    }

    // Push the appropriate strings out to the UI
    this._identityPopupContentHost.textContent = host;
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;
  },

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent : function(event) {
    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN)) {
      return; // Left click, space or enter only
    }

    // Don't allow left click, space or enter if the location has been modified.
    if (gURLBar.getAttribute("pageproxystate") != "valid") {
      return;
    }

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._identityPopup.hidden = false;

    // Update the popup strings
    this.setPopupMessages(this._identityBox.className);

    this.updateSitePermissions();

    // Add the "open" attribute to the identity box for styling
    this._identityBox.setAttribute("open", "true");
    var self = this;
    this._identityPopup.addEventListener("popuphidden", function onPopupHidden(e) {
      e.currentTarget.removeEventListener("popuphidden", onPopupHidden, false);
      self._identityBox.removeAttribute("open");
    }, false);

    // Now open the popup, anchored off the primary chrome element
    this._identityPopup.openPopup(this._identityIcon, "bottomcenter topleft");
  },

  onPopupShown : function(event) {
    document.getElementById('identity-popup-more-info-button').focus();

    this._identityPopup.addEventListener("blur", this, true);
    this._identityPopup.addEventListener("popuphidden", this);
  },

  onDragStart: function (event) {
    if (gURLBar.getAttribute("pageproxystate") != "valid")
      return;

    let value = gBrowser.currentURI.spec;
    let urlString = value + "\n" + gBrowser.contentTitle;
    let htmlString = "<a href=\"" + value + "\">" + value + "</a>";

    let dt = event.dataTransfer;
    dt.setData("text/x-moz-url", urlString);
    dt.setData("text/uri-list", value);
    dt.setData("text/plain", value);
    dt.setData("text/html", htmlString);
    dt.setDragImage(gProxyFavIcon, 16, 16);
  },
 
  handleEvent: function (event) {
    switch (event.type) {
      case "blur":
        // Focus hasn't moved yet, need to wait until after the blur event.
        setTimeout(() => {
          if (document.activeElement &&
              document.activeElement.compareDocumentPosition(this._identityPopup) &
                Node.DOCUMENT_POSITION_CONTAINS)
            return;

          this._identityPopup.hidePopup();
        }, 0);
        break;
      case "popuphidden":
        this._identityPopup.removeEventListener("blur", this, true);
        this._identityPopup.removeEventListener("popuphidden", this);
        break;
    }
  },

  updateSitePermissions: function () {
    while (this._permissionList.hasChildNodes())
      this._permissionList.removeChild(this._permissionList.lastChild);

    let uri = gBrowser.currentURI;

    for (let permission of SitePermissions.listPermissions()) {
      let state = SitePermissions.get(uri, permission);

      if (state == SitePermissions.UNKNOWN)
        continue;

      let item = this._createPermissionItem(permission, state);
      this._permissionList.appendChild(item);
    }

    this._permissionsContainer.hidden = !this._permissionList.hasChildNodes();
  },

  setPermission: function (aPermission, aState) {
    if (aState == SitePermissions.getDefault(aPermission))
      SitePermissions.remove(gBrowser.currentURI, aPermission);
    else
      SitePermissions.set(gBrowser.currentURI, aPermission, aState);
  },

  _createPermissionItem: function (aPermission, aState) {
    let menulist = document.createElement("menulist");
    let menupopup = document.createElement("menupopup");
    for (let state of SitePermissions.getAvailableStates(aPermission)) {
      let menuitem = document.createElement("menuitem");
      menuitem.setAttribute("value", state);
      menuitem.setAttribute("label", SitePermissions.getStateLabel(aPermission, state));
      menupopup.appendChild(menuitem);
    }
    menulist.appendChild(menupopup);
    menulist.setAttribute("value", aState);
    menulist.setAttribute("oncommand", "gIdentityHandler.setPermission('" +
                                       aPermission + "', this.value)");
    menulist.setAttribute("id", "identity-popup-permission:" + aPermission);

    let label = document.createElement("label");
    label.setAttribute("flex", "1");
    label.setAttribute("control", menulist.getAttribute("id"));
    label.setAttribute("value", SitePermissions.getPermissionLabel(aPermission));

    let container = document.createElement("hbox");
    container.setAttribute("align", "center");
    container.appendChild(label);
    container.appendChild(menulist);
    return container;
  }
};

function getNotificationBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getNotificationBox(foundBrowser)
  return null;
};

function getTabModalPromptBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getTabModalPromptBox(foundBrowser);
  return null;
};

/* DEPRECATED */
function getBrowser() gBrowser;
function getNavToolbox() gNavToolbox;

let gPrivateBrowsingUI = {
  init: function PBUI_init() {
    // Do nothing for normal windows
    if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }

    // Disable the Clear Recent History... menu item when in PB mode
    // temporary fix until bug 463607 is fixed
    document.getElementById("Tools:Sanitize").setAttribute("disabled", "true");

    if (window.location.href == getBrowserURL()) {
      // Adjust the window's title
      let docElement = document.documentElement;
      if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
        docElement.setAttribute("title",
          docElement.getAttribute("title_privatebrowsing"));
        docElement.setAttribute("titlemodifier",
          docElement.getAttribute("titlemodifier_privatebrowsing"));
      }
      docElement.setAttribute("privatebrowsingmode",
        PrivateBrowsingUtils.permanentPrivateBrowsing ? "permanent" : "temporary");
      gBrowser.updateTitlebar();

      if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
        // Adjust the New Window menu entries
        [
          { normal: "menu_newNavigator", private: "menu_newPrivateWindow" },
        ].forEach(function(menu) {
          let newWindow = document.getElementById(menu.normal);
          let newPrivateWindow = document.getElementById(menu.private);
          if (newWindow && newPrivateWindow) {
            newPrivateWindow.hidden = true;
            newWindow.label = newPrivateWindow.label;
            newWindow.accessKey = newPrivateWindow.accessKey;
            newWindow.command = newPrivateWindow.command;
          }
        });
      }
    }

    if (gURLBar &&
        !PrivateBrowsingUtils.permanentPrivateBrowsing) {
      // Disable switch to tab autocompletion for private windows.
      // We leave it enabled for permanent private browsing mode though.
      let value = gURLBar.getAttribute("autocompletesearchparam") || "";
      if (!value.includes("disable-private-actions")) {
        gURLBar.setAttribute("autocompletesearchparam",
                             value + " disable-private-actions");
      }
    }
  }
};

let gRemoteTabsUI = {
  init: function() {
    if (window.location.href != getBrowserURL() &&
        // Also check hidden window for the Mac no-window case
        window.location.href != "chrome://browser/content/hiddenWindow.xul") {
      return;
    }

    if (Services.appinfo.inSafeMode) {
      // e10s isn't supported in safe mode, so don't show the menu items for it
      return;
    }

#ifdef XP_MACOSX
    if (Services.prefs.getBoolPref("layers.acceleration.disabled")) {
      // On OS X, "Disable Hardware Acceleration" also disables OMTC and forces
      // a fallback to Basic Layers. This is incompatible with e10s.
      return;
    }
#endif

    let newNonRemoteWindow = document.getElementById("menu_newNonRemoteWindow");
    let autostart = Services.appinfo.browserTabsRemoteAutostart;
    newNonRemoteWindow.hidden = !autostart;
  }
};

/**
 * Switch to a tab that has a given URI, and focusses its browser window.
 * If a matching tab is in this window, it will be switched to. Otherwise, other
 * windows will be searched.
 *
 * @param aURI
 *        URI to search for
 * @param aOpenNew
 *        True to open a new tab and switch to it, if no existing tab is found.
 *        If no suitable window is found, a new one will be opened.
 * @param aOpenParams
 *        If switching to this URI results in us opening a tab, aOpenParams
 *        will be the parameter object that gets passed to openUILinkIn. Please
 *        see the documentation for openUILinkIn to see what parameters can be
 *        passed via this object.
 *        This object also allows:
 *        - 'ignoreFragment' property to be set to true to exclude fragment-portion
 *        matching when comparing URIs.
 *        - 'ignoreQueryString' property to be set to true to exclude query string
 *        matching when comparing URIs.
 *        - 'replaceQueryString' property to be set to true to exclude query string
 *        matching when comparing URIs and overwrite the initial query string with
 *        the one from the new URI.
 * @return True if an existing tab was found, false otherwise
 */
function switchToTabHavingURI(aURI, aOpenNew, aOpenParams={}) {
  // Certain URLs can be switched to irrespective of the source or destination
  // window being in private browsing mode:
  const kPrivateBrowsingWhitelist = new Set([
    "about:addons",
    "about:customizing",
  ]);

  let ignoreFragment = aOpenParams.ignoreFragment;
  let ignoreQueryString = aOpenParams.ignoreQueryString;
  let replaceQueryString = aOpenParams.replaceQueryString;

  // These properties are only used by switchToTabHavingURI and should
  // not be used as a parameter for the new load.
  delete aOpenParams.ignoreFragment;
  delete aOpenParams.ignoreQueryString;
  delete aOpenParams.replaceQueryString;

  // This will switch to the tab in aWindow having aURI, if present.
  function switchIfURIInWindow(aWindow) {
    // Only switch to the tab if neither the source nor the destination window
    // are private and they are not in permanent private browsing mode
    if (!kPrivateBrowsingWhitelist.has(aURI.spec) &&
        (PrivateBrowsingUtils.isWindowPrivate(window) ||
         PrivateBrowsingUtils.isWindowPrivate(aWindow)) &&
        !PrivateBrowsingUtils.permanentPrivateBrowsing) {
      return false;
    }

    let browsers = aWindow.gBrowser.browsers;
    for (let i = 0; i < browsers.length; i++) {
      let browser = browsers[i];
      if (ignoreFragment ? browser.currentURI.equalsExceptRef(aURI) :
                           browser.currentURI.equals(aURI)) {
        // Focus the matching window & tab
        aWindow.focus();
        if (ignoreFragment) {
          let spec = aURI.spec;
          if (!aURI.ref)
            spec += "#";
          browser.loadURI(spec);
        }
        aWindow.gBrowser.tabContainer.selectedIndex = i;
        return true;
      }
      if (ignoreQueryString || replaceQueryString) {
        if (browser.currentURI.spec.split("?")[0] == aURI.spec.split("?")[0]) {
          // Focus the matching window & tab
          aWindow.focus();
          if (replaceQueryString) {
            browser.loadURI(aURI.spec);
          }
          aWindow.gBrowser.tabContainer.selectedIndex = i;
          return true;
        }
      }
    }
    return false;
  }

  // This can be passed either nsIURI or a string.
  if (!(aURI instanceof Ci.nsIURI))
    aURI = Services.io.newURI(aURI, null, null);

  let isBrowserWindow = !!window.gBrowser;

  // Prioritise this window.
  if (isBrowserWindow && switchIfURIInWindow(window))
    return true;

  for (let browserWin of browserWindows()) {
    // Skip closed (but not yet destroyed) windows,
    // and the current window (which was checked earlier).
    if (browserWin.closed || browserWin == window)
      continue;
    if (switchIfURIInWindow(browserWin))
      return true;
  }

  // No opened tab has that url.
  if (aOpenNew) {
    if (isBrowserWindow && isTabEmpty(gBrowser.selectedTab))
      openUILinkIn(aURI.spec, "current", aOpenParams);
    else
      openUILinkIn(aURI.spec, "tab", aOpenParams);
  }

  return false;
}

let RestoreLastSessionObserver = {
  init: function () {
    if (SessionStore.canRestoreLastSession &&
        !PrivateBrowsingUtils.isWindowPrivate(window)) {
      Services.obs.addObserver(this, "sessionstore-last-session-cleared", true);
      goSetCommandEnabled("Browser:RestoreLastSession", true);
    }
  },

  observe: function () {
    // The last session can only be restored once so there's
    // no way we need to re-enable our menu item.
    Services.obs.removeObserver(this, "sessionstore-last-session-cleared");
    goSetCommandEnabled("Browser:RestoreLastSession", false);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

function restoreLastSession() {
  SessionStore.restoreLastSession();
}

var TabContextMenu = {
  contextTab: null,
  updateContextMenu: function updateContextMenu(aPopupMenu) {
    this.contextTab = aPopupMenu.triggerNode.localName == "tab" ?
                      aPopupMenu.triggerNode : gBrowser.selectedTab;
    let disabled = gBrowser.tabs.length == 1;

    var menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple");
    for (let menuItem of menuItems)
      menuItem.disabled = disabled;

#ifdef E10S_TESTING_ONLY
    menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-remote");
    for (let menuItem of menuItems)
      menuItem.hidden = !gMultiProcessBrowser;
#endif

    disabled = gBrowser.visibleTabs.length == 1;
    menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple-visible");
    for (let menuItem of menuItems)
      menuItem.disabled = disabled;

    // Session store
    document.getElementById("context_undoCloseTab").disabled =
      SessionStore.getClosedTabCount(window) == 0;

    // Only one of pin/unpin should be visible
    document.getElementById("context_pinTab").hidden = this.contextTab.pinned;
    document.getElementById("context_unpinTab").hidden = !this.contextTab.pinned;

    // Disable "Close Tabs to the Right" if there are no tabs
    // following it and hide it when the user rightclicked on a pinned
    // tab.
    document.getElementById("context_closeTabsToTheEnd").disabled =
      gBrowser.getTabsToTheEndFrom(this.contextTab).length == 0;
    document.getElementById("context_closeTabsToTheEnd").hidden = this.contextTab.pinned;

    // Disable "Close other Tabs" if there is only one unpinned tab and
    // hide it when the user rightclicked on a pinned tab.
    let unpinnedTabs = gBrowser.visibleTabs.length - gBrowser._numPinnedTabs;
    document.getElementById("context_closeOtherTabs").disabled = unpinnedTabs <= 1;
    document.getElementById("context_closeOtherTabs").hidden = this.contextTab.pinned;

    // Hide "Bookmark All Tabs" for a pinned tab.  Update its state if visible.
    let bookmarkAllTabs = document.getElementById("context_bookmarkAllTabs");
    bookmarkAllTabs.hidden = this.contextTab.pinned;
    if (!bookmarkAllTabs.hidden)
      PlacesCommandHook.updateBookmarkAllTabsCommand();

    // Hide "Move to Group" if it's a pinned tab.
    document.getElementById("context_tabViewMenu").hidden =
      (this.contextTab.pinned || !TabView.firstUseExperienced);
  }
};

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevToolsBrowser",
                                  "resource:///modules/devtools/gDevTools.jsm");

Object.defineProperty(this, "HUDService", {
  get: function HUDService_getter() {
    let devtools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
    return devtools.require("devtools/webconsole/hudservice");
  },
  configurable: true,
  enumerable: true
});

// Prompt user to restart the browser in safe mode
function safeModeRestart() {
  if (Services.appinfo.inSafeMode) {
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                     createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

    if (cancelQuit.data)
      return;

    Services.startup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
    return;
  }

  Services.obs.notifyObservers(null, "restart-in-safe-mode", "");
}

/* duplicateTabIn duplicates tab in a place specified by the parameter |where|.
 *
 * |where| can be:
 *  "tab"         new tab
 *  "tabshifted"  same as "tab" but in background if default is to select new
 *                tabs, and vice versa
 *  "window"      new window
 *
 * delta is the offset to the history entry that you want to load.
 */
function duplicateTabIn(aTab, where, delta) {
  let newTab = SessionStore.duplicateTab(window, aTab, delta);

  switch (where) {
    case "window":
      gBrowser.hideTab(newTab);
      gBrowser.replaceTabWithWindow(newTab);
      break;
    case "tabshifted":
      // A background tab has been opened, nothing else to do here.
      break;
    case "tab":
      gBrowser.selectedTab = newTab;
      break;
  }
}

var Scratchpad = {
  openScratchpad: function SP_openScratchpad() {
    return this.ScratchpadManager.openScratchpad();
  }
};

XPCOMUtils.defineLazyGetter(Scratchpad, "ScratchpadManager", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/scratchpad-manager.jsm", tmp);
  return tmp.ScratchpadManager;
});

var ResponsiveUI = {
  toggle: function RUI_toggle() {
    this.ResponsiveUIManager.toggle(window, gBrowser.selectedTab);
  }
};

XPCOMUtils.defineLazyGetter(ResponsiveUI, "ResponsiveUIManager", function() {
  let tmp = {};
  Cu.import("resource:///modules/devtools/responsivedesign.jsm", tmp);
  return tmp.ResponsiveUIManager;
});

function openEyedropper() {
  var eyedropper = new this.Eyedropper(this, { context: "menu",
                                               copyOnSelect: true });
  eyedropper.open();
}

Object.defineProperty(this, "Eyedropper", {
  get: function() {
    let devtools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
    return devtools.require("devtools/eyedropper/eyedropper").Eyedropper;
  },
  configurable: true,
  enumerable: true
});

XPCOMUtils.defineLazyGetter(window, "gShowPageResizers", function () {
#ifdef XP_WIN
  // Only show resizers on Windows 2000 and XP
  return parseFloat(Services.sysinfo.getProperty("version")) < 6;
#else
  return false;
#endif
});

var MousePosTracker = {
  _listeners: new Set(),
  _x: 0,
  _y: 0,
  get _windowUtils() {
    delete this._windowUtils;
    return this._windowUtils = window.getInterface(Ci.nsIDOMWindowUtils);
  },

  addListener: function (listener) {
    if (this._listeners.has(listener))
      return;

    listener._hover = false;
    this._listeners.add(listener);

    this._callListener(listener);
  },

  removeListener: function (listener) {
    this._listeners.delete(listener);
  },

  handleEvent: function (event) {
    var fullZoom = this._windowUtils.fullZoom;
    this._x = event.screenX / fullZoom - window.mozInnerScreenX;
    this._y = event.screenY / fullZoom - window.mozInnerScreenY;

    this._listeners.forEach(function (listener) {
      try {
        this._callListener(listener);
      } catch (e) {
        Cu.reportError(e);
      }
    }, this);
  },

  _callListener: function (listener) {
    let rect = listener.getMouseTargetRect();
    let hover = this._x >= rect.left &&
                this._x <= rect.right &&
                this._y >= rect.top &&
                this._y <= rect.bottom;

    if (hover == listener._hover)
      return;

    listener._hover = hover;

    if (hover) {
      if (listener.onMouseEnter)
        listener.onMouseEnter();
    } else {
      if (listener.onMouseLeave)
        listener.onMouseLeave();
    }
  }
};

function focusNextFrame(event) {
  let fm = Services.focus;
  let dir = event.shiftKey ? fm.MOVEFOCUS_BACKWARDDOC : fm.MOVEFOCUS_FORWARDDOC;
  let element = fm.moveFocus(window, null, dir, fm.FLAG_BYKEY);
  let panelOrNotificationSelector = "popupnotification " + element.localName + ", " +
                                    "panel " + element.localName;
  if (element.ownerDocument == document && !element.matches(panelOrNotificationSelector))
    focusAndSelectUrlBar();
}

function BrowserOpenNewTabOrWindow(event) {
  if (event.shiftKey) {
    OpenBrowserWindow();
  } else {
    BrowserOpenTab();
  }
}

let ToolbarIconColor = {
  init: function () {
    this._initialized = true;

    window.addEventListener("activate", this);
    window.addEventListener("deactivate", this);
    Services.obs.addObserver(this, "lightweight-theme-styling-update", false);

    // If the window isn't active now, we assume that it has never been active
    // before and will soon become active such that inferFromText will be
    // called from the initial activate event.
    if (Services.focus.activeWindow == window)
      this.inferFromText();
  },

  uninit: function () {
    this._initialized = false;

    window.removeEventListener("activate", this);
    window.removeEventListener("deactivate", this);
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "activate":
      case "deactivate":
        this.inferFromText();
        break;
    }
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "lightweight-theme-styling-update":
        // inferFromText needs to run after LightweightThemeConsumer.jsm's
        // lightweight-theme-styling-update observer.
        setTimeout(() => { this.inferFromText(); }, 0);
        break;
    }
  },

  inferFromText: function () {
    if (!this._initialized)
      return;

    function parseRGB(aColorString) {
      let rgb = aColorString.match(/^rgba?\((\d+), (\d+), (\d+)/);
      rgb.shift();
      return rgb.map(x => parseInt(x));
    }

    let toolbarSelector = "#navigator-toolbox > toolbar:not([collapsed=true]):not(#addon-bar)";
#ifdef XP_MACOSX
    toolbarSelector += ":not([type=menubar])";
#endif

    // The getComputedStyle calls and setting the brighttext are separated in
    // two loops to avoid flushing layout and making it dirty repeatedly.

    let luminances = new Map;
    for (let toolbar of document.querySelectorAll(toolbarSelector)) {
      let [r, g, b] = parseRGB(getComputedStyle(toolbar).color);
      let luminance = 0.2125 * r + 0.7154 * g + 0.0721 * b;
      luminances.set(toolbar, luminance);
    }

    for (let [toolbar, luminance] of luminances) {
      if (luminance <= 110)
        toolbar.removeAttribute("brighttext");
      else
        toolbar.setAttribute("brighttext", "true");
    }
  }
}

let PanicButtonNotifier = {
  init: function() {
    this._initialized = true;
    if (window.PanicButtonNotifierShouldNotify) {
      delete window.PanicButtonNotifierShouldNotify;
      this.notify();
    }
  },
  notify: function() {
    if (!this._initialized) {
      window.PanicButtonNotifierShouldNotify = true;
      return;
    }
    // Display notification panel here...
    try {
      let popup = document.getElementById("panic-button-success-notification");
      popup.hidden = false;
      let widget = CustomizableUI.getWidget("panic-button").forWindow(window);
      let anchor = widget.anchor;
      anchor = document.getAnonymousElementByAttribute(anchor, "class", "toolbarbutton-icon");
      popup.openPopup(anchor, popup.getAttribute("position"));
    } catch (ex) {
      Cu.reportError(ex);
    }
  },
  close: function() {
    let popup = document.getElementById("panic-button-success-notification");
    popup.hidePopup();
  },
};

let AboutPrivateBrowsingListener = {
  init: function () {
    window.messageManager.addMessageListener(
      "AboutPrivateBrowsing:OpenPrivateWindow",
      msg => {
        OpenBrowserWindow({private: true});
    });
  }
};
