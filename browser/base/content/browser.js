/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cu = Components.utils;
var Cc = Components.classes;

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
XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Favicons",
                                   "@mozilla.org/browser/favicon-service;1",
                                   "mozIAsyncFavicons");
XPCOMUtils.defineLazyServiceGetter(this, "gDNSService",
                                   "@mozilla.org/network/dns-service;1",
                                   "nsIDNSService");
XPCOMUtils.defineLazyServiceGetter(this, "WindowsUIUtils",
                                   "@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource:///modules/ContextualIdentityService.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "gAboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");
XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle('chrome://browser/locale/browser.properties');
});
XPCOMUtils.defineLazyModuleGetter(this, "AddonWatcher",
                                  "resource://gre/modules/AddonWatcher.jsm");

const nsIWebNavigation = Ci.nsIWebNavigation;

var gLastBrowserCharset = null;
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

if (AppConstants.platform != "macosx") {
  var gEditUIVisible = true;
}

/*globals gBrowser, gNavToolbox, gURLBar, gNavigatorBundle*/
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

XPCOMUtils.defineLazyModuleGetter(this, "Weave",
  "resource://services-sync/main.js");

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

XPCOMUtils.defineLazyGetter(this, "BrowserToolboxProcess", function() {
  let tmp = {};
  Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", tmp);
  return tmp.BrowserToolboxProcess;
});

XPCOMUtils.defineLazyModuleGetter(this, "Social",
  "resource:///modules/Social.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ProcessHangMonitor",
  "resource:///modules/ProcessHangMonitor.jsm");

if (AppConstants.MOZ_SAFE_BROWSING) {
  XPCOMUtils.defineLazyModuleGetter(this, "SafeBrowsing",
    "resource://gre/modules/SafeBrowsing.jsm");
}

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Translation",
  "resource:///modules/translation/Translation.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SitePermissions",
  "resource:///modules/SitePermissions.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gWebRTCUI",
  "resource:///modules/webrtcUI.jsm", "webrtcUI");

XPCOMUtils.defineLazyModuleGetter(this, "TabCrashHandler",
  "resource:///modules/ContentCrashHandlers.jsm");

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyModuleGetter(this, "PluginCrashReporter",
    "resource:///modules/ContentCrashHandlers.jsm");
}

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

XPCOMUtils.defineLazyModuleGetter(this, "LoginManagerParent",
  "resource://gre/modules/LoginManagerParent.jsm");

var gInitialPages = [
  "about:blank",
  "about:newtab",
  "about:home",
  "about:privatebrowsing",
  "about:welcomeback",
  "about:sessionrestore"
];

XPCOMUtils.defineLazyGetter(this, "Win7Features", function () {
  if (AppConstants.platform != "win")
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
  return null;
});

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                     "@mozilla.org/xre/app-info;1",
                                     "nsICrashReporter");
}

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

    // Clear undo history of the URL bar
    gURLBar.editor.transactionManager.clear()
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

    if (!this._reportButton)
      this._reportButton = document.getElementById("page-report-button");

    if (!gBrowser.selectedBrowser.blockedPopups) {
      // Hide the icon in the location bar (if the location bar exists)
      this._reportButton.hidden = true;

      // Hide the notification box (if it's visible).
      let notificationBox = gBrowser.getNotificationBox();
      let notification = notificationBox.getNotificationWithValue("popup-blocked");
      if (notification) {
        notificationBox.removeNotification(notification, false);
      }
      return;
    }

    this._reportButton.hidden = false;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!gBrowser.selectedBrowser.blockedPopups.reported) {
      if (gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var popupCount = gBrowser.selectedBrowser.blockedPopups.length;

        var stringKey = AppConstants.platform == "win"
                        ? "popupWarningButton"
                        : "popupWarningButtonUnix";

        var popupButtonText = gNavigatorBundle.getString(stringKey);
        var popupButtonAccesskey = gNavigatorBundle.getString(stringKey + ".accesskey");

        var messageBase = gNavigatorBundle.getString("popupWarning.message");
        var message = PluralForm.get(popupCount, messageBase)
                                .replace("#1", brandShortName)
                                .replace("#2", popupCount);

        let notificationBox = gBrowser.getNotificationBox();
        let notification = notificationBox.getNotificationWithValue("popup-blocked");
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
  let postData = params.postData;

  let wasRemote = browser.isRemoteBrowser;

  let process = browser.isRemoteBrowser ? Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT
                                        : Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  let mustChangeProcess = gMultiProcessBrowser &&
                          !E10SUtils.canLoadURIInProcess(uri, process);
  if ((!wasRemote && !mustChangeProcess) ||
      (wasRemote && mustChangeProcess)) {
    browser.inLoadURI = true;
  }
  try {
    if (!mustChangeProcess) {
      browser.webNavigation.loadURIWithOptions(uri, flags,
                                               referrer, referrerPolicy,
                                               postData, null, null);
    } else {
      if (postData) {
        postData = NetUtil.readInputStreamToString(postData, postData.available());
      }

      LoadInOtherProcess(browser, {
        uri: uri,
        flags: flags,
        referrer: referrer ? referrer.spec : null,
        referrerPolicy: referrerPolicy,
        postData: postData,
      });
    }
  } catch (e) {
    // If anything goes wrong when switching remoteness, just switch remoteness
    // manually and load the URI.
    // We might lose history that way but at least the browser loaded a page.
    // This might be necessary if SessionStore wasn't initialized yet i.e.
    // when the homepage is a non-remote page.
    if (mustChangeProcess) {
      Cu.reportError(e);
      gBrowser.updateBrowserRemotenessByURL(browser, uri);
      browser.webNavigation.loadURIWithOptions(uri, flags, referrer, referrerPolicy,
                                               postData, null, null);
    } else {
      throw e;
    }
  } finally {
    if ((!wasRemote && !mustChangeProcess) ||
        (wasRemote && mustChangeProcess)) {
      browser.inLoadURI = false;
    }
  }
}

// Starts a new load in the browser first switching the browser to the correct
// process
function LoadInOtherProcess(browser, loadOptions, historyIndex = -1) {
  let tab = gBrowser.getTabForBrowser(browser);
  SessionStore.navigateAndRestore(tab, loadOptions, historyIndex);
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
    FeedHandler.init();
    DevEdition.init();
    AboutPrivateBrowsingListener.init();
    TrackingProtection.init();
    RefreshBlocker.init();

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
      const TARGET_WIDTH = 1280;
      const TARGET_HEIGHT = 1040;
      let width = Math.min(screen.availWidth * .9, TARGET_WIDTH);
      let height = Math.min(screen.availHeight * .9, TARGET_HEIGHT);

      document.documentElement.setAttribute("width", width);
      document.documentElement.setAttribute("height", height);

      if (width < TARGET_WIDTH && height < TARGET_HEIGHT) {
        document.documentElement.setAttribute("sizemode", "maximized");
      }
    }

    if (!window.toolbar.visible) {
      // adjust browser UI for popups
      gURLBar.setAttribute("readonly", "true");
      gURLBar.setAttribute("enablehistory", "false");
      goSetCommandEnabled("cmd_newNavigatorTab", false);
    }

    // Misc. inits.
    TabletModeUpdater.init();
    CombinedStopReload.init();
    gPrivateBrowsingUI.init();

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

    // We need to set the OfflineApps message listeners up before we
    // load homepages, which might need them.
    OfflineApps.init();

    // This pageshow listener needs to be registered before we may call
    // swapBrowsersAndCloseOther() to receive pageshow events fired by that.
    let mm = window.messageManager;
    mm.addMessageListener("PageVisibility:Show", function(message) {
      if (message.target == gBrowser.selectedBrowser) {
        setTimeout(pageShowEventHandlers, 0, message.data.persisted);
      }
    });

    gBrowser.addEventListener("AboutTabCrashedLoad", function(event) {
      let ownerDoc = event.originalTarget;

      if (!ownerDoc.documentURI.startsWith("about:tabcrashed")) {
        return;
      }

      let browser = gBrowser.getBrowserForDocument(event.target);
      // Reset the zoom for the tabcrashed page.
      ZoomManager.setZoomForBrowser(browser, 1);
    }, false, true);

    gBrowser.addEventListener("InsecureLoginFormsStateChange", function() {
      gIdentityHandler.refreshForInsecureLoginForms();
    });

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

        // If this tab was passed as a window argument, clear the
        // reference to it from the arguments array.
        if (window.arguments[0] == tabToOpen) {
          window.arguments[0] = null;
        }

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

    if (AppConstants.MOZ_SAFE_BROWSING) {
      // Bug 778855 - Perf regression if we do this here. To be addressed in bug 779008.
      setTimeout(function() { SafeBrowsing.init(); }, 2000);
    }

    Services.obs.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-disabled", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-started", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-blocked", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-origin-blocked", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-failed", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-confirmation", false);
    Services.obs.addObserver(gXPInstallObserver, "addon-install-complete", false);
    window.messageManager.addMessageListener("Browser:URIFixup", gKeywordURIFixup);

    BrowserOffline.init();
    IndexedDBPromptHelper.init();

    if (AppConstants.E10S_TESTING_ONLY)
      gRemoteTabsUI.init();

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
      if (gBrowser.selectedBrowser.isRemoteBrowser) {
        // If the initial browser is remote, in order to optimize for first paint,
        // we'll defer switching focus to that browser until it has painted.
        let focusedElement = document.commandDispatcher.focusedElement;
        let mm = window.messageManager;
        mm.addMessageListener("Browser:FirstPaint", function onFirstPaint() {
          mm.removeMessageListener("Browser:FirstPaint", onFirstPaint);
          // If focus didn't move while we were waiting for first paint, we're okay
          // to move to the browser.
          if (document.commandDispatcher.focusedElement == focusedElement) {
            gBrowser.selectedBrowser.focus();
          }
        });
      } else {
        // If the initial browser is not remote, we can focus the browser
        // immediately with no paint performance impact.
        gBrowser.selectedBrowser.focus();
      }
    }

    // Enable/Disable auto-hide tabbar
    gBrowser.tabContainer.updateVisibility();

    BookmarkingUI.init();

    gPrefService.addObserver(gHomeButton.prefDomain, gHomeButton, false);

    var homeButton = document.getElementById("home-button");
    gHomeButton.updateTooltip(homeButton);

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

    if (AppConstants.platform != "macosx") {
      updateEditUIVisibility();
      let placesContext = document.getElementById("placesContext");
      placesContext.addEventListener("popupshowing", updateEditUIVisibility, false);
      placesContext.addEventListener("popuphiding", updateEditUIVisibility, false);
    }

    LightWeightThemeWebInstaller.init();

    if (Win7Features)
      Win7Features.onOpenWindow();

    FullScreen.init();

    // initialize the sync UI
    gSyncUI.init();
    gFxAccounts.init();

    if (AppConstants.MOZ_DATA_REPORTING)
      gDataNotificationInfoBar.init();

    gBrowserThumbnails.init();

    gMenuButtonBadgeManager.init();

    gMenuButtonUpdateBadge.init();

    UserContextStyleManager.init();

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

    // Report via telemetry whether we're able to play MP4/H.264/AAC video.
    // We suspect that some Windows users have a broken or have not installed
    // Windows Media Foundation, and we'd like to know how many. We'd also like
    // to know how good our coverage is on other platforms.
    // Note: we delay by 90 seconds reporting this, as calling canPlayType()
    // on Windows will cause DLLs to load, i.e. cause disk I/O.
    setTimeout(() => {
      let v = document.createElementNS("http://www.w3.org/1999/xhtml", "video");
      let aacWorks = v.canPlayType("audio/mp4") != "";
      Services.telemetry.getHistogramById("VIDEO_CAN_CREATE_AAC_DECODER").add(aacWorks);
      let h264Works = v.canPlayType("video/mp4") != "";
      Services.telemetry.getHistogramById("VIDEO_CAN_CREATE_H264_DECODER").add(h264Works);
    }, 90 * 1000);

    SessionStore.promiseInitialized.then(() => {
      // Bail out if the window has been closed in the meantime.
      if (window.closed) {
        return;
      }

      // Enable the Restore Last Session command if needed
      RestoreLastSessionObserver.init();

      SocialUI.init();

      // Start monitoring slow add-ons
      AddonWatcher.init();

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

    // First clean up services initialized in gBrowserInit.onLoad (or those whose
    // uninit methods don't depend on the services having been initialized).

    CombinedStopReload.uninit();

    gGestureSupport.init(false);

    gHistorySwipeAnimation.uninit();

    FullScreen.uninit();

    gFxAccounts.uninit();

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

    TabletModeUpdater.uninit();

    gTabletModePageCounter.finish();

    BrowserOnClick.uninit();

    FeedHandler.uninit();

    DevEdition.uninit();

    TrackingProtection.uninit();

    RefreshBlocker.uninit();

    gMenuButtonUpdateBadge.uninit();

    gMenuButtonBadgeManager.uninit();

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
      SocialUI.uninit();
      gBrowserThumbnails.uninit();
      FullZoom.destroy();

      Services.obs.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-disabled");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-started");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-blocked");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-origin-blocked");
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
};

if (AppConstants.platform == "macosx") {
  // nonBrowserWindowStartup(), nonBrowserWindowDelayedStartup(), and
  // nonBrowserWindowShutdown() are used for non-browser windows in
  // macBrowserOverlay
  gBrowserInit.nonBrowserWindowStartup = function() {
    // Disable inappropriate commands / submenus
    var disabledItems = ['Browser:SavePage',
                         'Browser:SendLink', 'cmd_pageSetup', 'cmd_print', 'cmd_find', 'cmd_findAgain',
                         'viewToolbarsMenu', 'viewSidebarMenuMenu', 'Browser:Reload',
                         'viewFullZoomMenu', 'pageStyleMenu', 'charsetMenu', 'View:PageSource', 'View:FullScreen',
                         'viewHistorySidebar', 'Browser:AddBookmarkAs', 'Browser:BookmarkAllTabs',
                         'View:PageInfo'];
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
  };

  gBrowserInit.nonBrowserWindowDelayedStartup = function() {
    this._delayedStartupTimeoutId = null;

    // initialise the offline listener
    BrowserOffline.init();

    // initialize the private browsing UI
    gPrivateBrowsingUI.init();

    // initialize the sync UI
    gSyncUI.init();

    if (AppConstants.E10S_TESTING_ONLY) {
      gRemoteTabsUI.init();
    }
  };

  gBrowserInit.nonBrowserWindowShutdown = function() {
    let dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
                      .getService(Ci.nsIMacDockSupport);
    dockSupport.dockMenu = null;

    // If nonBrowserWindowDelayedStartup hasn't run yet, we have no work to do -
    // just cancel the pending timeout and return;
    if (this._delayedStartupTimeoutId) {
      clearTimeout(this._delayedStartupTimeoutId);
      return;
    }

    BrowserOffline.uninit();
  };
}


/* Legacy global init functions */
var BrowserStartup        = gBrowserInit.onLoad.bind(gBrowserInit);
var BrowserShutdown       = gBrowserInit.onUnload.bind(gBrowserInit);

if (AppConstants.platform == "macosx") {
  var nonBrowserWindowStartup        = gBrowserInit.nonBrowserWindowStartup.bind(gBrowserInit);
  var nonBrowserWindowDelayedStartup = gBrowserInit.nonBrowserWindowDelayedStartup.bind(gBrowserInit);
  var nonBrowserWindowShutdown       = gBrowserInit.nonBrowserWindowShutdown.bind(gBrowserInit);
}

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
    PrintUtils.printWindow(gBrowser.selectedBrowser.outerWindowID,
                           gBrowser.selectedBrowser);
    break;
  case "Save":
    saveBrowser(gBrowser.selectedBrowser);
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

  let historyindex = aEvent.target.getAttribute("historyindex");
  duplicateTabIn(gBrowser.selectedTab, where, Number(historyindex));
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
  let metaKeyPressed = AppConstants.platform == "macosx"
                       ? aEvent.metaKey
                       : aEvent.ctrlKey;
  var backgroundTabModifier = aEvent.button == 1 || metaKeyPressed;

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
  if (gBrowser.currentURI.schemeIs("view-source")) {
    // Bug 1167797: For view source, we always skip the cache
    return BrowserReloadSkipCache();
  }
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
  // we're not a browser window, pass the URI string to a new browser window
  if (window.location.href != getBrowserURL())
  {
    window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", aURIString);
    return;
  }

  // This function throws for certain malformed URIs, so use exception handling
  // so that we don't disrupt startup
  try {
    gBrowser.loadTabs(aURIString.split("|"), false, true);
  }
  catch (e) {
  }
}

function focusAndSelectUrlBar() {
  // In customize mode, the url bar is disabled. If a new tab is opened or the
  // user switches to a different tab, this function gets called before we've
  // finished leaving customize mode, and the url bar will still be disabled.
  // We can't focus it when it's disabled, so we need to re-run ourselves when
  // we've finished leaving customize mode.
  if (CustomizationHandler.isExitingCustomizeMode) {
    gNavToolbox.addEventListener("aftercustomization", function afterCustomize() {
      gNavToolbox.removeEventListener("aftercustomization", afterCustomize);
      focusAndSelectUrlBar();
    });

    return true;
  }

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
  // If we're not a browser window, just close the window
  if (window.location.href != getBrowserURL()) {
    closeWindow(true);
    return;
  }

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

    // A corrupt Places database could make this throw, breaking navigation
    // from the location bar.
    try {
      let entry = yield PlacesUtils.keywords.fetch(keyword);
      if (entry) {
        shortcutURL = entry.url.href;
        postData = entry.postData;
      }
    } catch (ex) {
      Components.utils.reportError(`Unable to fetch data for Places keyword "${keyword}": ${ex}`);
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
 *        URL (required):
 *          A string URL for the page we'd like to view the source of.
 *        browser (optional):
 *          The browser containing the document that we would like to view the
 *          source of. This is required if outerWindowID is passed.
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

  let viewInternal = () => {
    let inTab = Services.prefs.getBoolPref("view_source.tab");
    if (inTab) {
      let tabBrowser = gBrowser;
      // In the case of sidebars and chat windows, gBrowser is defined but null,
      // because no #content element exists.  For these cases, we need to find
      // the most recent browser window.
      // In the case of popups, we need to find a non-popup browser window.
      if (!tabBrowser || !window.toolbar.visible) {
        // This returns only non-popup browser windows by default.
        let browserWindow = RecentWindow.getMostRecentBrowserWindow();
        tabBrowser = browserWindow.gBrowser;
      }
      // Some internal URLs (such as specific chrome: and about: URLs that are
      // not yet remote ready) cannot be loaded in a remote browser.  View
      // source in tab expects the new view source browser's remoteness to match
      // that of the original URL, so disable remoteness if necessary for this
      // URL.
      let contentProcess = Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT
      let forceNotRemote =
        gMultiProcessBrowser &&
        !E10SUtils.canLoadURIInProcess(args.URL, contentProcess);
      // `viewSourceInBrowser` will load the source content from the page
      // descriptor for the tab (when possible) or fallback to the network if
      // that fails.  Either way, the view source module will manage the tab's
      // location, so use "about:blank" here to avoid unnecessary redundant
      // requests.
      let tab = tabBrowser.loadOneTab("about:blank", {
        relatedToCurrent: true,
        inBackground: false,
        forceNotRemote,
      });
      args.viewSourceBrowser = tabBrowser.getBrowserForTab(tab);
      top.gViewSourceUtils.viewSourceInBrowser(args);
    } else {
      top.gViewSourceUtils.viewSource(args);
    }
  }

  // Check if external view source is enabled.  If so, try it.  If it fails,
  // fallback to internal view source.
  if (Services.prefs.getBoolPref("view_source.editor.external")) {
    top.gViewSourceUtils
       .openInExternalEditor(args, null, null, null, result => {
      if (!result) {
        viewInternal();
      }
    });
  } else {
    // Display using internal view source
    viewInternal();
  }
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

// documentURL - URL of the document to view, or null for this window's document
// initialTab - name of the initial tab to display, or null for the first tab
// imageElement - image to load in the Media Tab of the Page Info window; can be null/omitted
// frameOuterWindowID - the id of the frame that the context menu opened in; can be null/omitted
function BrowserPageInfo(documentURL, initialTab, imageElement, frameOuterWindowID) {
  if (documentURL instanceof HTMLDocument) {
    Deprecated.warning("Please pass the location URL instead of the document " +
                       "to BrowserPageInfo() as the first argument.",
                       "https://bugzilla.mozilla.org/show_bug.cgi?id=1238180");
    documentURL = documentURL.location;
  }

  let args = { initialTab, imageElement, frameOuterWindowID };
  var windows = Services.wm.getEnumerator("Browser:page-info");

  documentURL = documentURL || window.gBrowser.selectedBrowser.currentURI.spec;

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
    // 1. only if there's no opener (bug 370555).
    // 2. if remote newtab is enabled and it's the default remote newtab page
    let defaultRemoteURL = gAboutNewTabService.remoteEnabled &&
                           uri.spec === gAboutNewTabService.newTabURL;
    if ((gInitialPages.includes(uri.spec) || defaultRemoteURL) &&
        checkEmptyPageOrigin(gBrowser.selectedBrowser, uri)) {
      value = "";
    } else {
      // We should deal with losslessDecodeURI throwing for exotic URIs
      try {
        value = losslessDecodeURI(uri);
      } catch (ex) {
        value = "about:blank";
      }
    }

    valid = !isBlankPageURL(uri.spec);
  }

  gURLBar.value = value;
  gURLBar.valueIsTyped = !valid;
  SetPageProxyState(valid ? "valid" : "invalid");
}

function losslessDecodeURI(aURI) {
  let scheme = aURI.scheme;
  if (scheme == "moz-action")
    throw new Error("losslessDecodeURI should never get a moz-action URI");

  var value = aURI.spec;

  let decodeASCIIOnly = !["https", "http", "file", "ftp"].includes(scheme);
  // Try to decode as UTF-8 if there's no encoding sequence that we would break.
  if (!/%25(?:3B|2F|3F|3A|40|26|3D|2B|24|2C|23)/i.test(value)) {
    if (decodeASCIIOnly) {
      // This only decodes ascii characters (hex) 20-7e, except 25 (%).
      // This avoids both cases stipulated below (%-related issues, and \r, \n
      // and \t, which would be %0d, %0a and %09, respectively) as well as any
      // non-US-ascii characters.
      value = value.replace(/%(2[0-4]|2[6-9a-f]|[3-6][0-9a-f]|7[0-9a-e])/g, decodeURI);
    } else {
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
    }
  }

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
  if (!gURLBar)
    return;

  gURLBar.setAttribute("pageproxystate", aState);

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

var gMenuButtonBadgeManager = {
  BADGEID_APPUPDATE: "update",
  BADGEID_DOWNLOAD: "download",
  BADGEID_FXA: "fxa",

  fxaBadge: null,
  downloadBadge: null,
  appUpdateBadge: null,

  init: function () {
    PanelUI.panel.addEventListener("popupshowing", this, true);
  },

  uninit: function () {
    PanelUI.panel.removeEventListener("popupshowing", this, true);
  },

  handleEvent: function (e) {
    if (e.type === "popupshowing") {
      this.clearBadges();
    }
  },

  _showBadge: function () {
    let badgeToShow = this.downloadBadge || this.appUpdateBadge || this.fxaBadge;

    if (badgeToShow) {
      PanelUI.menuButton.setAttribute("badge-status", badgeToShow);
    } else {
      PanelUI.menuButton.removeAttribute("badge-status");
    }
  },

  _changeBadge: function (badgeId, badgeStatus = null) {
    if (badgeId == this.BADGEID_APPUPDATE) {
      this.appUpdateBadge = badgeStatus;
    } else if (badgeId == this.BADGEID_DOWNLOAD) {
      this.downloadBadge = badgeStatus;
    } else if (badgeId == this.BADGEID_FXA) {
      this.fxaBadge = badgeStatus;
    } else {
      Cu.reportError("The badge ID '" + badgeId + "' is unknown!");
    }
    this._showBadge();
  },

  addBadge: function (badgeId, badgeStatus) {
    if (!badgeStatus) {
      Cu.reportError("badgeStatus must be defined");
      return;
    }
    this._changeBadge(badgeId, badgeStatus);
  },

  removeBadge: function (badgeId) {
    this._changeBadge(badgeId);
  },

  clearBadges: function () {
    this.appUpdateBadge = null;
    this.downloadBadge = null;
    this.fxaBadge = null;
    this._showBadge();
  }
};

// Setup the hamburger button badges for updates, if enabled.
var gMenuButtonUpdateBadge = {
  enabled: false,
  badgeWaitTime: 0,
  timer: null,

  init: function () {
    try {
      this.enabled = Services.prefs.getBoolPref("app.update.badge");
    } catch (e) {}
    if (this.enabled) {
      try {
        this.badgeWaitTime = Services.prefs.getIntPref("app.update.badgeWaitTime");
      } catch (e) {
        this.badgeWaitTime = 345600; // 4 days
      }
      Services.obs.addObserver(this, "update-staged", false);
      Services.obs.addObserver(this, "update-downloaded", false);
    }
  },

  uninit: function () {
    if (this.timer)
      this.timer.cancel();
    if (this.enabled) {
      Services.obs.removeObserver(this, "update-staged");
      Services.obs.removeObserver(this, "update-downloaded");
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
    if (status == "failed") {
      // Background update has failed, let's show the UI responsible for
      // prompting the user to update manually.
      this.displayBadge(false);
      this.uninit();
      return;
    }

    // Give the user badgeWaitTime seconds to react before prompting.
    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.timer.initWithCallback(this, this.badgeWaitTime * 1000,
                                this.timer.TYPE_ONE_SHOT);
    // The timer callback will call uninit() when it completes.
  },

  notify: function () {
    // If the update is successfully applied, or if the updater has fallen back
    // to non-staged updates, add a badge to the hamburger menu to indicate an
    // update will be applied once the browser restarts.
    this.displayBadge(true);
    this.uninit();
  },

  displayBadge: function (succeeded) {
    let status = succeeded ? "succeeded" : "failed";
    let badgeStatus = "update-" + status;
    gMenuButtonBadgeManager.addBadge(gMenuButtonBadgeManager.BADGEID_APPUPDATE, badgeStatus);

    let stringId;
    let updateButtonText;
    if (succeeded) {
      let brandBundle = document.getElementById("bundle_brand");
      let brandShortName = brandBundle.getString("brandShortName");
      stringId = "appmenu.restartNeeded.description";
      updateButtonText = gNavigatorBundle.getFormattedString(stringId,
                                                             [brandShortName]);
    } else {
      stringId = "appmenu.updateFailed.description";
      updateButtonText = gNavigatorBundle.getString(stringId);
    }

    let updateButton = document.getElementById("PanelUI-update-status");
    updateButton.setAttribute("label", updateButtonText);
    updateButton.setAttribute("update-status", status);
    updateButton.hidden = false;
  }
};

// Values for telemtery bins: see TLS_ERROR_REPORT_UI in Histograms.json
const TLS_ERROR_REPORT_TELEMETRY_AUTO_CHECKED   = 2;
const TLS_ERROR_REPORT_TELEMETRY_AUTO_UNCHECKED = 3;
const TLS_ERROR_REPORT_TELEMETRY_MANUAL_SEND    = 4;
const TLS_ERROR_REPORT_TELEMETRY_AUTO_SEND      = 5;

const PREF_SSL_IMPACT_ROOTS = ["security.tls.version.min", "security.tls.version.max", "security.ssl3."];

const PREF_SSL_IMPACT = PREF_SSL_IMPACT_ROOTS.reduce((prefs, root) => {
  return prefs.concat(Services.prefs.getChildList(root));
}, []);

/**
 * Handle command events bubbling up from error page content
 * or from about:newtab or from remote error pages that invoke
 * us via async messaging.
 */
var BrowserOnClick = {
  init: function () {
    let mm = window.messageManager;
    mm.addMessageListener("Browser:CertExceptionError", this);
    mm.addMessageListener("Browser:SiteBlockedError", this);
    mm.addMessageListener("Browser:EnableOnlineMode", this);
    mm.addMessageListener("Browser:SendSSLErrorReport", this);
    mm.addMessageListener("Browser:SetSSLErrorReportAuto", this);
    mm.addMessageListener("Browser:ResetSSLPreferences", this);
    mm.addMessageListener("Browser:SSLErrorReportTelemetry", this);
    mm.addMessageListener("Browser:OverrideWeakCrypto", this);
    mm.addMessageListener("Browser:SSLErrorGoBack", this);
  },

  uninit: function () {
    let mm = window.messageManager;
    mm.removeMessageListener("Browser:CertExceptionError", this);
    mm.removeMessageListener("Browser:SiteBlockedError", this);
    mm.removeMessageListener("Browser:EnableOnlineMode", this);
    mm.removeMessageListener("Browser:SendSSLErrorReport", this);
    mm.removeMessageListener("Browser:SetSSLErrorReportAuto", this);
    mm.removeMessageListener("Browser:ResetSSLPreferences", this);
    mm.removeMessageListener("Browser:SSLErrorReportTelemetry", this);
    mm.removeMessageListener("Browser:OverrideWeakCrypto", this);
    mm.removeMessageListener("Browser:SSLErrorGoBack", this);
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
        this.onCertError(msg.target, msg.data.elementId,
                         msg.data.isTopFrame, msg.data.location,
                         msg.data.securityInfoAsString);
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
        this.onSSLErrorReport(msg.target,
                              msg.data.uri,
                              msg.data.securityInfo);
      break;
      case "Browser:ResetSSLPreferences":
        for (let prefName of PREF_SSL_IMPACT) {
          Services.prefs.clearUserPref(prefName);
        }
        msg.target.reload();
      break;
      case "Browser:SetSSLErrorReportAuto":
        Services.prefs.setBoolPref("security.ssl.errorReporting.automatic", msg.json.automatic);
        let bin = TLS_ERROR_REPORT_TELEMETRY_AUTO_UNCHECKED;
        if (msg.json.automatic) {
          bin = TLS_ERROR_REPORT_TELEMETRY_AUTO_CHECKED;
        }
        Services.telemetry.getHistogramById("TLS_ERROR_REPORT_UI").add(bin);
      break;
      case "Browser:SSLErrorReportTelemetry":
        let reportStatus = msg.data.reportStatus;
        Services.telemetry.getHistogramById("TLS_ERROR_REPORT_UI")
          .add(reportStatus);
      break;
      case "Browser:OverrideWeakCrypto":
        let weakCryptoOverride = Cc["@mozilla.org/security/weakcryptooverride;1"]
                                   .getService(Ci.nsIWeakCryptoOverride);
        weakCryptoOverride.addWeakCryptoOverride(
          msg.data.uri.host,
          PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser));
      break;
      case "Browser:SSLErrorGoBack":
        goBackFromErrorPage();
      break;
    }
  },

  onSSLErrorReport: function(browser, uri, securityInfo) {
    if (!Services.prefs.getBoolPref("security.ssl.errorReporting.enabled")) {
      Cu.reportError("User requested certificate error report sending, but certificate error reporting is disabled");
      return;
    }

    let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                           .getService(Ci.nsISerializationHelper);
    let transportSecurityInfo = serhelper.deserializeObject(securityInfo);
    transportSecurityInfo.QueryInterface(Ci.nsITransportSecurityInfo)

    let errorReporter = Cc["@mozilla.org/securityreporter;1"]
                          .getService(Ci.nsISecurityReporter);
    errorReporter.reportTLSError(transportSecurityInfo,
                                 uri.host, uri.port);
  },

  onCertError: function (browser, elementId, isTopFrame, location, securityInfoAsString) {
    let secHistogram = Services.telemetry.getHistogramById("SECURITY_UI");
    let securityInfo;

    switch (elementId) {
      case "exceptionDialogButton":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_CLICK_ADD_EXCEPTION);
        }

        securityInfo = getSecurityInfo(securityInfoAsString);
        let sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                    .SSLStatus;
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

      case "returnButton":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_GET_ME_OUT_OF_HERE);
        }
        goBackFromErrorPage();
        break;

      case "advancedButton":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_UNDERSTAND_RISKS);
        }

        securityInfo = getSecurityInfo(securityInfoAsString);
        let errorInfo = getDetailedCertErrorInfo(location,
                                                 securityInfo);
        browser.messageManager.sendAsyncMessage( "CertErrorDetails", {
            code: securityInfo.errorCode,
            info: errorInfo
        });
        break;

      case "copyToClipboard":
        const gClipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"]
                                    .getService(Ci.nsIClipboardHelper);
        securityInfo = getSecurityInfo(securityInfoAsString);
        let detailedInfo = getDetailedCertErrorInfo(location,
                                                    securityInfo);
        gClipboardHelper.copyString(detailedInfo);
        break;

    }
  },

  onAboutBlocked: function (elementId, reason, isTopFrame, location) {
    // Depending on what page we are displaying here (malware/phishing/unwanted)
    // use the right strings and links for each.
    let bucketName = "";
    let sendTelemetry = false;
    if (reason === 'malware') {
      sendTelemetry = true;
      bucketName = "WARNING_MALWARE_PAGE_";
    } else if (reason === 'phishing') {
      sendTelemetry = true;
      bucketName = "WARNING_PHISHING_PAGE_";
    } else if (reason === 'unwanted') {
      sendTelemetry = true;
      bucketName = "WARNING_UNWANTED_PAGE_";
    }
    let secHistogram = Services.telemetry.getHistogramById("SECURITY_UI");
    let nsISecTel = Ci.nsISecurityUITelemetry;
    bucketName += isTopFrame ? "TOP_" : "FRAME_";
    switch (elementId) {
      case "getMeOutButton":
        if (sendTelemetry) {
          secHistogram.add(nsISecTel[bucketName + "GET_ME_OUT_OF_HERE"]);
        }
        getMeOutOfHere();
        break;

      case "reportButton":
        // This is the "Why is this site blocked" button. We redirect
        // to the generic page describing phishing/malware protection.

        // We log even if malware/phishing/unwanted info URL couldn't be found:
        // the measurement is for how many users clicked the WHY BLOCKED button
        if (sendTelemetry) {
          secHistogram.add(nsISecTel[bucketName + "WHY_BLOCKED"]);
        }
        openHelpLink("phishing-malware", false, "current");
        break;

      case "ignoreWarningButton":
        if (gPrefService.getBoolPref("browser.safebrowsing.allowOverride")) {
          if (sendTelemetry) {
            secHistogram.add(nsISecTel[bucketName + "IGNORE_WARNING"]);
          }
          this.ignoreWarningButton(reason);
        }
        break;

      case "whyForbiddenButton":
        // This is the "Why is this site blocked" button for family friendly browsing
        // for Fennec. There's no desktop focused support page yet.
        gBrowser.loadURI("https://support.mozilla.org/kb/controlledaccess");
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
          openUILinkIn(gSafeBrowsing.getReportURL('MalwareMistake'), 'tab');
        }
      };
    } else if (reason === 'phishing') {
      title = gNavigatorBundle.getString("safebrowsing.deceptiveSite");
      buttons[1] = {
        label: gNavigatorBundle.getString("safebrowsing.notADeceptiveSiteButton.label"),
        accessKey: gNavigatorBundle.getString("safebrowsing.notADeceptiveSiteButton.accessKey"),
        callback: function() {
          openUILinkIn(gSafeBrowsing.getReportURL('PhishMistake'), 'tab');
        }
      };
    } else if (reason === 'unwanted') {
      title = gNavigatorBundle.getString("safebrowsing.reportedUnwantedSite");
      // There is no button for reporting errors since Google doesn't currently
      // provide a URL endpoint for these reports.
    } else {
      return; // no notifications for forbidden sites
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
  gBrowser.loadURI(getDefaultHomePage());
}

/**
 * Re-direct the browser to the previous page or a known-safe page if no
 * previous page is found in history.  This function is used when the user
 * browses to a secure page with certificate issues and is presented with
 * about:certerror.  The "Go Back" button should take the user to the previous
 * or a default start page so that even when their own homepage is on a server
 * that has certificate errors, we can get them somewhere safe.
 */
function goBackFromErrorPage() {
  const ss = Cc["@mozilla.org/browser/sessionstore;1"].
             getService(Ci.nsISessionStore);
  let state = JSON.parse(ss.getTabState(gBrowser.selectedTab));
  if (state.index == 1) {
    // If the unsafe page is the first or the only one in history, go to the
    // start page.
    gBrowser.loadURI(getDefaultHomePage());
  } else {
    BrowserBack();
  }
}

/**
 * Return the default start page for the cases when the user's own homepage is
 * infected, so we can get them somewhere safe.
 */
function getDefaultHomePage() {
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
  return url;
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
}

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

function getSecurityInfo(securityInfoAsString) {
  if (!securityInfoAsString)
    return null;

  const serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                       .getService(Ci.nsISerializationHelper);
  let securityInfo = serhelper.deserializeObject(securityInfoAsString);
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);

  return securityInfo;
}

/**
 * Returns a string with detailed information about the certificate validation
 * failure from the specified URI that can be used to send a report.
 */
function getDetailedCertErrorInfo(location, securityInfo) {
  if (!securityInfo)
    return "";

  let certErrorDetails = location;
  let code = securityInfo.errorCode;
  let errors = Cc["@mozilla.org/nss_errors_service;1"]
                  .getService(Ci.nsINSSErrorsService);

  certErrorDetails += "\r\n\r\n" + errors.getErrorMessage(errors.getXPCOMFromNSSError(code));

  const sss = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  // SiteSecurityService uses different storage if the channel is
  // private. Thus we must give isSecureHost correct flags or we
  // might get incorrect results.
  let flags = PrivateBrowsingUtils.isWindowPrivate(window) ?
              Ci.nsISocketProvider.NO_PERMANENT_STORAGE : 0;

  let uri = Services.io.newURI(location, null, null);

  let hasHSTS = sss.isSecureHost(sss.HEADER_HSTS, uri.host, flags);
  let hasHPKP = sss.isSecureHost(sss.HEADER_HPKP, uri.host, flags);
  certErrorDetails += "\r\n\r\n" +
                      gNavigatorBundle.getFormattedString("certErrorDetailsHSTS.label",
                                                          [hasHSTS]);
  certErrorDetails += "\r\n" +
                      gNavigatorBundle.getFormattedString("certErrorDetailsKeyPinning.label",
                                                          [hasHPKP]);

  let certChain = "";
  if (securityInfo.failedCertChain) {
    let certs = securityInfo.failedCertChain.getEnumerator();
    while (certs.hasMoreElements()) {
      let cert = certs.getNext();
      cert.QueryInterface(Ci.nsIX509Cert);
      certChain += getPEMString(cert);
    }
  }

  certErrorDetails += "\r\n\r\n" +
                      gNavigatorBundle.getString("certErrorDetailsCertChain.label") +
                      "\r\n\r\n" + certChain;

  return certErrorDetails;
}

// TODO: can we pull getDERString and getPEMString in from pippki.js instead of
// duplicating them here?
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

function getPEMString(cert)
{
  var derb64 = btoa(getDERString(cert));
  // Wrap the Base64 string into lines of 64 characters,
  // with CRLF line breaks (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return "-----BEGIN CERTIFICATE-----\r\n"
         + wrapped
         + "\r\n-----END CERTIFICATE-----\r\n";
}

var PrintPreviewListener = {
  _printPreviewTab: null,
  _tabBeforePrintPreview: null,

  getPrintPreviewBrowser: function () {
    if (!this._printPreviewTab) {
      let browser = gBrowser.selectedTab.linkedBrowser;
      let forceNotRemote = gMultiProcessBrowser && !browser.isRemoteBrowser;
      this._tabBeforePrintPreview = gBrowser.selectedTab;
      this._printPreviewTab = gBrowser.loadOneTab("about:blank",
                                                  { inBackground: false,
                                                    forceNotRemote });
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
  canDropLink: aEvent => Services.droppedLinkHandler.canDropLink(aEvent, true),

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
        this.setIcon(aMsg.target, aMsg.data.url, aMsg.data.loadingPrincipal);
        break;

      case "Link:AddSearch":
        this.addSearch(aMsg.target, aMsg.data.engine, aMsg.data.url);
        break;
    }
  },

  setIcon: function(aBrowser, aURL, aLoadingPrincipal) {
    if (gBrowser.isFailedIcon(aURL))
      return false;

    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab)
      return false;

    gBrowser.setIcon(tab, aURL, aLoadingPrincipal);
    return true;
  },

  addSearch: function(aBrowser, aEngine, aURL) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab)
      return;

    BrowserSearch.addEngine(aBrowser, aEngine, makeURI(aURL));
  },
}

const BrowserSearch = {
  addEngine: function(browser, engine, uri) {
    // Check to see whether we've already added an engine with this title
    if (browser.engines) {
      if (browser.engines.some(e => e.title == engine.title))
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
        this.updateOpenSearchBadge();
    }
  },

  /**
   * Update the browser UI to show whether or not additional engines are
   * available when a page is loaded or the user switches tabs to a page that
   * has search engines.
   */
  updateOpenSearchBadge: function() {
    var searchBar = this.searchBar;
    if (!searchBar)
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

    let openSearchPageIfFieldIsNotActive = function(aSearchBar) {
      if (!aSearchBar || document.activeElement != aSearchBar.textbox.inputField) {
        let url = gBrowser.currentURI.spec.toLowerCase();
        let mm = gBrowser.selectedBrowser.messageManager;
        let newTabRemoted = Services.prefs.getBoolPref("browser.newtabpage.remote");
        let localNewTabEnabled = url === "about:newtab" && !newTabRemoted && NewTabUtils.allPages.enabled;
        if (url === "about:home" || localNewTabEnabled) {
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
      BrowserSearch.recordSearchInTelemetry(engine, "contextmenu");
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

  get _isExtendedTelemetryEnabled() {
    return Services.prefs.getBoolPref("toolkit.telemetry.enabled");
  },

  _getSearchEngineId: function (engine) {
    if (engine && engine.identifier) {
      return engine.identifier;
    }

    if (!engine || (engine.name === undefined) || !this._isExtendedTelemetryEnabled)
      return "other";

    return "other-" + engine.name;
  },

  /**
   * Helper to record a search with Telemetry.
   *
   * Telemetry records only search counts and nothing pertaining to the search itself.
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
  recordSearchInTelemetry: function (engine, source, selection) {
    const SOURCES = [
      "abouthome",
      "contextmenu",
      "newtab",
      "searchbar",
      "urlbar",
    ];

    BrowserUITelemetry.countSearchEvent(source, null, selection);

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

XPCOMUtils.defineConstant(this, "BrowserSearch", BrowserSearch);

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
  let children = aParent.childNodes;
  for (var i = children.length - 1; i >= 0; --i) {
    if (children[i].hasAttribute("index"))
      aParent.removeChild(children[i]);
  }

  const MAX_HISTORY_MENU_ITEMS = 15;

  const tooltipBack = gNavigatorBundle.getString("tabHistory.goBack");
  const tooltipCurrent = gNavigatorBundle.getString("tabHistory.current");
  const tooltipForward = gNavigatorBundle.getString("tabHistory.goForward");

  function updateSessionHistory(sessionHistory, initial)
  {
    let count = sessionHistory.entries.length;

    if (!initial) {
      if (count <= 1) {
        // if there is only one entry now, close the popup.
        aParent.hidePopup();
        return;
      } else if (aParent.id != "backForwardMenu" && !aParent.parentNode.open) {
        // if the popup wasn't open before, but now needs to be, reopen the menu.
        // It should trigger FillHistoryMenu again. This might happen with the
        // delay from click-and-hold menus but skip this for the context menu
        // (backForwardMenu) rather than figuring out how the menu should be
        // positioned and opened as it is an extreme edgecase.
        aParent.parentNode.open = true;
        return;
      }
    }

    let index = sessionHistory.index;
    let half_length = Math.floor(MAX_HISTORY_MENU_ITEMS / 2);
    let start = Math.max(index - half_length, 0);
    let end = Math.min(start == 0 ? MAX_HISTORY_MENU_ITEMS : index + half_length + 1, count);
    if (end == count) {
      start = Math.max(count - MAX_HISTORY_MENU_ITEMS, 0);
    }

    let existingIndex = 0;

    for (let j = end - 1; j >= start; j--) {
      let entry = sessionHistory.entries[j];
      let uri = entry.url;

      let item = existingIndex < children.length ?
                   children[existingIndex] : document.createElement("menuitem");

      let entryURI = BrowserUtils.makeURI(entry.url, entry.charset, null);
      item.setAttribute("uri", uri);
      item.setAttribute("label", entry.title || uri);
      item.setAttribute("index", j);

      // Cache this so that gotoHistoryIndex doesn't need the original index
      item.setAttribute("historyindex", j - index);

      if (j != index) {
        PlacesUtils.favicons.getFaviconURLForPage(entryURI, function (aURI) {
          if (aURI) {
            let iconURL = PlacesUtils.favicons.getFaviconLinkForIcon(aURI).spec;
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

      if (!item.parentNode) {
        aParent.appendChild(item);
      }

      existingIndex++;
    }

    if (!initial) {
      let existingLength = children.length;
      while (existingIndex < existingLength) {
        aParent.removeChild(aParent.lastChild);
        existingIndex++;
      }
    }
  }

  let sessionHistory = SessionStore.getSessionHistory(gBrowser.selectedTab, updateSessionHistory);
  if (!sessionHistory)
    return false;

  // don't display the popup for a single item
  if (sessionHistory.entries.length <= 1)
    return false;

  updateSessionHistory(sessionHistory, true);
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
    extraFeatures += ",remote";
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
  if (AppConstants.platform == "macosx")
    return;

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
}

/**
 * Opens a new tab with the userContextId specified as an attribute of
 * sourceEvent. This attribute is propagated to the top level originAttributes
 * living on the tab's docShell.
 *
 * @param event
 *        A click event on a userContext File Menu option
 */
function openNewUserContextTab(event)
{
  openUILinkIn(BROWSER_NEW_TAB_URL, "tab", {
    userContextId: parseInt(event.target.getAttribute('usercontextid')),
  });
}

/**
 * Updates File Menu User Context UI visibility depending on
 * privacy.userContext.enabled pref state.
 */
function updateUserContextUIVisibility()
{
  let menu = document.getElementById("menu_newUserContext");
  menu.hidden = !Services.prefs.getBoolPref("privacy.userContext.enabled");
  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    menu.setAttribute("disabled", "true");
  }
}

/**
 * Updates the User Context UI indicators if the browser is in a non-default context
 */
function updateUserContextUIIndicator(browser)
{
  let hbox = document.getElementById("userContext-icons");

  let userContextId = browser.getAttribute("usercontextid");
  if (!userContextId) {
    hbox.hidden = true;
    return;
  }

  let identity = ContextualIdentityService.getIdentityFromId(userContextId);
  if (!identity) {
    hbox.hidden = true;
    return;
  }

  let label = document.getElementById("userContext-label");
  label.value = ContextualIdentityService.getUserContextLabel(userContextId);
  label.style.color = identity.color;

  let indicator = document.getElementById("userContext-indicator");
  indicator.style.listStyleImage = "url(" + identity.icon + ")";

  hbox.hidden = false;
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
  get canViewSource () {
    delete this.canViewSource;
    return this.canViewSource = document.getElementById("canViewSource");
  },

  init: function () {
    // Initialize the security button's state and tooltip text.
    var securityUI = gBrowser.securityUI;
    this.onSecurityChange(null, null, securityUI.state, true);
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

  showTooltip: function (x, y, tooltip, direction) {
    if (Cc["@mozilla.org/widget/dragservice;1"].getService(Ci.nsIDragService).
        getCurrentSession()) {
      return;
    }

    // The x,y coordinates are relative to the <browser> element using
    // the chrome zoom level.
    let elt = document.getElementById("remoteBrowserTooltip");
    elt.label = tooltip;
    elt.style.direction = direction;

    let anchor = gBrowser.selectedBrowser;
    elt.openPopupAtScreen(anchor.boxObject.screenX + x, anchor.boxObject.screenY + y, false, null);
  },

  hideTooltip: function () {
    let elt = document.getElementById("remoteBrowserTooltip");
    elt.hidePopup();
  },

  getTabCount: function () {
    return gBrowser.tabs.length;
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
        let canViewSource = true;
        // Get the URI either from a channel or a pseudo-object
        if (aRequest instanceof nsIChannel || "URI" in aRequest) {
          location = aRequest.URI;

          // For keyword URIs clear the user typed value since they will be changed into real URIs
          if (location.scheme == "keyword" && aWebProgress.isTopLevel)
            gBrowser.userTypedValue = null;

          canViewSource = !Services.prefs.getBoolPref("view_source.tab") ||
                          location.scheme != "view-source";

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
        if (browser.documentContentType && BrowserUtils.mimeTypeIsTextBased(browser.documentContentType)) {
          this.isImage.removeAttribute('disabled');
        } else {
          canViewSource = false;
          this.isImage.setAttribute('disabled', 'true');
        }

        if (canViewSource) {
          this.canViewSource.removeAttribute('disabled');
        } else {
          this.canViewSource.setAttribute('disabled', 'true');
        }
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
      if ((location == "about:blank" && checkEmptyPageOrigin()) ||
          location == "") {  // Second condition is for new tabs, otherwise
                             // reload function is enabled until tab is refreshed.
        this.reloadCommand.setAttribute("disabled", "true");
      } else {
        this.reloadCommand.removeAttribute("disabled");
      }

      URLBarSetURI(aLocationURI);

      BookmarkingUI.onLocationChange();

      SocialUI.updateState();

      UITour.onLocationChange(location);

      gTabletModePageCounter.inc();

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
      if (location == "about:blank" &&
          gBrowser.selectedTab.hasAttribute("customizemode")) {
        gCustomizeMode.enter();
      } else if (CustomizationHandler.isEnteringCustomizeMode ||
                 CustomizationHandler.isCustomizing()) {
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

    if (AppConstants.MOZ_CRASHREPORTER && aLocationURI) {
      let uri = aLocationURI.clone();
      try {
        // If the current URI contains a username/password, remove it.
        uri.userPass = "";
      } catch (ex) { /* Ignore failures on about: URIs. */ }

      try {
        gCrashReporter.annotateCrashReport("URL", uri.spec);
      } catch (ex) {
        // Don't make noise when the crash reporter is built but not enabled.
        if (ex.result != Components.results.NS_ERROR_NOT_INITIALIZED) {
          throw ex;
        }
      }
    }
  },

  asyncUpdateUI: function () {
    FeedHandler.updateFeeds();
    BrowserSearch.updateOpenSearchBadge();
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

  // This is called in multiple ways:
  //  1. Due to the nsIWebProgressListener.onSecurityChange notification.
  //  2. Called by tabbrowser.xml when updating the current browser.
  //  3. Called directly during this object's initializations.
  // aRequest will be null always in case 2 and 3, and sometimes in case 1 (for
  // instance, there won't be a request when STATE_BLOCKED_TRACKING_CONTENT is observed).
  onSecurityChange: function (aWebProgress, aRequest, aState, aIsSimulated) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    let uri = gBrowser.currentURI;
    let spec = uri.spec;
    if (this._state == aState &&
        this._lastLocation == spec)
      return;
    this._state = aState;
    this._lastLocation = spec;

    if (typeof(aIsSimulated) != "boolean" && typeof(aIsSimulated) != "undefined") {
      throw "onSecurityChange: aIsSimulated receieved an unexpected type";
    }

    // Make sure the "https" part of the URL is striked out or not,
    // depending on the current mixed active content blocking state.
    gURLBar.formatValue();

    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}
    gIdentityHandler.updateIdentity(this._state, uri);
    TrackingProtection.onSecurityChange(this._state, aIsSimulated);
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

  get _isVisible () {
    return XULBrowserWindow.statusTextField.label != "";
  },

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
  // Keep track of which browsers we've started load timers for, since
  // we won't see STATE_START events for pre-rendered tabs.
  _startedLoadTimer: new WeakSet(),

  onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    // Collect telemetry data about tab load times.
    if (aWebProgress.isTopLevel && (!aRequest.originalURI || aRequest.originalURI.spec.scheme != "about")) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
          this._startedLoadTimer.add(aBrowser);
          TelemetryStopwatch.start("FX_PAGE_LOAD_MS", aBrowser);
          Services.telemetry.getHistogramById("FX_TOTAL_TOP_VISITS").add(true);
        } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                   this._startedLoadTimer.has(aBrowser)) {
          this._startedLoadTimer.delete(aBrowser);
          TelemetryStopwatch.finish("FX_PAGE_LOAD_MS", aBrowser);
        }
      } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                 aStatus == Cr.NS_BINDING_ABORTED &&
                 this._startedLoadTimer.has(aBrowser)) {
        this._startedLoadTimer.delete(aBrowser);
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
      mm.sendAsyncMessage("Reader:PushState", {isArticle: gBrowser.selectedBrowser.isArticle});
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
}

function nsBrowserAccess() { }

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow, Ci.nsISupports]),

  _openURIInNewTab: function(aURI, aReferrer, aReferrerPolicy, aIsPrivate,
                             aIsExternal, aForceNotRemote=false,
                             aUserContextId=Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID) {
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
                                      userContextId: aUserContextId,
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
    let isPrivate = aOpener
                  ? PrivateBrowsingUtils.isContentWindowPrivate(aOpener)
                  : PrivateBrowsingUtils.isWindowPrivate(window);

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
        let userContextId = aOpener && aOpener.document
                              ? aOpener.document.nodePrincipal.originAttributes.userContextId
                              : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
        let browser = this._openURIInNewTab(aURI, referrer, referrerPolicy,
                                            isPrivate, isExternal,
                                            forceNotRemote, userContextId);
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

    var userContextId = aParams.openerOriginAttributes &&
                        ("userContextId" in aParams.openerOriginAttributes)
                          ? aParams.openerOriginAttributes.userContextId
                          : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID

    let browser = this._openURIInNewTab(aURI, aParams.referrer,
                                        aParams.referrerPolicy,
                                        aParams.isPrivate,
                                        isExternal, false,
                                        userContextId);
    if (browser)
      return browser.QueryInterface(Ci.nsIFrameLoaderOwner);

    return null;
  },

  isTabContentWindow: function (aWindow) {
    return gBrowser.browsers.some(browser => browser.contentWindow == aWindow);
  },

  canClose() {
    return CanCloseWindow();
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
    if (AppConstants.platform == "linux") {
      Services.prefs.setBoolPref("ui.key.menuAccessKeyFocuses", !isVisible);
    }
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

var TabletModeUpdater = {
  init() {
    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      this.update(WindowsUIUtils.inTabletMode);
      Services.obs.addObserver(this, "tablet-mode-change", false);
    }
  },

  uninit() {
    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      Services.obs.removeObserver(this, "tablet-mode-change");
    }
  },

  observe(subject, topic, data) {
    this.update(data == "tablet-mode");
  },

  update(isInTabletMode) {
    let wasInTabletMode = document.documentElement.hasAttribute("tabletmode");
    if (isInTabletMode) {
      document.documentElement.setAttribute("tabletmode", "true");
    } else {
      document.documentElement.removeAttribute("tabletmode");
    }
    if (wasInTabletMode != isInTabletMode) {
      TabsInTitlebar.updateAppearance(true);
    }
  },
};

var gTabletModePageCounter = {
  enabled: false,
  inc() {
    this.enabled = AppConstants.isPlatformAndVersionAtLeast("win", "10.0");
    if (!this.enabled) {
      this.inc = () => {};
      return;
    }
    this.inc = this._realInc;
    this.inc();
  },

  _desktopCount: 0,
  _tabletCount: 0,
  _realInc() {
    let inTabletMode = document.documentElement.hasAttribute("tabletmode");
    this[inTabletMode ? "_tabletCount" : "_desktopCount"]++;
  },

  finish() {
    if (this.enabled) {
      let histogram = Services.telemetry.getKeyedHistogramById("FX_TABLETMODE_PAGE_LOAD");
      histogram.add("tablet", this._tabletCount);
      histogram.add("desktop", this._desktopCount);
    }
  },
};

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
};

const nodeToTooltipMap = {
  "bookmarks-menu-button": "bookmarksMenuButton.tooltip",
  "new-window-button": "newWindowButton.tooltip",
  "new-tab-button": "newTabButton.tooltip",
  "tabs-newtab-button": "newTabButton.tooltip",
  "fullscreen-button": "fullscreenButton.tooltip",
  "downloads-button": "downloads.tooltip",
};
const nodeToShortcutMap = {
  "bookmarks-menu-button": "manBookmarkKb",
  "new-window-button": "key_newNavigator",
  "new-tab-button": "key_newNavigatorTab",
  "tabs-newtab-button": "key_newNavigatorTab",
  "fullscreen-button": "key_fullScreen",
  "downloads-button": "key_openDownloads"
};

if (AppConstants.platform == "macosx") {
  nodeToTooltipMap["print-button"] = "printButton.tooltip";
  nodeToShortcutMap["print-button"] = "printKb";
}

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
    if (node.nodeType == Node.ELEMENT_NODE &&
        (node.localName == "a" ||
         node.namespaceURI == "http://www.w3.org/1998/Math/MathML")) {
      href = node.getAttributeNS("http://www.w3.org/1999/xlink", "href");
      if (href) {
        baseURI = node.baseURI;
        break;
      }
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

  // first get document wide referrer policy, then
  // get referrer attribute from clicked link and parse it and
  // allow per element referrer to overrule the document wide referrer if enabled
  let referrerPolicy = doc.referrerPolicy;
  if (Services.prefs.getBoolPref("network.http.enablePerElementReferrer") &&
      linkNode) {
    let referrerAttrValue = Services.netUtils.parseAttributePolicyString(linkNode.
                            getAttribute("referrerpolicy"));
    if (referrerAttrValue != Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
      referrerPolicy = referrerAttrValue;
    }
  }

  urlSecurityCheck(href, doc.nodePrincipal);
  let params = { charset: doc.characterSet,
                 allowMixedContent: persistAllowMixedContentInChildTab,
                 referrerURI: referrerURI,
                 referrerPolicy: referrerPolicy,
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
}

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
  let selectedCharset = CharsetMenu.foldCharset(gBrowser.selectedBrowser.characterSet);
  for (let menuItem of target.getElementsByTagName("menuitem")) {
    let isSelected = menuItem.getAttribute("charset") === selectedCharset;
    menuItem.setAttribute("checked", isSelected);
  }
}

var gPageStyleMenu = {
  // This maps from a <browser> element (or, more specifically, a
  // browser's permanentKey) to an Object that contains the most recent
  // information about the browser content's stylesheets. That Object
  // is populated via the PageStyle:StyleSheets message from the content
  // process. The Object should have the following structure:
  //
  // filteredStyleSheets (Array):
  //   An Array of objects with a filtered list representing all stylesheets
  //   that the current page offers. Each object has the following members:
  //
  //   title (String):
  //     The title of the stylesheet
  //
  //   disabled (bool):
  //     Whether or not the stylesheet is currently applied
  //
  //   href (String):
  //     The URL of the stylesheet. Stylesheets loaded via a data URL will
  //     have this property set to null.
  //
  // authorStyleDisabled (bool):
  //   Whether or not the user currently has "No Style" selected for
  //   the current page.
  //
  // preferredStyleSheetSet (bool):
  //   Whether or not the user currently has the "Default" style selected
  //   for the current page.
  //
  _pageStyleSheets: new WeakMap(),

  init: function() {
    let mm = window.messageManager;
    mm.addMessageListener("PageStyle:StyleSheets", (msg) => {
      this._pageStyleSheets.set(msg.target.permanentKey, msg.data);
    });
  },

  /**
   * Returns an array of Objects representing stylesheets in a
   * browser. Note that the pageshow event needs to fire in content
   * before this information will be available.
   *
   * @param browser (optional)
   *        The <xul:browser> to search for stylesheets. If omitted, this
   *        defaults to the currently selected tab's browser.
   * @returns Array
   *        An Array of Objects representing stylesheets in the browser.
   *        See the documentation for gPageStyleMenu for a description
   *        of the Object structure.
   */
  getBrowserStyleSheets: function (browser) {
    if (!browser) {
      browser = gBrowser.selectedBrowser;
    }

    let data = this._pageStyleSheets.get(browser.permanentKey);
    if (!data) {
      return [];
    }
    return data.filteredStyleSheets;
  },

  _getStyleSheetInfo: function (browser) {
    let data = this._pageStyleSheets.get(browser.permanentKey);
    if (!data) {
      return {
        filteredStyleSheets: [],
        authorStyleDisabled: false,
        preferredStyleSheetSet: true
      };
    }

    return data;
  },

  fillPopup: function (menuPopup) {
    let styleSheetInfo = this._getStyleSheetInfo(gBrowser.selectedBrowser);
    var noStyle = menuPopup.firstChild;
    var persistentOnly = noStyle.nextSibling;
    var sep = persistentOnly.nextSibling;
    while (sep.nextSibling)
      menuPopup.removeChild(sep.nextSibling);

    let styleSheets = styleSheetInfo.filteredStyleSheets;
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

    // This notification is also received because of a loss in connectivity,
    // which we ignore by updating the UI to the current value of io.offline
    this._updateOfflineUI(Services.io.offline);
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
  warnUsage(browser, uri) {
    if (!browser)
      return;

    let mainAction = {
      label: gNavigatorBundle.getString("offlineApps.manageUsage"),
      accessKey: gNavigatorBundle.getString("offlineApps.manageUsageAccessKey"),
      callback: this.manage
    };

    let warnQuotaKB = Services.prefs.getIntPref("offline-apps.quota.warn");
    // This message shows the quota in MB, and so we divide the quota (in kb) by 1024.
    let message = gNavigatorBundle.getFormattedString("offlineApps.usage",
                                                      [ uri.host,
                                                        warnQuotaKB / 1024 ]);

    let anchorID = "indexedDB-notification-icon";
    PopupNotifications.show(browser, "offline-app-usage", message,
                            anchorID, mainAction);

    // Now that we've warned once, prevent the warning from showing up
    // again.
    Services.perms.add(uri, "offline-app",
                       Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);
  },

  // XXX: duplicated in preferences/advanced.js
  _getOfflineAppUsage(host, groups) {
    let cacheService = Cc["@mozilla.org/network/application-cache-service;1"].
                       getService(Ci.nsIApplicationCacheService);
    if (!groups) {
      try {
        groups = cacheService.getGroups();
      } catch (ex) {
        return 0;
      }
    }

    let usage = 0;
    for (let group of groups) {
      let uri = Services.io.newURI(group, null, null);
      if (uri.asciiHost == host) {
        let cache = cacheService.getActiveCache(group);
        usage += cache.usage;
      }
    }

    return usage;
  },

  _usedMoreThanWarnQuota(uri) {
    // if the user has already allowed excessive usage, don't bother checking
    if (Services.perms.testExactPermission(uri, "offline-app") !=
        Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN) {
      let usageBytes = this._getOfflineAppUsage(uri.asciiHost);
      let warnQuotaKB = Services.prefs.getIntPref("offline-apps.quota.warn");
      // The pref is in kb, the usage we get is in bytes, so multiply the quota
      // to compare correctly:
      if (usageBytes >= warnQuotaKB * 1024) {
        return true;
      }
    }

    return false;
  },

  requestPermission(browser, docId, uri) {
    let host = uri.asciiHost;
    let notificationID = "offline-app-requested-" + host;
    let notification = PopupNotifications.getNotification(notificationID, browser);

    if (notification) {
      notification.options.controlledItems.push([
        Cu.getWeakReference(browser), docId, uri
      ]);
    } else {
      let mainAction = {
        label: gNavigatorBundle.getString("offlineApps.allow"),
        accessKey: gNavigatorBundle.getString("offlineApps.allowAccessKey"),
        callback: function() {
          for (let [browser, docId, uri] of notification.options.controlledItems) {
            OfflineApps.allowSite(browser, docId, uri);
          }
        }
      };
      let secondaryActions = [{
        label: gNavigatorBundle.getString("offlineApps.never"),
        accessKey: gNavigatorBundle.getString("offlineApps.neverAccessKey"),
        callback: function() {
          for (let [, , uri] of notification.options.controlledItems) {
            OfflineApps.disallowSite(uri);
          }
        }
      }];
      let message = gNavigatorBundle.getFormattedString("offlineApps.available",
                                                        [host]);
      let anchorID = "indexedDB-notification-icon";
      let options = {
        controlledItems : [[Cu.getWeakReference(browser), docId, uri]]
      };
      notification = PopupNotifications.show(browser, notificationID, message,
                                             anchorID, mainAction,
                                             secondaryActions, options);
    }
  },

  disallowSite(uri) {
    Services.perms.add(uri, "offline-app", Services.perms.DENY_ACTION);
  },

  allowSite(browserRef, docId, uri) {
    Services.perms.add(uri, "offline-app", Services.perms.ALLOW_ACTION);

    // When a site is enabled while loading, manifest resources will
    // start fetching immediately.  This one time we need to do it
    // ourselves.
    let browser = browserRef.get();
    if (browser && browser.messageManager) {
      browser.messageManager.sendAsyncMessage("OfflineApps:StartFetching", {
        docId,
      });
    }
  },

  manage() {
    openAdvancedPreferences("networkTab");
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "OfflineApps:CheckUsage":
        let uri = makeURI(msg.data.uri);
        if (this._usedMoreThanWarnQuota(uri)) {
          this.warnUsage(msg.target, uri);
        }
        break;
      case "OfflineApps:RequestPermission":
        this.requestPermission(msg.target, msg.data.docId, makeURI(msg.data.uri));
        break;
    }
  },

  init() {
    let mm = window.messageManager;
    mm.addMessageListener("OfflineApps:CheckUsage", this);
    mm.addMessageListener("OfflineApps:RequestPermission", this);
  },
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

function CanCloseWindow()
{
  // Avoid redundant calls to canClose from showing multiple
  // PermitUnload dialogs.
  if (Services.startup.shuttingDown || window.skipNextCanClose) {
    return true;
  }

  for (let browser of gBrowser.browsers) {
    let {permitUnload, timedOut} = browser.permitUnload();
    if (timedOut) {
      return true;
    }
    if (!permitUnload) {
      return false;
    }
  }
  return true;
}

function WindowIsClosing()
{
  if (!closeWindow(false, warnAboutClosingWindow))
    return false;

  // In theory we should exit here and the Window's internal Close
  // method should trigger canClose on nsBrowserAccess. However, by
  // that point it's too late to be able to show a prompt for
  // PermitUnload. So we do it here, when we still can.
  if (CanCloseWindow()) {
    // This flag ensures that the later canClose call does nothing.
    // It's only needed to make tests pass, since they detect the
    // prompt even when it's not actually shown.
    window.skipNextCanClose = true;
    return true;
  }

  return false;
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

  // OS X doesn't quit the application when the last window is closed, but keeps
  // the session alive. Hence don't prompt users to save tabs, but warn about
  // closing multiple tabs.
  return AppConstants.platform != "macosx"
         || (isPBWindow || gBrowser.warnAboutClosingTabs(gBrowser.closingTabsEnum.ALL));
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
  return new Promise(resolve => {
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
        resolve(emWindow);
        return;
      }
    }

    switchToTabHavingURI("about:addons", true);

    if (aView) {
      // This must be a new load, else the ping/pong would have
      // found the window above.
      Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
        Services.obs.removeObserver(observer, aTopic);
        aSubject.loadView(aView);
        resolve(aSubject);
      }, "EM-loaded", false);
    } else {
      resolve();
    }
  });
}

function AddKeywordForSearchField() {
  let mm = gBrowser.selectedBrowser.messageManager;

  let onMessage = (message) => {
    mm.removeMessageListener("ContextMenu:SearchFieldBookmarkData:Result", onMessage);

    let bookmarkData = message.data;
    let title = gNavigatorBundle.getFormattedString("addKeywordTitleAutoFill",
                                                    [bookmarkData.title]);
    PlacesUIUtils.showBookmarkDialog({ action: "add"
                                     , type: "bookmark"
                                     , uri: makeURI(bookmarkData.spec)
                                     , title: title
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
  mm.addMessageListener("ContextMenu:SearchFieldBookmarkData:Result", onMessage);

  mm.sendAsyncMessage("ContextMenu:SearchFieldBookmarkData", {}, { target: gContextMenu.target });
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
  // Prevent an unnecessary blank tab left behind
  var blankTabToRemove = null;
  if (gBrowser.tabs.length == 1 && isTabEmpty(gBrowser.selectedTab))
    blankTabToRemove = gBrowser.selectedTab;

  var tab = null;
  if (SessionStore.getClosedTabCount(window) > (aIndex || 0)) {
    tab = SessionStore.undoCloseTab(window, aIndex || 0);

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

  if (aTab.hasAttribute("customizemode"))
    return false;

  // Tab may be in the early stages of being restored - so even though it's
  // currently blank, we don't want to treat it as such.
  if (SessionStore.isTabRestoring(aTab))
    return false;

  let browser = aTab.linkedBrowser;
  if (!isBlankPageURL(browser.currentURI.spec))
    return false;

  if (!checkEmptyPageOrigin(browser))
    return false;

  if (browser.canGoForward || browser.canGoBack)
    return false;

  return true;
}

/**
 * Check whether a page can be considered as 'empty', that its URI
 * reflects its origin, and that if it's loaded in a tab, that tab
 * could be considered 'empty' (e.g. like the result of opening
 * a 'blank' new tab).
 *
 * We have to do more than just check the URI, because especially
 * for things like about:blank, it is possible that the opener or
 * some other page has control over the contents of the page.
 *
 * @param browser {Browser}
 *        The browser whose page we're checking (the selected browser
 *        in this window if omitted).
 * @param uri {nsIURI}
 *        The URI against which we're checking (the browser's currentURI
 *        if omitted).
 *
 * @return false if the page was opened by or is controlled by arbitrary web
 *         content, unless that content corresponds with the URI.
 *         true if the page is blank and controlled by a principal matching
 *         that URI (or the system principal if the principal has no URI)
 */
function checkEmptyPageOrigin(browser = gBrowser.selectedBrowser,
                              uri = browser.currentURI) {
  // If another page opened this page with e.g. window.open, this page might
  // be controlled by its opener - return false.
  if (browser.hasContentOpener) {
    return false;
  }
  let contentPrincipal = browser.contentPrincipal;
  // Not all principals have URIs...
  if (contentPrincipal.URI) {
    // A manually entered about:blank URI is slightly magical:
    if (uri.spec == "about:blank" && contentPrincipal.isNullPrincipal) {
      return true;
    }
    return contentPrincipal.URI.equals(uri);
  }
  // ... so for those that don't have them, enforce that the page has the
  // system principal (this matches e.g. on about:newtab).
  let ssm = Services.scriptSecurityManager;
  return ssm.isSystemPrincipal(contentPrincipal);
}

function BrowserOpenSyncTabs() {
  gSyncUI.openSyncedTabsPanel();
}

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
  /**
   * nsIURI for which the identity UI is displayed. This has been already
   * processed by nsIURIFixup.createExposableURI.
   */
  _uri: null,

  /**
   * We only know the connection type if this._uri has a defined "host" part.
   *
   * These URIs, like "about:" and "data:" URIs, will usually be treated as a
   * non-secure connection, unless they refer to an internally implemented
   * browser page or resolve to "file:" URIs.
   */
  _uriHasHost: false,

  /**
   * Whether this._uri refers to an internally implemented browser page.
   *
   * Note that this is set for some "about:" pages, but general "chrome:" URIs
   * are not included in this category by default.
   */
  _isSecureInternalUI: false,

  /**
   * nsISSLStatus metadata provided by gBrowser.securityUI the last time the
   * identity UI was updated, or null if the connection is not secure.
   */
  _sslStatus: null,

  /**
   * Bitmask provided by nsIWebProgressListener.onSecurityChange.
   */
  _state: 0,

  get _isBroken() {
    return this._state & Ci.nsIWebProgressListener.STATE_IS_BROKEN;
  },

  get _isSecure() {
    // If a <browser> is included within a chrome document, then this._state
    // will refer to the security state for the <browser> and not the top level
    // document. In this case, don't upgrade the security state in the UI
    // with the secure state of the embedded <browser>.
    return !this._isURILoadedFromFile && this._state & Ci.nsIWebProgressListener.STATE_IS_SECURE;
  },

  get _isEV() {
    // If a <browser> is included within a chrome document, then this._state
    // will refer to the security state for the <browser> and not the top level
    // document. In this case, don't upgrade the security state in the UI
    // with the EV state of the embedded <browser>.
    return !this._isURILoadedFromFile && this._state & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL;
  },

  get _isMixedActiveContentLoaded() {
    return this._state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_ACTIVE_CONTENT;
  },

  get _isMixedActiveContentBlocked() {
    return this._state & Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT;
  },

  get _isMixedPassiveContentLoaded() {
    return this._state & Ci.nsIWebProgressListener.STATE_LOADED_MIXED_DISPLAY_CONTENT;
  },

  get _isCertUserOverridden() {
    return this._state & Ci.nsIWebProgressListener.STATE_CERT_USER_OVERRIDDEN;
  },

  get _hasInsecureLoginForms() {
    // checks if the page has been flagged for an insecure login. Also checks
    // if the pref to degrade the UI is set to true
    return LoginManagerParent.hasInsecureLoginForms(gBrowser.selectedBrowser) &&
           Services.prefs.getBoolPref("security.insecure_password.ui.enabled");
  },

  // smart getters
  get _identityPopup () {
    delete this._identityPopup;
    return this._identityPopup = document.getElementById("identity-popup");
  },
  get _identityBox () {
    delete this._identityBox;
    return this._identityBox = document.getElementById("identity-box");
  },
  get _identityPopupContentHosts () {
    delete this._identityPopupContentHosts;
    return this._identityPopupContentHosts = [...document.querySelectorAll(".identity-popup-headline.host")];
  },
  get _identityPopupContentHostless () {
    delete this._identityPopupContentHostless;
    return this._identityPopupContentHostless = [...document.querySelectorAll(".identity-popup-headline.hostless")];
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
  get _identityPopupMixedContentLearnMore () {
    delete this._identityPopupMixedContentLearnMore;
    return this._identityPopupMixedContentLearnMore =
      document.getElementById("identity-popup-mcb-learn-more");
  },
  get _identityPopupInsecureLoginFormsLearnMore () {
    delete this._identityPopupInsecureLoginFormsLearnMore;
    return this._identityPopupInsecureLoginFormsLearnMore =
      document.getElementById("identity-popup-insecure-login-forms-learn-more");
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
    return this._identityIcon = document.getElementById("identity-icon");
  },
  get _permissionList () {
    delete this._permissionList;
    return this._permissionList = document.getElementById("identity-popup-permission-list");
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

  toggleSubView(name, anchor) {
    let view = document.getElementById("identity-popup-multiView");
    if (view.showingSubView) {
      view.showMainView();
    } else {
      view.showSubView(`identity-popup-${name}View`, anchor);
    }

    // If an element is focused that's not the anchor, clear the focus.
    // Elements of hidden views have -moz-user-focus:ignore but setting that
    // per CSS selector doesn't blur a focused element in those hidden views.
    if (Services.focus.focusedElement != anchor) {
      Services.focus.clearFocus(window);
    }
  },

  disableMixedContentProtection() {
    // Use telemetry to measure how often unblocking happens
    const kMIXED_CONTENT_UNBLOCK_EVENT = 2;
    let histogram =
      Services.telemetry.getHistogramById(
        "MIXED_CONTENT_UNBLOCK_COUNTER");
    histogram.add(kMIXED_CONTENT_UNBLOCK_EVENT);
    // Reload the page with the content unblocked
    BrowserReloadWithFlags(
      Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_MIXED_CONTENT);
    this._identityPopup.hidePopup();
  },

  enableMixedContentProtection() {
    gBrowser.selectedBrowser.messageManager.sendAsyncMessage(
      "MixedContent:ReenableProtection", {});
    BrowserReload();
    this._identityPopup.hidePopup();
  },

  removeCertException() {
    if (!this._uriHasHost) {
      Cu.reportError("Trying to revoke a cert exception on a URI without a host?");
      return;
    }
    let host = this._uri.host;
    let port = this._uri.port > 0 ? this._uri.port : 443;
    this._overrideService.clearValidityOverride(host, port);
    BrowserReloadSkipCache();
    this._identityPopup.hidePopup();
  },

  /**
   * Helper to parse out the important parts of _sslStatus (of the SSL cert in
   * particular) for use in constructing identity UI strings
  */
  getIdentityData : function() {
    var result = {};
    var cert = this._sslStatus.serverCert;

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
   * Update the identity user interface for the page currently being displayed.
   *
   * This examines the SSL certificate metadata, if available, as well as the
   * connection type and other security-related state information for the page.
   *
   * @param state
   *        Bitmask provided by nsIWebProgressListener.onSecurityChange.
   * @param uri
   *        nsIURI for which the identity UI should be displayed, already
   *        processed by nsIURIFixup.createExposableURI.
   */
  updateIdentity(state, uri) {
    let shouldHidePopup = this._uri && (this._uri.spec != uri.spec);
    this._state = state;

    // Firstly, populate the state properties required to display the UI. See
    // the documentation of the individual properties for details.
    this.setURI(uri);
    this._sslStatus = gBrowser.securityUI
                              .QueryInterface(Ci.nsISSLStatusProvider)
                              .SSLStatus;
    if (this._sslStatus) {
      this._sslStatus.QueryInterface(Ci.nsISSLStatus);
    }

    // Then, update the user interface with the available data.
    this.refreshIdentityBlock();
    // Handle a location change while the Control Center is focused
    // by closing the popup (bug 1207542)
    if (shouldHidePopup) {
      this._identityPopup.hidePopup();
    }
    this.showWeakCryptoInfoBar();

    // NOTE: We do NOT update the identity popup (the control center) when
    // we receive a new security state on the existing page (i.e. from a
    // subframe). If the user opened the popup and looks at the provided
    // information we don't want to suddenly change the panel contents.
  },

  /**
   * This is called asynchronously when requested by the Logins module, after
   * the insecure login forms state for the page has been updated.
   */
  refreshForInsecureLoginForms() {
    // Check this._uri because we don't want to refresh the user interface if
    // this is called before the first page load in the window for any reason.
    if (!this._uri) {
      Cu.reportError("Unexpected early call to refreshForInsecureLoginForms.");
      return;
    }
    this.refreshIdentityBlock();
  },

  /**
   * Attempt to provide proper IDN treatment for host names
   */
  getEffectiveHost: function() {
    if (!this._IDNService)
      this._IDNService = Cc["@mozilla.org/network/idn-service;1"]
                         .getService(Ci.nsIIDNService);
    try {
      return this._IDNService.convertToDisplayIDN(this._uri.host, {});
    } catch (e) {
      // If something goes wrong (e.g. host is an IP address) just fail back
      // to the full domain.
      return this._uri.host;
    }
  },

  /**
   * Return the CSS class name to set on the "fullscreen-warning" element to
   * display information about connection security in the notification shown
   * when a site enters the fullscreen mode.
   */
  get fullscreenWarningClassName() {
    // Note that the fullscreen warning does not handle _isSecureInternalUI.
    if (this._uriHasHost && this._isEV) {
      return "verifiedIdentity";
    }
    if (this._uriHasHost && this._isSecure) {
      return "verifiedDomain";
    }
    return "unknownIdentity";
  },

  /**
   * Updates the identity block user interface with the data from this object.
   */
  refreshIdentityBlock() {
    if (!this._identityBox) {
      return;
    }

    let icon_label = "";
    let tooltip = "";
    let icon_country_label = "";
    let icon_labels_dir = "ltr";

    if (this._isSecureInternalUI) {
      this._identityBox.className = "chromeUI";
      let brandBundle = document.getElementById("bundle_brand");
      icon_label = brandBundle.getString("brandShorterName");
    } else if (this._uriHasHost && this._isEV) {
      this._identityBox.className = "verifiedIdentity";
      if (this._isMixedActiveContentBlocked) {
        this._identityBox.classList.add("mixedActiveBlocked");
      }

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

    } else if (this._uriHasHost && this._isSecure) {
      this._identityBox.className = "verifiedDomain";
      if (this._isMixedActiveContentBlocked) {
        this._identityBox.classList.add("mixedActiveBlocked");
      }
      if (this._isCertUserOverridden) {
        this._identityBox.classList.add("certUserOverridden");
        // Cert is trusted because of a security exception, verifier is a special string.
        tooltip = gNavigatorBundle.getString("identity.identified.verified_by_you");
      } else {
        // It's a normal cert, verifier is the CA Org.
        tooltip = gNavigatorBundle.getFormattedString("identity.identified.verifier",
                                                      [this.getIdentityData().caOrg]);
      }
    } else {
      this._identityBox.className = "unknownIdentity";
      if (this._isBroken) {
        if (this._isMixedActiveContentLoaded) {
          this._identityBox.classList.add("mixedActiveContent");
        } else if (this._isMixedActiveContentBlocked) {
          this._identityBox.classList.add("mixedDisplayContentLoadedActiveBlocked");
        } else if (this._isMixedPassiveContentLoaded) {
          this._identityBox.classList.add("mixedDisplayContent");
        } else {
          this._identityBox.classList.add("weakCipher");
        }
      }
      if (this._hasInsecureLoginForms) {
        // Insecure login forms can only be present on "unknown identity"
        // pages, either already insecure or with mixed active content loaded.
        this._identityBox.classList.add("insecureLoginForms");
      }
      tooltip = gNavigatorBundle.getString("identity.unknown.tooltip");
    }

    // Push the appropriate strings out to the UI
    this._identityBox.tooltipText = tooltip;
    this._identityIcon.tooltipText = gNavigatorBundle.getString("identity.icon.tooltip");
    this._identityIconLabel.value = icon_label;
    this._identityIconCountryLabel.value = icon_country_label;
    // Set cropping and direction
    this._identityIconLabel.crop = icon_country_label ? "end" : "center";
    this._identityIconLabel.parentNode.style.direction = icon_labels_dir;
    // Hide completely if the organization label is empty
    this._identityIconLabel.parentNode.collapsed = icon_label ? false : true;
  },

  /**
   * Show the weak crypto notification bar.
   */
  showWeakCryptoInfoBar() {
    if (!this._uriHasHost || !this._isBroken || !this._sslStatus.cipherName ||
        this._sslStatus.cipherName.indexOf("_RC4_") < 0) {
      return;
    }

    let notificationBox = gBrowser.getNotificationBox();
    let notification = notificationBox.getNotificationWithValue("weak-crypto");
    if (notification) {
      return;
    }

    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");
    let message = gNavigatorBundle.getFormattedString("weakCryptoOverriding.message",
                                                      [brandShortName]);

    let host = this._uri.host;
    let port = 443;
    try {
      if (this._uri.port > 0) {
        port = this._uri.port;
      }
    } catch (e) {}

    let buttons = [{
      label: gNavigatorBundle.getString("revokeOverride.label"),
      accessKey: gNavigatorBundle.getString("revokeOverride.accesskey"),
      callback: function (aNotification, aButton) {
        try {
          let weakCryptoOverride = Cc["@mozilla.org/security/weakcryptooverride;1"]
                                     .getService(Ci.nsIWeakCryptoOverride);
          weakCryptoOverride.removeWeakCryptoOverride(host, port,
            PrivateBrowsingUtils.isBrowserPrivate(gBrowser.selectedBrowser));
          BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }];

    const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
    notificationBox.appendNotification(message, "weak-crypto", null,
                                       priority, buttons);
  },

  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   */
  refreshIdentityPopup() {
    // Update "Learn More" for Mixed Content Blocking and Insecure Login Forms.
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    this._identityPopupMixedContentLearnMore
        .setAttribute("href", baseURL + "mixed-content");
    this._identityPopupInsecureLoginFormsLearnMore
        .setAttribute("href", baseURL + "insecure-password");

    // Determine connection security information.
    let connection = "not-secure";
    if (this._isSecureInternalUI) {
      connection = "chrome";
    } else if (this._isURILoadedFromFile) {
      connection = "file";
    } else if (this._isEV) {
      connection = "secure-ev";
    } else if (this._isCertUserOverridden) {
      connection = "secure-cert-user-overridden";
    } else if (this._isSecure) {
      connection = "secure";
    }

    // Determine if there are insecure login forms.
    let loginforms = "secure";
    if (this._hasInsecureLoginForms) {
      loginforms = "insecure";
    }

    // Determine the mixed content state.
    let mixedcontent = [];
    if (this._isMixedPassiveContentLoaded) {
      mixedcontent.push("passive-loaded");
    }
    if (this._isMixedActiveContentLoaded) {
      mixedcontent.push("active-loaded");
    } else if (this._isMixedActiveContentBlocked) {
      mixedcontent.push("active-blocked");
    }
    mixedcontent = mixedcontent.join(" ");

    // We have no specific flags for weak ciphers (yet). If a connection is
    // broken and we can't detect any mixed content loaded then it's a weak
    // cipher.
    let ciphers = "";
    if (this._isBroken && !this._isMixedActiveContentLoaded && !this._isMixedPassiveContentLoaded) {
      ciphers = "weak";
    }

    // Update all elements.
    let elementIDs = [
      "identity-popup",
      "identity-popup-securityView-body",
    ];

    function updateAttribute(elem, attr, value) {
      if (value) {
        elem.setAttribute(attr, value);
      } else {
        elem.removeAttribute(attr);
      }
    }

    for (let id of elementIDs) {
      let element = document.getElementById(id);
      updateAttribute(element, "connection", connection);
      updateAttribute(element, "loginforms", loginforms);
      updateAttribute(element, "ciphers", ciphers);
      updateAttribute(element, "mixedcontent", mixedcontent);
      updateAttribute(element, "isbroken", this._isBroken);
    }

    // Initialize the optional strings to empty values
    let supplemental = "";
    let verifier = "";
    let host = "";
    let owner = "";
    let hostless = false;

    try {
      host = this.getEffectiveHost();
    } catch (e) {
      // Some URIs might have no hosts.
    }

    // Fallback for special protocols.
    if (!host) {
      host = this._uri.specIgnoringRef;
      // Special URIs without a host (eg, about:) should crop the end so
      // the protocol can be seen.
      hostless = true;
    }

    // Fill in the CA name if we have a valid TLS certificate.
    if (this._isSecure) {
      verifier = this._identityBox.tooltipText;
    }

    // Fill in organization information if we have a valid EV certificate.
    if (this._isEV) {
      let iData = this.getIdentityData();
      host = owner = iData.subjectOrg;
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
    }

    // Push the appropriate strings out to the UI.
    this._identityPopupContentHosts.forEach((el) => {
      el.textContent = host;
      el.hidden = hostless;
    });
    this._identityPopupContentHostless.forEach((el) => {
      el.setAttribute("value", host);
      el.hidden = !hostless;
    });
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;

    // Update per-site permissions section.
    this.updateSitePermissions();
  },

  setURI(uri) {
    this._uri = uri;

    try {
      this._uri.host;
      this._uriHasHost = true;
    } catch (ex) {
      this._uriHasHost = false;
    }

    let whitelist = /^(?:accounts|addons|cache|config|crashes|customizing|downloads|healthreport|home|license|newaddon|permissions|preferences|privatebrowsing|rights|sessionrestore|support|welcomeback)(?:[?#]|$)/i;
    this._isSecureInternalUI = uri.schemeIs("about") && whitelist.test(uri.path);

    // Create a channel for the sole purpose of getting the resolved URI
    // of the request to determine if it's loaded from the file system.
    this._isURILoadedFromFile = false;
    let chanOptions = {uri: this._uri, loadUsingSystemPrincipal: true};
    let resolvedURI;
    try {
      resolvedURI = NetUtil.newChannel(chanOptions).URI;
      if (resolvedURI.schemeIs("jar")) {
        // Given a URI "jar:<jar-file-uri>!/<jar-entry>"
        // create a new URI using <jar-file-uri>!/<jar-entry>
        resolvedURI = NetUtil.newURI(resolvedURI.path);
      }
      // Check the URI again after resolving.
      this._isURILoadedFromFile = resolvedURI.schemeIs("file");
    } catch (ex) {
      // NetUtil's methods will throw for malformed URIs and the like
    }
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
    this.refreshIdentityPopup();

    // Add the "open" attribute to the identity box for styling
    this._identityBox.setAttribute("open", "true");

    // Now open the popup, anchored off the primary chrome element
    this._identityPopup.openPopup(this._identityIcon, "bottomcenter topleft");
  },

  onPopupShown(event) {
    if (event.target == this._identityPopup) {
      window.addEventListener("focus", this, true);
    }
  },

  onPopupHidden(event) {
    if (event.target == this._identityPopup) {
      window.removeEventListener("focus", this, true);
      this._identityBox.removeAttribute("open");
    }
  },

  handleEvent(event) {
    let elem = document.activeElement;
    let position = elem.compareDocumentPosition(this._identityPopup);

    if (!(position & (Node.DOCUMENT_POSITION_CONTAINS |
                      Node.DOCUMENT_POSITION_CONTAINED_BY)) &&
        !this._identityPopup.hasAttribute("noautohide")) {
      // Hide the panel when focusing an element that is
      // neither an ancestor nor descendant unless the panel has
      // @noautohide (e.g. for a tour).
      this._identityPopup.hidePopup();
    }
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
    dt.setDragImage(this._identityIcon, 16, 16);
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
    label.setAttribute("class", "identity-popup-permission-label");
    label.setAttribute("control", menulist.getAttribute("id"));
    label.textContent = SitePermissions.getPermissionLabel(aPermission);

    let container = document.createElement("hbox");
    container.setAttribute("align", "center");
    container.appendChild(label);
    container.appendChild(menulist);

    // The menuitem text can be long and we don't want the dropdown
    // to expand to the width of unselected labels.
    // Need to set this attribute after it's appended, otherwise it gets
    // overridden with sizetopopup="pref".
    menulist.setAttribute("sizetopopup", "none");

    return container;
  }
};

function getNotificationBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getNotificationBox(foundBrowser)
  return null;
}

function getTabModalPromptBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getTabModalPromptBox(foundBrowser);
  return null;
}

/* DEPRECATED */
function getBrowser() {
  return gBrowser;
}
function getNavToolbox() {
  return gNavToolbox;
}

var gPrivateBrowsingUI = {
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

    let urlBarSearchParam = gURLBar.getAttribute("autocompletesearchparam") || "";
    if (!PrivateBrowsingUtils.permanentPrivateBrowsing &&
        !urlBarSearchParam.includes("disable-private-actions")) {
      // Disable switch to tab autocompletion for private windows.
      // We leave it enabled for permanent private browsing mode though.
      urlBarSearchParam += " disable-private-actions";
    }
    if (!urlBarSearchParam.includes("private-window")) {
      urlBarSearchParam += " private-window";
    }
    gURLBar.setAttribute("autocompletesearchparam", urlBarSearchParam);
  }
};

var gRemoteTabsUI = {
  init: function() {
    if (window.location.href != getBrowserURL() &&
        // Also check hidden window for the Mac no-window case
        window.location.href != "chrome://browser/content/hiddenWindow.xul") {
      return;
    }

    if (AppConstants.platform == "macosx" &&
        Services.prefs.getBoolPref("layers.acceleration.disabled")) {
      // On OS X, "Disable Hardware Acceleration" also disables OMTC and forces
      // a fallback to Basic Layers. This is incompatible with e10s.
      return;
    }

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

var RestoreLastSessionObserver = {
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
  _updateToggleMuteMenuItem(aTab, aConditionFn) {
    ["muted", "soundplaying"].forEach(attr => {
      if (!aConditionFn || aConditionFn(attr)) {
        if (aTab.hasAttribute(attr)) {
          aTab.toggleMuteMenuItem.setAttribute(attr, "true");
        } else {
          aTab.toggleMuteMenuItem.removeAttribute(attr);
        }
      }
    });
  },
  updateContextMenu: function updateContextMenu(aPopupMenu) {
    this.contextTab = aPopupMenu.triggerNode.localName == "tab" ?
                      aPopupMenu.triggerNode : gBrowser.selectedTab;
    let disabled = gBrowser.tabs.length == 1;

    var menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple");
    for (let menuItem of menuItems)
      menuItem.disabled = disabled;

    if (AppConstants.E10S_TESTING_ONLY) {
      menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-remote");
      for (let menuItem of menuItems)
        menuItem.hidden = !gMultiProcessBrowser;
    }

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

    // Adjust the state of the toggle mute menu item.
    let toggleMute = document.getElementById("context_toggleMuteTab");
    if (this.contextTab.hasAttribute("muted")) {
      toggleMute.label = gNavigatorBundle.getString("unmuteTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("unmuteTab.accesskey");
    } else {
      toggleMute.label = gNavigatorBundle.getString("muteTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("muteTab.accesskey");
    }

    this.contextTab.toggleMuteMenuItem = toggleMute;
    this._updateToggleMuteMenuItem(this.contextTab);

    this.contextTab.addEventListener("TabAttrModified", this, false);
    aPopupMenu.addEventListener("popuphiding", this, false);
  },
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "popuphiding":
        gBrowser.removeEventListener("TabAttrModified", this);
        aEvent.target.removeEventListener("popuphiding", this);
        break;
      case "TabAttrModified":
        let tab = aEvent.target;
        this._updateToggleMuteMenuItem(tab,
          attr => aEvent.detail.changed.indexOf(attr) >= 0);
        break;
    }
  }
};

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource://devtools/client/framework/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevToolsBrowser",
                                  "resource://devtools/client/framework/gDevTools.jsm");

Object.defineProperty(this, "HUDService", {
  get: function HUDService_getter() {
    let devtools = Cu.import("resource://devtools/shared/Loader.jsm", {}).devtools;
    return devtools.require("devtools/client/webconsole/hudservice");
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
  switch (where) {
    case "window":
      let otherWin = OpenBrowserWindow();
      let delayedStartupFinished = (subject, topic) => {
        if (topic == "browser-delayed-startup-finished" &&
            subject == otherWin) {
          Services.obs.removeObserver(delayedStartupFinished, topic);
          let otherGBrowser = otherWin.gBrowser;
          let otherTab = otherGBrowser.selectedTab;
          SessionStore.duplicateTab(otherWin, aTab, delta);
          otherGBrowser.removeTab(otherTab, { animate: false });
        }
      };

      Services.obs.addObserver(delayedStartupFinished,
                               "browser-delayed-startup-finished",
                               false);
      break;
    case "tabshifted":
      SessionStore.duplicateTab(window, aTab, delta);
      // A background tab has been opened, nothing else to do here.
      break;
    case "tab":
      let newTab = SessionStore.duplicateTab(window, aTab, delta);
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
  Cu.import("resource://devtools/client/scratchpad/scratchpad-manager.jsm", tmp);
  return tmp.ScratchpadManager;
});

var ResponsiveUI = {
  toggle: function RUI_toggle() {
    this.ResponsiveUIManager.toggle(window, gBrowser.selectedTab);
  }
};

XPCOMUtils.defineLazyGetter(ResponsiveUI, "ResponsiveUIManager", function() {
  let tmp = {};
  Cu.import("resource://devtools/client/responsivedesign/responsivedesign.jsm", tmp);
  return tmp.ResponsiveUIManager;
});

function openEyedropper() {
  var eyedropper = new this.Eyedropper(this, { context: "menu",
                                               copyOnSelect: true });
  eyedropper.open();
}

Object.defineProperty(this, "Eyedropper", {
  get: function() {
    let devtools = Cu.import("resource://devtools/shared/Loader.jsm", {}).devtools;
    return devtools.require("devtools/client/eyedropper/eyedropper").Eyedropper;
  },
  configurable: true,
  enumerable: true
});

XPCOMUtils.defineLazyGetter(window, "gShowPageResizers", function () {
  // Only show resizers on Windows 2000 and XP
  return AppConstants.isPlatformAndVersionAtMost("win", "5.9");
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

function BrowserOpenNewTabOrWindow(event) {
  if (event.shiftKey) {
    OpenBrowserWindow();
  } else {
    BrowserOpenTab();
  }
}

var ToolbarIconColor = {
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
    if (AppConstants.platform == "macosx")
      toolbarSelector += ":not([type=menubar])";

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

var PanicButtonNotifier = {
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

var AboutPrivateBrowsingListener = {
  init: function () {
    window.messageManager.addMessageListener(
      "AboutPrivateBrowsing:OpenPrivateWindow",
      msg => {
        OpenBrowserWindow({private: true});
    });
    window.messageManager.addMessageListener(
      "AboutPrivateBrowsing:ToggleTrackingProtection",
      msg => {
        const PREF = "privacy.trackingprotection.pbmode.enabled";
        Services.prefs.setBoolPref(PREF, !Services.prefs.getBoolPref(PREF));
    });
    window.messageManager.addMessageListener(
      "AboutPrivateBrowsing:DontShowIntroPanelAgain",
      msg => {
        TrackingProtection.dontShowIntroPanelAgain();
    });
  }
};

function TabModalPromptBox(browser) {
  this._weakBrowserRef = Cu.getWeakReference(browser);
}

TabModalPromptBox.prototype = {
  _promptCloseCallback(onCloseCallback, principalToAllowFocusFor, allowFocusCheckbox, ...args) {
    if (principalToAllowFocusFor && allowFocusCheckbox &&
        allowFocusCheckbox.checked) {
      Services.perms.addFromPrincipal(principalToAllowFocusFor, "focus-tab-by-prompt",
                                      Services.perms.ALLOW_ACTION);
    }
    onCloseCallback.apply(this, args);
  },

  appendPrompt(args, onCloseCallback) {
    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let newPrompt = document.createElementNS(XUL_NS, "tabmodalprompt");
    let browser = this.browser;
    browser.parentNode.appendChild(newPrompt);
    browser.setAttribute("tabmodalPromptShowing", true);

    newPrompt.clientTop; // style flush to assure binding is attached

    let principalToAllowFocusFor = this._allowTabFocusByPromptPrincipal;
    delete this._allowTabFocusByPromptPrincipal;

    let allowFocusCheckbox; // Define outside the if block so we can bind it into the callback.
    let hostForAllowFocusCheckbox = "";
    try {
      hostForAllowFocusCheckbox = principalToAllowFocusFor.URI.host;
    } catch (ex) { /* Ignore exceptions for host-less URIs */ }
    if (hostForAllowFocusCheckbox) {
      let allowFocusRow = document.createElementNS(XUL_NS, "row");
      allowFocusCheckbox = document.createElementNS(XUL_NS, "checkbox");
      let spacer = document.createElementNS(XUL_NS, "spacer");
      allowFocusRow.appendChild(spacer);
      let label = gBrowser.mStringBundle.getFormattedString("tabs.allowTabFocusByPromptForSite",
                                                            [hostForAllowFocusCheckbox]);
      allowFocusCheckbox.setAttribute("label", label);
      allowFocusRow.appendChild(allowFocusCheckbox);
      newPrompt.appendChild(allowFocusRow);
    }

    let tab = gBrowser.getTabForBrowser(browser);
    let closeCB = this._promptCloseCallback.bind(null, onCloseCallback, principalToAllowFocusFor,
                                                 allowFocusCheckbox);
    newPrompt.init(args, tab, closeCB);
    return newPrompt;
  },

  removePrompt(aPrompt) {
    let browser = this.browser;
    browser.parentNode.removeChild(aPrompt);

    let prompts = this.listPrompts();
    if (prompts.length) {
      let prompt = prompts[prompts.length - 1];
      prompt.Dialog.setDefaultFocus();
    } else {
      browser.removeAttribute("tabmodalPromptShowing");
      browser.focus();
    }
  },

  listPrompts(aPrompt) {
    // Get the nodelist, then return as an array
    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let els = this.browser.parentNode.getElementsByTagNameNS(XUL_NS, "tabmodalprompt");
    return Array.from(els);
  },

  onNextPromptShowAllowFocusCheckboxFor(principal) {
    this._allowTabFocusByPromptPrincipal = principal;
  },

  get browser() {
    let browser = this._weakBrowserRef.get();
    if (!browser) {
      throw "Stale promptbox! The associated browser is gone.";
    }
    return browser;
  },
};

let UserContextStyleManager = {
  init() {
    for (let styleId in document.styleSheets) {
      let styleSheet = document.styleSheets[styleId];
      if (styleSheet.href != "chrome://browser/content/usercontext/usercontext.css") {
        continue;
      }

      if (ContextualIdentityService.needsCssRule()) {
        for (let ruleId in styleSheet.cssRules) {
          let cssRule = styleSheet.cssRules[ruleId];
          if (cssRule.selectorText != ":root") {
            continue;
          }

          ContextualIdentityService.storeCssRule(cssRule.cssText);
          break;
        }
      }

      ContextualIdentityService.cssRules().forEach(rule => {
        styleSheet.insertRule(rule, styleSheet.cssRules.length);
      });

      break;
    }
  },
};
