/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */
/* globals StatusPanel */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/NotificationDB.jsm");

const {WebExtensionPolicy} = Cu.getGlobalForObject(Services);

// lazy module getters

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutHome: "resource:///modules/AboutHome.jsm",
  BrowserUITelemetry: "resource:///modules/BrowserUITelemetry.jsm",
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  CharsetMenu: "resource://gre/modules/CharsetMenu.jsm",
  Color: "resource://gre/modules/Color.jsm",
  ContentSearch: "resource:///modules/ContentSearch.jsm",
  ContextualIdentityService: "resource://gre/modules/ContextualIdentityService.jsm",
  CustomizableUI: "resource:///modules/CustomizableUI.jsm",
  Deprecated: "resource://gre/modules/Deprecated.jsm",
  DownloadsCommon: "resource:///modules/DownloadsCommon.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ExtensionsUI: "resource:///modules/ExtensionsUI.jsm",
  FormValidationHandler: "resource:///modules/FormValidationHandler.jsm",
  LanguagePrompt: "resource://gre/modules/LanguagePrompt.jsm",
  LightweightThemeConsumer: "resource://gre/modules/LightweightThemeConsumer.jsm",
  LightweightThemeManager: "resource://gre/modules/LightweightThemeManager.jsm",
  Log: "resource://gre/modules/Log.jsm",
  LoginManagerParent: "resource://gre/modules/LoginManagerParent.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  OpenInTabsUtils: "resource:///modules/OpenInTabsUtils.jsm",
  PageActions: "resource:///modules/PageActions.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PanelMultiView: "resource:///modules/PanelMultiView.jsm",
  PanelView: "resource:///modules/PanelMultiView.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ProcessHangMonitor: "resource:///modules/ProcessHangMonitor.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  ReaderMode: "resource://gre/modules/ReaderMode.jsm",
  ReaderParent: "resource:///modules/ReaderParent.jsm",
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  SchedulePressure: "resource:///modules/SchedulePressure.jsm",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.jsm",
  SimpleServiceDiscovery: "resource://gre/modules/SimpleServiceDiscovery.jsm",
  SitePermissions: "resource:///modules/SitePermissions.jsm",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  TelemetryStopwatch: "resource://gre/modules/TelemetryStopwatch.jsm",
  Translation: "resource:///modules/translation/Translation.jsm",
  UITour: "resource:///modules/UITour.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
  Utils: "resource://gre/modules/sessionstore/Utils.jsm",
  Weave: "resource://services-sync/main.js",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  fxAccounts: "resource://gre/modules/FxAccounts.jsm",
  webrtcUI: "resource:///modules/webrtcUI.jsm",
  ZoomUI: "resource:///modules/ZoomUI.jsm",
});

if (AppConstants.MOZ_CRASHREPORTER) {
  ChromeUtils.defineModuleGetter(this, "PluginCrashReporter",
    "resource:///modules/ContentCrashHandlers.jsm");
}

XPCOMUtils.defineLazyScriptGetter(this, "PrintUtils",
                                  "chrome://global/content/printUtils.js");
XPCOMUtils.defineLazyScriptGetter(this, "ZoomManager",
                                  "chrome://global/content/viewZoomOverlay.js");
XPCOMUtils.defineLazyScriptGetter(this, "FullZoom",
                                  "chrome://browser/content/browser-fullZoom.js");
XPCOMUtils.defineLazyScriptGetter(this, "PanelUI",
                                  "chrome://browser/content/customizableui/panelUI.js");
XPCOMUtils.defineLazyScriptGetter(this, "gViewSourceUtils",
                                  "chrome://global/content/viewSourceUtils.js");
XPCOMUtils.defineLazyScriptGetter(this, ["LightWeightThemeWebInstaller",
                                         "gExtensionsNotifications",
                                         "gXPInstallObserver"],
                                  "chrome://browser/content/browser-addons.js");
XPCOMUtils.defineLazyScriptGetter(this, "ctrlTab",
                                  "chrome://browser/content/browser-ctrlTab.js");
XPCOMUtils.defineLazyScriptGetter(this, "CustomizationHandler",
                                  "chrome://browser/content/browser-customization.js");
XPCOMUtils.defineLazyScriptGetter(this, ["PointerLock", "FullScreen"],
                                  "chrome://browser/content/browser-fullScreenAndPointerLock.js");
XPCOMUtils.defineLazyScriptGetter(this, ["gGestureSupport", "gHistorySwipeAnimation"],
                                  "chrome://browser/content/browser-gestureSupport.js");
XPCOMUtils.defineLazyScriptGetter(this, "gSafeBrowsing",
                                  "chrome://browser/content/browser-safebrowsing.js");
XPCOMUtils.defineLazyScriptGetter(this, "gSync",
                                  "chrome://browser/content/browser-sync.js");
XPCOMUtils.defineLazyScriptGetter(this, "gBrowserThumbnails",
                                  "chrome://browser/content/browser-thumbnails.js");
XPCOMUtils.defineLazyScriptGetter(this, ["setContextMenuContentData",
                                         "openContextMenu", "nsContextMenu"],
                                  "chrome://browser/content/nsContextMenu.js");
XPCOMUtils.defineLazyScriptGetter(this, ["DownloadsPanel",
                                         "DownloadsOverlayLoader",
                                         "DownloadsSubview",
                                         "DownloadsView", "DownloadsViewUI",
                                         "DownloadsViewController",
                                         "DownloadsSummary", "DownloadsFooter",
                                         "DownloadsBlockedSubview"],
                                  "chrome://browser/content/downloads/downloads.js");
XPCOMUtils.defineLazyScriptGetter(this, ["DownloadsButton",
                                         "DownloadsIndicatorView"],
                                  "chrome://browser/content/downloads/indicator.js");
XPCOMUtils.defineLazyScriptGetter(this, "gEditItemOverlay",
                                  "chrome://browser/content/places/editBookmark.js");
if (AppConstants.NIGHTLY_BUILD) {
  XPCOMUtils.defineLazyScriptGetter(this, "gWebRender",
                                    "chrome://browser/content/browser-webrender.js");
}

// lazy service getters

XPCOMUtils.defineLazyServiceGetters(this, {
  Favicons: ["@mozilla.org/browser/favicon-service;1", "mozIAsyncFavicons"],
  gAboutNewTabService: ["@mozilla.org/browser/aboutnewtab-service;1", "nsIAboutNewTabService"],
  gDNSService: ["@mozilla.org/network/dns-service;1", "nsIDNSService"],
  gSerializationHelper: ["@mozilla.org/network/serialization-helper;1", "nsISerializationHelper"],
  Marionette: ["@mozilla.org/remote/marionette;1", "nsIMarionette"],
  SessionStartup: ["@mozilla.org/browser/sessionstartup;1", "nsISessionStartup"],
  WindowsUIUtils: ["@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils"],
});

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyServiceGetter(this, "gCrashReporter",
                                     "@mozilla.org/xre/app-info;1",
                                     "nsICrashReporter");
}

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/browser.properties");
});
XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  // This is a stringbundle-like interface to gBrowserBundle, formerly a getter for
  // the "bundle_browser" element.
  return {
    getString(key) {
      return gBrowserBundle.GetStringFromName(key);
    },
    getFormattedString(key, array) {
      return gBrowserBundle.formatStringFromName(key, array, array.length);
    }
  };
});
XPCOMUtils.defineLazyGetter(this, "gTabBrowserBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/tabbrowser.properties");
});

XPCOMUtils.defineLazyGetter(this, "gCustomizeMode", function() {
  let scope = {};
  ChromeUtils.import("resource:///modules/CustomizeMode.jsm", scope);
  return new scope.CustomizeMode(window);
});

XPCOMUtils.defineLazyGetter(this, "InlineSpellCheckerUI", function() {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/InlineSpellChecker.jsm", tmp);
  return new tmp.InlineSpellChecker();
});

XPCOMUtils.defineLazyGetter(this, "PageMenuParent", function() {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/PageMenu.jsm", tmp);
  return new tmp.PageMenuParent();
});

XPCOMUtils.defineLazyGetter(this, "PopupNotifications", function() {
  let tmp = {};
  ChromeUtils.import("resource://gre/modules/PopupNotifications.jsm", tmp);
  try {
    // Hide all notifications while the URL is being edited and the address bar
    // has focus, including the virtual focus in the results popup.
    // We also have to hide notifications explicitly when the window is
    // minimized because of the effects of the "noautohide" attribute on Linux.
    // This can be removed once bug 545265 and bug 1320361 are fixed.
    let shouldSuppress = () => {
      return window.windowState == window.STATE_MINIMIZED ||
             (gURLBar.getAttribute("pageproxystate") != "valid" &&
             gURLBar.focused);
    };
    return new tmp.PopupNotifications(gBrowser,
                                      document.getElementById("notification-popup"),
                                      document.getElementById("notification-popup-box"),
                                      { shouldSuppress });
  } catch (ex) {
    Cu.reportError(ex);
    return null;
  }
});

XPCOMUtils.defineLazyGetter(this, "Win7Features", function() {
  if (AppConstants.platform != "win")
    return null;

  const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
  if (WINTASKBAR_CONTRACTID in Cc &&
      Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available) {
    let AeroPeek = ChromeUtils.import("resource:///modules/WindowsPreviewPerTab.jsm", {}).AeroPeek;
    return {
      onOpenWindow() {
        AeroPeek.onOpenWindow(window);
      },
      onCloseWindow() {
        AeroPeek.onCloseWindow(window);
      }
    };
  }
  return null;
});

var gBrowser;
var gLastValidURLStr = "";
var gInPrintPreviewMode = false;
var gContextMenu = null; // nsContextMenu instance
var gMultiProcessBrowser =
  window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsILoadContext)
        .useRemoteTabs;

if (AppConstants.platform != "macosx") {
  var gEditUIVisible = true;
}

/* globals gNavToolbox, gURLBar:true */
[
  ["gNavToolbox",         "navigator-toolbox"],
  ["gURLBar",             "urlbar"],
].forEach(function(elementGlobal) {
  var [name, id] = elementGlobal;
  Object.defineProperty(window, name, {
    configurable: true,
    enumerable: true,
    get() {
      var element = document.getElementById(id);
      if (!element)
        return null;
      delete window[name];
      return window[name] = element;
    },
    set(val) {
      delete window[name];
      return window[name] = val;
    },
  });
});

// Smart getter for the findbar.  If you don't wish to force the creation of
// the findbar, check gFindBarInitialized first.

Object.defineProperty(this, "gFindBar", {
  configurable: true,
  enumerable: true,
  get() {
    return gBrowser.getCachedFindBar();
  },
});

Object.defineProperty(this, "gFindBarInitialized", {
  configurable: true,
  enumerable: true,
  get() {
    return gBrowser.isFindBarInitialized();
  },
});

Object.defineProperty(this, "gFindBarPromise", {
  configurable: true,
  enumerable: true,
  get() {
    return gBrowser.getFindBar();
  },
});

async function gLazyFindCommand(cmd, ...args) {
  let fb = await gFindBarPromise;
  // We could be closed by now, or the tab with XBL binding could have gone away:
  if (fb && fb[cmd]) {
    fb[cmd].apply(fb, args);
  }
}

Object.defineProperty(this, "AddonManager", {
  configurable: true,
  enumerable: true,
  get() {
    let tmp = {};
    ChromeUtils.import("resource://gre/modules/AddonManager.jsm", tmp);
    return this.AddonManager = tmp.AddonManager;
  },
  set(val) {
    delete this.AddonManager;
    return this.AddonManager = val;
  },
});


var gInitialPages = [
  "about:blank",
  "about:newtab",
  "about:home",
  "about:privatebrowsing",
  "about:welcomeback",
  "about:sessionrestore"
];

function isInitialPage(url) {
  return gInitialPages.includes(url) || url == BROWSER_NEW_TAB_URL;
}

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
  // Bug 414797: Clone the back/forward buttons' context menu into both buttons.
  let popup = document.getElementById("backForwardMenu").cloneNode(true);
  popup.removeAttribute("id");
  // Prevent the back/forward buttons' context attributes from being inherited.
  popup.setAttribute("context", "");

  let backButton = document.getElementById("back-button");
  backButton.setAttribute("type", "menu");
  backButton.appendChild(popup);
  gClickAndHoldListenersOnElement.add(backButton);

  let forwardButton = document.getElementById("forward-button");
  popup = popup.cloneNode(true);
  forwardButton.setAttribute("type", "menu");
  forwardButton.appendChild(popup);
  gClickAndHoldListenersOnElement.add(forwardButton);
}


const gClickAndHoldListenersOnElement = {
  _timers: new Map(),

  _mousedownHandler(aEvent) {
    if (aEvent.button != 0 ||
        aEvent.currentTarget.open ||
        aEvent.currentTarget.disabled)
      return;

    // Prevent the menupopup from opening immediately
    aEvent.currentTarget.firstChild.hidden = true;

    aEvent.currentTarget.addEventListener("mouseout", this);
    aEvent.currentTarget.addEventListener("mouseup", this);
    this._timers.set(aEvent.currentTarget, setTimeout((b) => this._openMenu(b), 500, aEvent.currentTarget));
  },

  _clickHandler(aEvent) {
    if (aEvent.button == 0 &&
        aEvent.target == aEvent.currentTarget &&
        !aEvent.currentTarget.open &&
        !aEvent.currentTarget.disabled) {
      let cmdEvent = document.createEvent("xulcommandevent");
      cmdEvent.initCommandEvent("command", true, true, window, 0,
                                aEvent.ctrlKey, aEvent.altKey, aEvent.shiftKey,
                                aEvent.metaKey, null, aEvent.mozInputSource);
      aEvent.currentTarget.dispatchEvent(cmdEvent);

      // This is here to cancel the XUL default event
      // dom.click() triggers a command even if there is a click handler
      // however this can now be prevented with preventDefault().
      aEvent.preventDefault();
    }
  },

  _openMenu(aButton) {
    this._cancelHold(aButton);
    aButton.firstChild.hidden = false;
    aButton.open = true;
  },

  _mouseoutHandler(aEvent) {
    let buttonRect = aEvent.currentTarget.getBoundingClientRect();
    if (aEvent.clientX >= buttonRect.left &&
        aEvent.clientX <= buttonRect.right &&
        aEvent.clientY >= buttonRect.bottom)
      this._openMenu(aEvent.currentTarget);
    else
      this._cancelHold(aEvent.currentTarget);
  },

  _mouseupHandler(aEvent) {
    this._cancelHold(aEvent.currentTarget);
  },

  _cancelHold(aButton) {
    clearTimeout(this._timers.get(aButton));
    aButton.removeEventListener("mouseout", this);
    aButton.removeEventListener("mouseup", this);
  },

  handleEvent(e) {
    switch (e.type) {
      case "mouseout":
        this._mouseoutHandler(e);
        break;
      case "mousedown":
        this._mousedownHandler(e);
        break;
      case "click":
        this._clickHandler(e);
        break;
      case "mouseup":
        this._mouseupHandler(e);
        break;
    }
  },

  remove(aButton) {
    aButton.removeEventListener("mousedown", this, true);
    aButton.removeEventListener("click", this, true);
  },

  add(aElm) {
    this._timers.delete(aElm);

    aElm.addEventListener("mousedown", this, true);
    aElm.addEventListener("click", this, true);
  }
};

const gSessionHistoryObserver = {
  observe(subject, topic, data) {
    if (topic != "browser:purge-session-history")
      return;

    var backCommand = document.getElementById("Browser:Back");
    backCommand.setAttribute("disabled", "true");
    var fwdCommand = document.getElementById("Browser:Forward");
    fwdCommand.setAttribute("disabled", "true");

    // Hide session restore button on about:home
    window.messageManager.broadcastAsyncMessage("Browser:HideSessionRestoreButton");

    // Clear undo history of the URL bar
    gURLBar.editor.transactionManager.clear();
  }
};

const gStoragePressureObserver = {
  _lastNotificationTime: -1,

  observe(subject, topic, data) {
    if (topic != "QuotaManager::StoragePressure") {
      return;
    }

    const NOTIFICATION_VALUE = "storage-pressure-notification";
    let notificationBox = document.getElementById("high-priority-global-notificationbox");
    if (notificationBox.getNotificationWithValue(NOTIFICATION_VALUE)) {
      // Do not display the 2nd notification when there is already one
      return;
    }

    // Don't display notification twice within the given interval.
    // This is because
    //   - not to annoy user
    //   - give user some time to clean space.
    //     Even user sees notification and starts acting, it still takes some time.
    const MIN_NOTIFICATION_INTERVAL_MS =
      Services.prefs.getIntPref("browser.storageManager.pressureNotification.minIntervalMS");
    let duration = Date.now() - this._lastNotificationTime;
    if (duration <= MIN_NOTIFICATION_INTERVAL_MS) {
      return;
    }
    this._lastNotificationTime = Date.now();

    const BYTES_IN_GIGABYTE = 1073741824;
    const USAGE_THRESHOLD_BYTES = BYTES_IN_GIGABYTE *
      Services.prefs.getIntPref("browser.storageManager.pressureNotification.usageThresholdGB");
    let msg = "";
    let buttons = [];
    let usage = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    let prefStrBundle = document.getElementById("bundle_preferences");
    let brandShortName = document.getElementById("bundle_brand").getString("brandShortName");
    buttons.push({
      label: prefStrBundle.getString("spaceAlert.learnMoreButton.label"),
      accessKey: prefStrBundle.getString("spaceAlert.learnMoreButton.accesskey"),
      callback(notificationBar, button) {
        let learnMoreURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "storage-permissions";
        gBrowser.selectedTab = gBrowser.addTab(learnMoreURL);
      }
    });
    if (usage < USAGE_THRESHOLD_BYTES) {
      // The firefox-used space < 5GB, then warn user to free some disk space.
      // This is because this usage is small and not the main cause for space issue.
      // In order to avoid the bad and wrong impression among users that
      // firefox eats disk space a lot, indicate users to clean up other disk space.
      msg = prefStrBundle.getFormattedString("spaceAlert.under5GB.message", [brandShortName]);
      buttons.push({
        label: prefStrBundle.getString("spaceAlert.under5GB.okButton.label"),
        accessKey: prefStrBundle.getString("spaceAlert.under5GB.okButton.accesskey"),
        callback() {}
      });
    } else {
      // The firefox-used space >= 5GB, then guide users to about:preferences
      // to clear some data stored on firefox by websites.
      let descriptionStringID = "spaceAlert.over5GB.message1";
      let prefButtonLabelStringID = "spaceAlert.over5GB.prefButton.label";
      let prefButtonAccesskeyStringID = "spaceAlert.over5GB.prefButton.accesskey";
      if (AppConstants.platform == "win") {
        descriptionStringID = "spaceAlert.over5GB.messageWin1";
        prefButtonLabelStringID = "spaceAlert.over5GB.prefButtonWin.label";
        prefButtonAccesskeyStringID = "spaceAlert.over5GB.prefButtonWin.accesskey";
      }
      msg = prefStrBundle.getFormattedString(descriptionStringID, [brandShortName]);
      buttons.push({
        label: prefStrBundle.getString(prefButtonLabelStringID),
        accessKey: prefStrBundle.getString(prefButtonAccesskeyStringID),
        callback(notificationBar, button) {
          // The advanced subpanes are only supported in the old organization, which will
          // be removed by bug 1349689.
          let win = gBrowser.ownerGlobal;
          win.openPreferences("privacy-sitedata", { origin: "storagePressure" });
        }
      });
    }

    notificationBox.appendNotification(
      msg, NOTIFICATION_VALUE, null, notificationBox.PRIORITY_WARNING_HIGH, buttons, null);
  }
};

var gPopupBlockerObserver = {
  handleEvent(aEvent) {
    if (aEvent.originalTarget != gBrowser.selectedBrowser)
      return;

    gIdentityHandler.refreshIdentityBlock();

    if (!gBrowser.selectedBrowser.blockedPopups ||
        !gBrowser.selectedBrowser.blockedPopups.length) {
      // Hide the notification box (if it's visible).
      let notificationBox = gBrowser.getNotificationBox();
      let notification = notificationBox.getNotificationWithValue("popup-blocked");
      if (notification) {
        notificationBox.removeNotification(notification, false);
      }
      return;
    }

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!gBrowser.selectedBrowser.blockedPopups.reported) {
      if (Services.prefs.getBoolPref("privacy.popups.showBrowserMessage")) {
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
        } else {
          var buttons = [{
            label: popupButtonText,
            accessKey: popupButtonAccesskey,
            popup: "blockedPopupOptions",
            callback: null
          }];

          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          notificationBox.appendNotification(message, "popup-blocked",
                                             "chrome://browser/skin/notification-icons/popup.svg",
                                             priority, buttons);
        }
      }

      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      gBrowser.selectedBrowser.blockedPopups.reported = true;
    }
  },

  toggleAllowPopupsForSite(aEvent) {
    var pm = Services.perms;
    var shouldBlock = aEvent.target.getAttribute("block") == "true";
    var perm = shouldBlock ? pm.DENY_ACTION : pm.ALLOW_ACTION;
    pm.addFromPrincipal(gBrowser.contentPrincipal, "popup", perm);

    if (!shouldBlock)
      this.showAllBlockedPopups(gBrowser.selectedBrowser);

    gBrowser.getNotificationBox().removeCurrentNotification();
  },

  fillPopupList(aEvent) {
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
    var uri = browser.contentPrincipal.URI || browser.currentURI;
    var blockedPopupAllowSite = document.getElementById("blockedPopupAllowSite");
    try {
      blockedPopupAllowSite.removeAttribute("hidden");
      let uriHost = uri.asciiHost ? uri.host : uri.spec;
      var pm = Services.perms;
      if (pm.testPermission(uri, "popup") == pm.ALLOW_ACTION) {
        // Offer an item to block popups for this site, if a whitelist entry exists
        // already for it.
        let blockString = gNavigatorBundle.getFormattedString("popupBlock", [uriHost]);
        blockedPopupAllowSite.setAttribute("label", blockString);
        blockedPopupAllowSite.setAttribute("block", "true");
      } else {
        // Offer an item to allow popups for this site
        let allowString = gNavigatorBundle.getFormattedString("popupAllow", [uriHost]);
        blockedPopupAllowSite.setAttribute("label", allowString);
        blockedPopupAllowSite.removeAttribute("block");
      }
    } catch (e) {
      blockedPopupAllowSite.setAttribute("hidden", "true");
    }

    if (PrivateBrowsingUtils.isWindowPrivate(window))
      blockedPopupAllowSite.setAttribute("disabled", "true");
    else
      blockedPopupAllowSite.removeAttribute("disabled");

    let blockedPopupDontShowMessage = document.getElementById("blockedPopupDontShowMessage");
    let showMessage = Services.prefs.getBoolPref("privacy.popups.showBrowserMessage");
    blockedPopupDontShowMessage.setAttribute("checked", !showMessage);
    blockedPopupDontShowMessage.setAttribute("label", gNavigatorBundle.getString("popupWarningDontShowFromMessage"));

    let blockedPopupsSeparator =
        document.getElementById("blockedPopupsSeparator");
    blockedPopupsSeparator.setAttribute("hidden", true);

    gBrowser.selectedBrowser.retrieveListOfBlockedPopups().then(blockedPopups => {
      let foundUsablePopupURI = false;
      if (blockedPopups) {
        for (let i = 0; i < blockedPopups.length; i++) {
          let blockedPopup = blockedPopups[i];

          // popupWindowURI will be null if the file picker popup is blocked.
          // xxxdz this should make the option say "Show file picker" and do it (Bug 590306)
          if (!blockedPopup.popupWindowURIspec)
            continue;

          var popupURIspec = blockedPopup.popupWindowURIspec;

          // Sometimes the popup URI that we get back from the blockedPopup
          // isn't useful (for instance, netscape.com's popup URI ends up
          // being "http://www.netscape.com", which isn't really the URI of
          // the popup they're trying to show).  This isn't going to be
          // useful to the user, so we won't create a menu item for it.
          if (popupURIspec == "" || popupURIspec == "about:blank" ||
              popupURIspec == "<self>" ||
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

      // Show the separator if we added any
      // showable popup addresses to the menu.
      if (foundUsablePopupURI)
        blockedPopupsSeparator.removeAttribute("hidden");
    }, null);
  },

  onPopupHiding(aEvent) {
    let item = aEvent.target.lastChild;
    while (item && item.getAttribute("observes") != "blockedPopupsSeparator") {
      let next = item.previousSibling;
      item.remove();
      item = next;
    }
  },

  showBlockedPopup(aEvent) {
    var target = aEvent.target;
    var popupReportIndex = target.getAttribute("popupReportIndex");
    let browser = target.popupReportBrowser;
    browser.unblockPopup(popupReportIndex);
  },

  showAllBlockedPopups(aBrowser) {
    aBrowser.retrieveListOfBlockedPopups().then(popups => {
      for (let i = 0; i < popups.length; i++) {
        if (popups[i].popupWindowURIspec)
          aBrowser.unblockPopup(i);
      }
    }, null);
  },

  editPopupSettings() {
    let prefillValue = "";
    try {
      // We use contentPrincipal rather than currentURI to get the right
      // value in case this is a data: URI that's inherited off something else.
      // Some principals don't have URIs, so fall back in case URI is not present.
      let principalURI = gBrowser.contentPrincipal.URI || gBrowser.currentURI;
      if (principalURI) {
        // asciiHost conveniently doesn't throw.
        if (principalURI.asciiHost) {
          prefillValue = principalURI.prePath;
        } else {
          // For host-less URIs like file://, prePath would effectively allow
          // popups everywhere on file://. Use the full spec:
          prefillValue = principalURI.spec;
        }
      }
    } catch (e) { }

    var bundlePreferences = document.getElementById("bundle_preferences");
    var params = { blockVisible: false,
                   sessionVisible: false,
                   allowVisible: true,
                   prefilledHost: prefillValue,
                   permissionType: "popup",
                   windowTitle: bundlePreferences.getString("popuppermissionstitle2"),
                   introText: bundlePreferences.getString("popuppermissionstext") };
    var existingWindow = Services.wm.getMostRecentWindow("Browser:Permissions");
    if (existingWindow) {
      existingWindow.initWithParams(params);
      existingWindow.focus();
    } else
      window.openDialog("chrome://browser/content/preferences/permissions.xul",
                        "_blank", "resizable,dialog=no,centerscreen", params);
  },

  dontShowMessage() {
    var showMessage = Services.prefs.getBoolPref("privacy.popups.showBrowserMessage");
    Services.prefs.setBoolPref("privacy.popups.showBrowserMessage", !showMessage);
    gBrowser.getNotificationBox().removeCurrentNotification();
  }
};

function gKeywordURIFixup({ target: browser, data: fixupInfo }) {
  let deserializeURI = (spec) => spec ? makeURI(spec) : null;

  // We get called irrespective of whether we did a keyword search, or
  // whether the original input would be vaguely interpretable as a URL,
  // so figure that out first.
  let alternativeURI = deserializeURI(fixupInfo.fixedURI);
  if (!fixupInfo.keywordProviderName || !alternativeURI || !alternativeURI.host) {
    return;
  }

  let contentPrincipal = browser.contentPrincipal;

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
  let hostName = alternativeURI.displayHost;
  // and the ascii-only host for the pref:
  let asciiHost = alternativeURI.asciiHost;
  // Normalize out a single trailing dot - NB: not using endsWith/lastIndexOf
  // because we need to be sure this last dot is the *only* dot, too.
  // More generally, this is used for the pref and should stay in sync with
  // the code in nsDefaultURIFixup::KeywordURIFixup .
  if (asciiHost.indexOf(".") == asciiHost.length - 1) {
    asciiHost = asciiHost.slice(0, -1);
  }

  let isIPv4Address = host => {
    let parts = host.split(".");
    if (parts.length != 4) {
      return false;
    }
    return parts.every(part => {
      let n = parseInt(part, 10);
      return n >= 0 && n <= 255;
    });
  };
  // Avoid showing fixup information if we're suggesting an IP. Note that
  // decimal representations of IPs are normalized to a 'regular'
  // dot-separated IP address by network code, but that only happens for
  // numbers that don't overflow. Longer numbers do not get normalized,
  // but still work to access IP addresses. So for instance,
  // 1097347366913 (ff7f000001) gets resolved by using the final bytes,
  // making it the same as 7f000001, which is 127.0.0.1 aka localhost.
  // While 2130706433 would get normalized by network, 1097347366913
  // does not, and we have to deal with both cases here:
  if (isIPv4Address(asciiHost) || /^(?:\d+|0x[a-f0-9]+)$/i.test(asciiHost))
    return;

  let onLookupComplete = (request, record, status) => {
    let browserRef = weakBrowser.get();
    if (!Components.isSuccessCode(status) || !browserRef)
      return;

    let currentURI = browserRef.currentURI;
    // If we're in case (3) (see above), don't show an info bar.
    if (!currentURI.equals(previousURI) &&
        !currentURI.equals(preferredURI)) {
      return;
    }

    // show infobar offering to visit the host
    let notificationBox = gBrowser.getNotificationBox(browserRef);
    if (notificationBox.getNotificationWithValue("keyword-uri-fixup"))
      return;

    let message = gNavigatorBundle.getFormattedString(
      "keywordURIFixup.message", [hostName]);
    let yesMessage = gNavigatorBundle.getFormattedString(
      "keywordURIFixup.goTo", [hostName]);

    let buttons = [
      {
        label: yesMessage,
        accessKey: gNavigatorBundle.getString("keywordURIFixup.goTo.accesskey"),
        callback() {
          // Do not set this preference while in private browsing.
          if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
            let pref = "browser.fixup.domainwhitelist." + asciiHost;
            Services.prefs.setBoolPref(pref, true);
          }
          openTrustedLinkIn(alternativeURI.spec, "current");
        }
      },
      {
        label: gNavigatorBundle.getString("keywordURIFixup.dismiss"),
        accessKey: gNavigatorBundle.getString("keywordURIFixup.dismiss.accesskey"),
        callback() {
          let notification = notificationBox.getNotificationWithValue("keyword-uri-fixup");
          notificationBox.removeNotification(notification, true);
        }
      }
    ];
    let notification =
      notificationBox.appendNotification(message, "keyword-uri-fixup", null,
                                         notificationBox.PRIORITY_INFO_HIGH,
                                         buttons);
    notification.persistence = 1;
  };

  try {
    gDNSService.asyncResolve(hostName, 0, onLookupComplete, Services.tm.mainThread,
                             contentPrincipal.originAttributes);
  } catch (ex) {
    // Do nothing if the URL is invalid (we don't want to show a notification in that case).
    if (ex.result != Cr.NS_ERROR_UNKNOWN_HOST) {
      // ... otherwise, report:
      Cu.reportError(ex);
    }
  }
}

function serializeInputStream(aStream) {
  let data = {
    content: NetUtil.readInputStreamToString(aStream, aStream.available()),
  };

  if (aStream instanceof Ci.nsIMIMEInputStream) {
    data.headers = new Map();
    aStream.visitHeaders((name, value) => {
      data.headers.set(name, value);
    });
  }

  return data;
}

/**
 * Handles URIs when we want to deal with them in chrome code rather than pass
 * them down to a content browser. This can avoid unnecessary process switching
 * for the browser.
 * @param aBrowser the browser that is attempting to load the URI
 * @param aUri the nsIURI that is being loaded
 * @returns true if the URI is handled, otherwise false
 */
function handleUriInChrome(aBrowser, aUri) {
  if (aUri.scheme == "file") {
    try {
      let mimeType = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService)
                                              .getTypeFromURI(aUri);
      if (mimeType == "application/x-xpinstall") {
        let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
        AddonManager.getInstallForURL(aUri.spec, install => {
          AddonManager.installAddonFromWebpage(mimeType, aBrowser, systemPrincipal,
                                               install);
        }, mimeType);
        return true;
      }
    } catch (e) {
      return false;
    }
  }

  return false;
}

// A shared function used by both remote and non-remote browser XBL bindings to
// load a URI or redirect it to the correct process.
function _loadURI(browser, uri, params = {}) {
  let tab = gBrowser.getTabForBrowser(browser);
  // Preloaded browsers don't have tabs, so we ignore those.
  if (tab) {
    maybeRecordAbandonmentTelemetry(tab, "newURI");
  }

  if (!uri) {
    uri = "about:blank";
  }

  let {
    flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
    referrerURI,
    referrerPolicy = Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
    triggeringPrincipal,
    postData,
    userContextId,
  } = params || {};

  let currentRemoteType = browser.remoteType;
  let requiredRemoteType;
  try {
    let fixupFlags = Ci.nsIURIFixup.FIXUP_FLAG_NONE;
    if (flags & Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
      fixupFlags |= Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    }
    if (flags & Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS) {
      fixupFlags |= Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS;
    }
    let uriObject = Services.uriFixup.createFixupURI(uri, fixupFlags);
    if (handleUriInChrome(browser, uriObject)) {
      // If we've handled the URI in Chrome then just return here.
      return;
    }

    // Note that I had thought that we could set uri = uriObject.spec here, to
    // save on fixup later on, but that changes behavior and breaks tests.
    requiredRemoteType =
      E10SUtils.getRemoteTypeForURIObject(uriObject, gMultiProcessBrowser,
                                          currentRemoteType, browser.currentURI);
  } catch (e) {
    // createFixupURI throws if it can't create a URI. If that's the case then
    // we still need to pass down the uri because docshell handles this case.
    requiredRemoteType = gMultiProcessBrowser ? E10SUtils.DEFAULT_REMOTE_TYPE
                                              : E10SUtils.NOT_REMOTE;
  }

  let mustChangeProcess = requiredRemoteType != currentRemoteType;
  let newFrameloader = false;
  if (browser.getAttribute("preloadedState") === "consumed" && uri != "about:newtab") {
    // Leaving about:newtab from a used to be preloaded browser should run the process
    // selecting algorithm again.
    mustChangeProcess = true;
    newFrameloader = true;
    browser.removeAttribute("preloadedState");
  }

  // !requiredRemoteType means we're loading in the parent/this process.
  if (!requiredRemoteType) {
    browser.inLoadURI = true;
  }
  try {
    if (!mustChangeProcess) {
      if (userContextId) {
        browser.webNavigation.setOriginAttributesBeforeLoading({ userContextId });
      }

      browser.webNavigation.loadURIWithOptions(uri, flags,
                                               referrerURI, referrerPolicy,
                                               postData, null, null, triggeringPrincipal);
    } else {
      // Check if the current browser is allowed to unload.
      let {permitUnload, timedOut} = browser.permitUnload();
      if (!timedOut && !permitUnload) {
        return;
      }

      if (postData) {
        postData = serializeInputStream(postData);
      }

      let loadParams = {
        uri,
        triggeringPrincipal: triggeringPrincipal
          ? gSerializationHelper.serializeToString(triggeringPrincipal)
          : null,
        flags,
        referrer: referrerURI ? referrerURI.spec : null,
        referrerPolicy,
        remoteType: requiredRemoteType,
        postData,
        newFrameloader,
      };

      if (userContextId) {
        loadParams.userContextId = userContextId;
      }

      LoadInOtherProcess(browser, loadParams);
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

      if (userContextId) {
        browser.webNavigation.setOriginAttributesBeforeLoading({ userContextId });
      }

      browser.webNavigation.loadURIWithOptions(uri, flags, referrerURI, referrerPolicy,
                                               postData, null, null, triggeringPrincipal);
    } else {
      throw e;
    }
  } finally {
    if (!requiredRemoteType) {
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
  if (browser.getAttribute("preloadedState") === "consumed") {
    browser.removeAttribute("preloadedState");
    data.loadOptions.newFrameloader = true;
  }

  if (data.loadOptions.reloadInFreshProcess) {
    // Convert the fresh process load option into a large allocation remote type
    // to use common processing from this point.
    data.loadOptions.remoteType = E10SUtils.LARGE_ALLOCATION_REMOTE_TYPE;
    data.loadOptions.newFrameloader = true;
  } else if (browser.remoteType == E10SUtils.LARGE_ALLOCATION_REMOTE_TYPE) {
    // If we're in a Large-Allocation process, we prefer switching back into a
    // normal content process, as that way we can clean up the L-A process.
    data.loadOptions.remoteType =
      E10SUtils.getRemoteTypeForURI(data.loadOptions.uri, gMultiProcessBrowser);
  }

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
                             "browser-delayed-startup-finished");
  }
}

if (document.documentElement.getAttribute("windowtype") == "navigator:browser") {
  window.addEventListener("MozBeforeInitialXULLayout", () => {
    gBrowserInit.onBeforeInitialXULLayout();
  }, { once: true });
  // The listener of DOMContentLoaded must be set on window, rather than
  // document, because the window can go away before the event is fired.
  // In that case, we don't want to initialize anything, otherwise we
  // may be leaking things because they will never be destroyed after.
  window.addEventListener("DOMContentLoaded", () => {
    gBrowserInit.onDOMContentLoaded();
  }, { once: true });
}

let _resolveDelayedStartup;
var delayedStartupPromise = new Promise(resolve => {
  _resolveDelayedStartup = resolve;
});

var gBrowserInit = {
  delayedStartupFinished: false,

  onBeforeInitialXULLayout() {
    // Set a sane starting width/height for all resolutions on new profiles.
    if (Services.prefs.getBoolPref("privacy.resistFingerprinting")) {
      // When the fingerprinting resistance is enabled, making sure that we don't
      // have a maximum window to interfere with generating rounded window dimensions.
      document.documentElement.setAttribute("sizemode", "normal");
    } else if (!document.documentElement.hasAttribute("width")) {
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

    // Update the chromemargin attribute so the window can be sized correctly.
    window.TabBarVisibility.update();
    TabsInTitlebar.init();

    new LightweightThemeConsumer(document);
    CompactTheme.init();
    if (window.matchMedia("(-moz-os-version: windows-win8)").matches &&
        window.matchMedia("(-moz-windows-default-theme)").matches) {
      let windowFrameColor = new Color(...ChromeUtils.import("resource:///modules/Windows8WindowFrameColor.jsm", {})
                                            .Windows8WindowFrameColor.get());
      // Default to black for foreground text.
      if (!windowFrameColor.isContrastRatioAcceptable(new Color(0, 0, 0))) {
        document.documentElement.setAttribute("darkwindowframe", "true");
      }
    }
    ToolbarIconColor.init();
  },

  onDOMContentLoaded() {
    gBrowser = window._gBrowser;
    delete window._gBrowser;
    gBrowser.init();

    window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIWebNavigation)
          .QueryInterface(Ci.nsIDocShellTreeItem).treeOwner
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIXULWindow)
          .XULBrowserWindow = window.XULBrowserWindow;
    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow =
      new nsBrowserAccess();
    BrowserWindowTracker.track(window);

    let initBrowser = gBrowser.initialBrowser;

    // remoteType and sameProcessAsFrameLoader are passed through to
    // updateBrowserRemoteness as part of an options object, which itself defaults
    // to an empty object. So defaulting them to undefined here will cause the
    // default behavior in updateBrowserRemoteness if they don't get set.
    let isRemote = gMultiProcessBrowser;
    let remoteType;
    let sameProcessAsFrameLoader;

    let tabArgument = window.arguments && window.arguments[0];
    if (tabArgument instanceof XULElement) {
      // The window's first argument is a tab if and only if we are swapping tabs.
      // We must set the browser's usercontextid before updateBrowserRemoteness(),
      // so that the newly created remote tab child has the correct usercontextid.
      if (tabArgument.hasAttribute("usercontextid")) {
        initBrowser.setAttribute("usercontextid",
                                 tabArgument.getAttribute("usercontextid"));
      }

      let linkedBrowser = tabArgument.linkedBrowser;
      if (linkedBrowser) {
        remoteType = linkedBrowser.remoteType;
        isRemote = remoteType != E10SUtils.NOT_REMOTE;
        sameProcessAsFrameLoader = linkedBrowser.frameLoader;
      }
      initBrowser.removeAttribute("blank");
    }

    gBrowser.updateBrowserRemoteness(initBrowser, isRemote, {
      remoteType, sameProcessAsFrameLoader
    });

    // Hack to ensure that the about:home favicon is loaded
    // instantaneously, to avoid flickering and improve perceived performance.
    this._callWithURIToLoad(uriToLoad => {
      if (uriToLoad == "about:home") {
        gBrowser.setIcon(gBrowser.selectedTab, "chrome://branding/content/icon32.png");
      } else if (uriToLoad == "about:privatebrowsing") {
        gBrowser.setIcon(gBrowser.selectedTab, "chrome://browser/skin/privatebrowsing/favicon.svg");
      }
    });

    this._setInitialFocus();

    // Update the UI density before TabsInTitlebar lays out the titlbar.
    gUIDensity.init();
    TabsInTitlebar.whenWindowLayoutReady();
  },

  onLoad() {
    gBrowser.addEventListener("DOMUpdateBlockedPopups", gPopupBlockerObserver);

    Services.obs.addObserver(gPluginHandler.NPAPIPluginCrashed, "plugin-crashed");

    window.addEventListener("AppCommand", HandleAppCommandEvent, true);

    // These routines add message listeners. They must run before
    // loading the frame script to ensure that we don't miss any
    // message sent between when the frame script is loaded and when
    // the listener is registered.
    DOMEventHandler.init();
    gPageStyleMenu.init();
    LanguageDetectionListener.init();
    BrowserOnClick.init();
    FeedHandler.init();
    AboutCapabilitiesListener.init();
    TrackingProtection.init();
    CaptivePortalWatcher.init();
    ZoomUI.init(window);

    let mm = window.getGroupMessageManager("browsers");
    mm.loadFrameScript("chrome://browser/content/tab-content.js", true);
    mm.loadFrameScript("chrome://browser/content/content.js", true);
    mm.loadFrameScript("chrome://browser/content/content-UITour.js", true);
    mm.loadFrameScript("chrome://global/content/content-HybridContentTelemetry.js", true);
    mm.loadFrameScript("chrome://global/content/manifestMessages.js", true);

    window.messageManager.addMessageListener("Browser:LoadURI", RedirectLoad);

    if (!gMultiProcessBrowser) {
      // There is a Content:Click message manually sent from content.
      Services.els.addSystemEventListener(gBrowser.tabpanels, "click",
        contentAreaClick, true);
    }

    // hook up UI through progress listener
    gBrowser.addProgressListener(window.XULBrowserWindow);
    gBrowser.addTabsProgressListener(window.TabsProgressListener);

    SidebarUI.init();

    // We do this in onload because we want to ensure the button's state
    // doesn't flicker as the window is being shown.
    DownloadsButton.init();

    // Certain kinds of automigration rely on this notification to complete
    // their tasks BEFORE the browser window is shown. SessionStore uses it to
    // restore tabs into windows AFTER important parts like gMultiProcessBrowser
    // have been initialized.
    Services.obs.notifyObservers(window, "browser-window-before-show");

    if (!window.toolbar.visible) {
      // adjust browser UI for popups
      gURLBar.setAttribute("readonly", "true");
    }

    // Misc. inits.
    TabletModeUpdater.init();
    CombinedStopReload.ensureInitialized();
    gPrivateBrowsingUI.init();
    BrowserSearch.init();
    BrowserPageActions.init();
    gAccessibilityServiceIndicator.init();

    gRemoteControl.updateVisualCue(Marionette.running);

    // If we are given a tab to swap in, take care of it before first paint to
    // avoid an about:blank flash.
    let tabToOpen = window.arguments && window.arguments[0];
    if (tabToOpen instanceof XULElement) {
      // Clear the reference to the tab from the arguments array.
      window.arguments[0] = null;

      // Stop the about:blank load
      gBrowser.stop();
      // make sure it has a docshell
      gBrowser.docShell;

      try {
        gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, tabToOpen);
      } catch (e) {
        Cu.reportError(e);
      }
    }

    // Wait until chrome is painted before executing code not critical to making the window visible
    this._boundDelayedStartup = this._delayedStartup.bind(this);
    window.addEventListener("MozAfterPaint", this._boundDelayedStartup);

    if (!PrivateBrowsingUtils.enabled) {
      document.getElementById("Tools:PrivateBrowsing").hidden = true;
      // Setting disabled doesn't disable the shortcut, so we just remove
      // the keybinding.
      document.getElementById("key_privatebrowsing").remove();
    }

    this._loadHandled = true;
  },

  _cancelDelayedStartup() {
    window.removeEventListener("MozAfterPaint", this._boundDelayedStartup);
    this._boundDelayedStartup = null;
  },

  _delayedStartup() {
    let { TelemetryTimestamps } =
      ChromeUtils.import("resource://gre/modules/TelemetryTimestamps.jsm", {});
    TelemetryTimestamps.add("delayedStartupStarted");

    this._cancelDelayedStartup();

    // We need to set the OfflineApps message listeners up before we
    // load homepages, which might need them.
    OfflineApps.init();

    // This pageshow listener needs to be registered before we may call
    // swapBrowsersAndCloseOther() to receive pageshow events fired by that.
    window.messageManager.addMessageListener("PageVisibility:Show", function(message) {
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
    }, true);

    gBrowser.addEventListener("PermissionStateChange", function() {
      gIdentityHandler.refreshIdentityBlock();
    }, true);

    // Get the service so that it initializes and registers listeners for new
    // tab pages in order to be ready for any early-loading about:newtab pages,
    // e.g., start/home page, command line / startup uris to load, sessionstore
    gAboutNewTabService.QueryInterface(Ci.nsISupports);

    this._handleURIToLoad();

    Services.obs.addObserver(gIdentityHandler, "perm-changed");
    Services.obs.addObserver(gRemoteControl, "remote-active");
    Services.obs.addObserver(gSessionHistoryObserver, "browser:purge-session-history");
    Services.obs.addObserver(gStoragePressureObserver, "QuotaManager::StoragePressure");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-disabled");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-started");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-blocked");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-origin-blocked");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-failed");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-confirmation");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-complete");
    window.messageManager.addMessageListener("Browser:URIFixup", gKeywordURIFixup);

    BrowserOffline.init();
    IndexedDBPromptHelper.init();
    CanvasPermissionPromptHelper.init();
    WebAuthnPromptHelper.init();

    // Initialize the full zoom setting.
    // We do this before the session restore service gets initialized so we can
    // apply full zoom settings to tabs restored by the session restore service.
    FullZoom.init();
    PanelUI.init();

    UpdateUrlbarSearchSplitterState();

    BookmarkingUI.init();
    AutoShowBookmarksToolbar.init();

    Services.prefs.addObserver(gHomeButton.prefDomain, gHomeButton);

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

    PlacesToolbarHelper.init();

    ctrlTab.readPref();
    Services.prefs.addObserver(ctrlTab.prefName, ctrlTab);

    // The object handling the downloads indicator is initialized here in the
    // delayed startup function, but the actual indicator element is not loaded
    // unless there are downloads to be displayed.
    DownloadsButton.initializeIndicator();

    if (AppConstants.platform != "macosx") {
      updateEditUIVisibility();
      let placesContext = document.getElementById("placesContext");
      placesContext.addEventListener("popupshowing", updateEditUIVisibility);
      placesContext.addEventListener("popuphiding", updateEditUIVisibility);
    }

    LightWeightThemeWebInstaller.init();

    if (Win7Features)
      Win7Features.onOpenWindow();

    FullScreen.init();
    PointerLock.init();

    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      MenuTouchModeObserver.init();
    }

    if (AppConstants.MOZ_DATA_REPORTING)
      gDataNotificationInfoBar.init();

    if (!AppConstants.MOZILLA_OFFICIAL)
      DevelopmentHelpers.init();

    gExtensionsNotifications.init();

    let wasMinimized = window.windowState == window.STATE_MINIMIZED;
    window.addEventListener("sizemodechange", () => {
      let isMinimized = window.windowState == window.STATE_MINIMIZED;
      if (wasMinimized != isMinimized) {
        wasMinimized = isMinimized;
        UpdatePopupNotificationsVisibility();
      }
    });

    window.addEventListener("mousemove", MousePosTracker);
    window.addEventListener("dragover", MousePosTracker);

    gNavToolbox.addEventListener("customizationstarting", CustomizationHandler);
    gNavToolbox.addEventListener("customizationending", CustomizationHandler);

    if (AppConstants.platform == "linux") {
      let { WindowDraggingElement } =
        ChromeUtils.import("resource://gre/modules/WindowDraggingUtils.jsm", {});
      new WindowDraggingElement(document.getElementById("titlebar"));
    }

    SessionStore.promiseInitialized.then(() => {
      // Bail out if the window has been closed in the meantime.
      if (window.closed) {
        return;
      }

      // Enable the Restore Last Session command if needed
      RestoreLastSessionObserver.init();

      SidebarUI.startDelayedLoad();

      PanicButtonNotifier.init();
    });

    gBrowser.tabContainer.addEventListener("TabSelect", gURLBar);

    gBrowser.tabContainer.addEventListener("TabSelect", function() {
      for (let panel of document.querySelectorAll("panel[tabspecific='true']")) {
        if (panel.state == "open") {
          panel.hidePopup();
        }
      }
    });

    this.delayedStartupFinished = true;

    _resolveDelayedStartup();

    SessionStore.promiseAllWindowsRestored.then(() => {
      this._schedulePerWindowIdleTasks();
      document.documentElement.setAttribute("sessionrestored", "true");
    });

    Services.obs.notifyObservers(window, "browser-delayed-startup-finished");
    TelemetryTimestamps.add("delayedStartupFinished");
  },

  _setInitialFocus() {
    let initiallyFocusedElement = document.commandDispatcher.focusedElement;

    let firstBrowserPaintDeferred = {};
    firstBrowserPaintDeferred.promise = new Promise(resolve => {
      firstBrowserPaintDeferred.resolve = resolve;
    });

    let mm = window.messageManager;
    mm.addMessageListener("Browser:FirstPaint", function onFirstPaint() {
      mm.removeMessageListener("Browser:FirstPaint", onFirstPaint);
      firstBrowserPaintDeferred.resolve();
    });

    let initialBrowser = gBrowser.selectedBrowser;
    mm.addMessageListener("Browser:FirstNonBlankPaint",
                          function onFirstNonBlankPaint() {
      mm.removeMessageListener("Browser:FirstNonBlankPaint", onFirstNonBlankPaint);
      initialBrowser.removeAttribute("blank");
    });

    // To prevent flickering of the urlbar-history-dropmarker in the general
    // case, the urlbar has the 'focused' attribute set by default.
    // If we are not fully sure the urlbar will be focused in this window,
    // we should remove the attribute before first paint.
    let shouldRemoveFocusedAttribute = true;
    this._callWithURIToLoad(uriToLoad => {
      if ((isBlankPageURL(uriToLoad) || uriToLoad == "about:privatebrowsing") &&
          focusAndSelectUrlBar()) {
        shouldRemoveFocusedAttribute = false;
        return;
      }

      if (gBrowser.selectedBrowser.isRemoteBrowser) {
        // If the initial browser is remote, in order to optimize for first paint,
        // we'll defer switching focus to that browser until it has painted.
        firstBrowserPaintDeferred.promise.then(() => {
          // If focus didn't move while we were waiting for first paint, we're okay
          // to move to the browser.
          if (document.commandDispatcher.focusedElement == initiallyFocusedElement) {
            gBrowser.selectedBrowser.focus();
          }
        });
      } else {
        // If the initial browser is not remote, we can focus the browser
        // immediately with no paint performance impact.
        gBrowser.selectedBrowser.focus();
      }
    });
    // Delay removing the attribute using requestAnimationFrame to avoid
    // invalidating styles multiple times in a row if _uriToLoadPromise
    // resolves before first paint.
    if (shouldRemoveFocusedAttribute) {
      window.requestAnimationFrame(() => {
        if (shouldRemoveFocusedAttribute)
          gURLBar.removeAttribute("focused");
      });
    }
  },

  _handleURIToLoad() {
    this._callWithURIToLoad(uriToLoad => {
      if (!uriToLoad || uriToLoad == "about:blank") {
        return;
      }

      // We don't check if uriToLoad is a XULElement because this case has
      // already been handled before first paint, and the argument cleared.
      if (uriToLoad instanceof Ci.nsIArray) {
        let count = uriToLoad.length;
        let specs = [];
        for (let i = 0; i < count; i++) {
          let urisstring = uriToLoad.queryElementAt(i, Ci.nsISupportsString);
          specs.push(urisstring.data);
        }

        // This function throws for certain malformed URIs, so use exception handling
        // so that we don't disrupt startup
        try {
          gBrowser.loadTabs(specs, {
            inBackground: false,
            replace: true,
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          });
        } catch (e) {}
      } else if (window.arguments.length >= 3) {
        // window.arguments[2]: referrer (nsIURI | string)
        //                 [3]: postData (nsIInputStream)
        //                 [4]: allowThirdPartyFixup (bool)
        //                 [5]: referrerPolicy (int)
        //                 [6]: userContextId (int)
        //                 [7]: originPrincipal (nsIPrincipal)
        //                 [8]: triggeringPrincipal (nsIPrincipal)
        let referrerURI = window.arguments[2];
        if (typeof(referrerURI) == "string") {
          try {
            referrerURI = makeURI(referrerURI);
          } catch (e) {
            referrerURI = null;
          }
        }
        let referrerPolicy = (window.arguments[5] != undefined ?
            window.arguments[5] : Ci.nsIHttpChannel.REFERRER_POLICY_UNSET);
        let userContextId = (window.arguments[6] != undefined ?
            window.arguments[6] : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID);
        loadURI(uriToLoad, referrerURI, window.arguments[3] || null,
                window.arguments[4] || false, referrerPolicy, userContextId,
                // pass the origin principal (if any) and force its use to create
                // an initial about:blank viewer if present:
                window.arguments[7], !!window.arguments[7], window.arguments[8]);
        window.focus();
      } else {
        // Note: loadOneOrMoreURIs *must not* be called if window.arguments.length >= 3.
        // Such callers expect that window.arguments[0] is handled as a single URI.
        loadOneOrMoreURIs(uriToLoad, Services.scriptSecurityManager.getSystemPrincipal());
      }
    });
  },

  /**
   * Use this function as an entry point to schedule tasks that
   * need to run once per window after startup, and can be scheduled
   * by using an idle callback.
   *
   * The functions scheduled here will fire from idle callbacks
   * once every window has finished being restored by session
   * restore, and after the equivalent only-once tasks
   * have run (from _scheduleStartupIdleTasks in nsBrowserGlue.js).
   */
  _schedulePerWindowIdleTasks() {
    // Bail out if the window has been closed in the meantime.
    if (window.closed) {
      return;
    }

    function scheduleIdleTask(func, options) {
      requestIdleCallback(function idleTaskRunner() {
        if (!window.closed) {
          func();
        }
      }, options);
    }

    scheduleIdleTask(() => {
      // Initialize the Sync UI
      gSync.init();
    });

    scheduleIdleTask(() => {
      CombinedStopReload.startAnimationPrefMonitoring();
    });

    scheduleIdleTask(() => {
      // setup simple gestures support
      gGestureSupport.init(true);

      // setup history swipe animation
      gHistorySwipeAnimation.init();
    });

    scheduleIdleTask(() => {
      gBrowserThumbnails.init();
    });

    scheduleIdleTask(() => {
      // Initialize the download manager some time after the app starts so that
      // auto-resume downloads begin (such as after crashing or quitting with
      // active downloads) and speeds up the first-load of the download manager UI.
      // If the user manually opens the download manager before the timeout, the
      // downloads will start right away, and initializing again won't hurt.
      try {
        DownloadsCommon.initializeAllDataLinks();
        ChromeUtils.import("resource:///modules/DownloadsTaskbar.jsm", {})
          .DownloadsTaskbar.registerIndicator(window);
      } catch (ex) {
        Cu.reportError(ex);
      }
    }, {timeout: 10000});
  },

  // Returns the URI(s) to load at startup if it is immediately known, or a
  // promise resolving to the URI to load.
  get _uriToLoadPromise() {
    delete this._uriToLoadPromise;
    return this._uriToLoadPromise = function() {
      // window.arguments[0]: URI to load (string), or an nsIArray of
      //                      nsISupportsStrings to load, or a xul:tab of
      //                      a tabbrowser, which will be replaced by this
      //                      window (for this case, all other arguments are
      //                      ignored).
      if (!window.arguments || !window.arguments[0]) {
        return null;
      }

      let uri = window.arguments[0];
      let defaultArgs = Cc["@mozilla.org/browser/clh;1"]
                          .getService(Ci.nsIBrowserHandler)
                          .defaultArgs;

      // If the given URI is different from the homepage, we want to load it.
      if (uri != defaultArgs) {
        return uri;
      }

      // The URI appears to be the the homepage. We want to load it only if
      // session restore isn't about to override the homepage.
      let willOverride = SessionStartup.willOverrideHomepage;
      if (typeof willOverride == "boolean") {
        return willOverride ? null : uri;
      }
      return willOverride.then(willOverrideHomepage =>
                                 willOverrideHomepage ? null : uri);
    }();
  },

  // Calls the given callback with the URI to load at startup.
  // Synchronously if possible, or after _uriToLoadPromise resolves otherwise.
  _callWithURIToLoad(callback) {
    let uriToLoad = this._uriToLoadPromise;
    if (!uriToLoad || !uriToLoad.then)
      callback(uriToLoad);
    else
      uriToLoad.then(callback);
  },

  onUnload() {
    gUIDensity.uninit();

    TabsInTitlebar.uninit();

    ToolbarIconColor.uninit();

    CompactTheme.uninit();

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

    gSync.uninit();

    gExtensionsNotifications.uninit();

    Services.obs.removeObserver(gPluginHandler.NPAPIPluginCrashed, "plugin-crashed");

    try {
      gBrowser.removeProgressListener(window.XULBrowserWindow);
      gBrowser.removeTabsProgressListener(window.TabsProgressListener);
    } catch (ex) {
    }

    PlacesToolbarHelper.uninit();

    BookmarkingUI.uninit();

    TabletModeUpdater.uninit();

    gTabletModePageCounter.finish();

    BrowserOnClick.uninit();

    FeedHandler.uninit();

    AboutCapabilitiesListener.uninit();

    TrackingProtection.uninit();

    CaptivePortalWatcher.uninit();

    SidebarUI.uninit();

    DownloadsButton.uninit();

    gAccessibilityServiceIndicator.uninit();

    LanguagePrompt.uninit();

    BrowserSearch.uninit();

    // Now either cancel delayedStartup, or clean up the services initialized from
    // it.
    if (this._boundDelayedStartup) {
      this._cancelDelayedStartup();
    } else {
      if (Win7Features)
        Win7Features.onCloseWindow();

      Services.prefs.removeObserver(ctrlTab.prefName, ctrlTab);
      ctrlTab.uninit();
      gBrowserThumbnails.uninit();
      FullZoom.destroy();

      Services.obs.removeObserver(gIdentityHandler, "perm-changed");
      Services.obs.removeObserver(gRemoteControl, "remote-active");
      Services.obs.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
      Services.obs.removeObserver(gStoragePressureObserver, "QuotaManager::StoragePressure");
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
        Services.prefs.removeObserver(gHomeButton.prefDomain, gHomeButton);
      } catch (ex) {
        Cu.reportError(ex);
      }

      if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
        MenuTouchModeObserver.uninit();
      }
      BrowserOffline.uninit();
      IndexedDBPromptHelper.uninit();
      CanvasPermissionPromptHelper.uninit();
      WebAuthnPromptHelper.uninit();
      PanelUI.uninit();
      AutoShowBookmarksToolbar.uninit();
    }

    // Final window teardown, do this last.
    gBrowser.destroy();
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
  // macWindow.inc.xul
  gBrowserInit.nonBrowserWindowStartup = function() {
    // Disable inappropriate commands / submenus
    var disabledItems = ["Browser:SavePage",
                         "Browser:SendLink", "cmd_pageSetup", "cmd_print", "cmd_find", "cmd_findAgain",
                         "viewToolbarsMenu", "viewSidebarMenuMenu", "Browser:Reload",
                         "viewFullZoomMenu", "pageStyleMenu", "charsetMenu", "View:PageSource", "View:FullScreen",
                         "viewHistorySidebar", "Browser:AddBookmarkAs", "Browser:BookmarkAllTabs",
                         "View:PageInfo"];
    var element;

    for (let disabledItem of disabledItems) {
      element = document.getElementById(disabledItem);
      if (element)
        element.setAttribute("disabled", "true");
    }

    // If no windows are active (i.e. we're the hidden window), disable the close, minimize
    // and zoom menu commands as well
    if (window.location.href == "chrome://browser/content/hiddenWindow.xul") {
      var hiddenWindowDisabledItems = ["cmd_close", "minimizeWindow", "zoomWindow"];
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
        } catch (e) {
        }
      }
    }

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      document.getElementById("macDockMenuNewWindow").hidden = true;
    }
    if (!PrivateBrowsingUtils.enabled) {
      document.getElementById("macDockMenuNewPrivateWindow").hidden = true;
    }

    this._delayedStartupTimeoutId = setTimeout(this.nonBrowserWindowDelayedStartup.bind(this), 0);
  };

  gBrowserInit.nonBrowserWindowDelayedStartup = function() {
    this._delayedStartupTimeoutId = null;

    // initialise the offline listener
    BrowserOffline.init();

    // initialize the private browsing UI
    gPrivateBrowsingUI.init();

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
    gLazyFindCommand("onFindCommand");
    break;
  case "Help":
    openHelpLink("firefox-help");
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

function maybeRecordAbandonmentTelemetry(tab, type) {
  if (!tab.hasAttribute("busy")) {
    return;
  }

  let histogram = Services.telemetry
                          .getHistogramById("BUSY_TAB_ABANDONED");
  histogram.add(type);
}

function gotoHistoryIndex(aEvent) {
  let index = aEvent.target.getAttribute("index");
  if (!index)
    return false;

  let where = whereToOpenLink(aEvent);

  if (where == "current") {
    // Normal click. Go there in the current tab and update session history.

    try {
      maybeRecordAbandonmentTelemetry(gBrowser.selectedTab,
                                      "historyNavigation");
      gBrowser.gotoIndex(index);
    } catch (ex) {
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
      maybeRecordAbandonmentTelemetry(gBrowser.selectedTab, "forward");
      gBrowser.goForward();
    } catch (ex) {
    }
  } else {
    duplicateTabIn(gBrowser.selectedTab, where, 1);
  }
}

function BrowserBack(aEvent) {
  let where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      maybeRecordAbandonmentTelemetry(gBrowser.selectedTab, "back");
      gBrowser.goBack();
    } catch (ex) {
    }
  } else {
    duplicateTabIn(gBrowser.selectedTab, where, -1);
  }
}

function BrowserHandleBackspace() {
  switch (Services.prefs.getIntPref("browser.backspace_action")) {
  case 0:
    BrowserBack();
    break;
  case 1:
    goDoCommand("cmd_scrollPageUp");
    break;
  }
}

function BrowserHandleShiftBackspace() {
  switch (Services.prefs.getIntPref("browser.backspace_action")) {
  case 0:
    BrowserForward();
    break;
  case 1:
    goDoCommand("cmd_scrollPageDown");
    break;
  }
}

function BrowserStop() {
  maybeRecordAbandonmentTelemetry(gBrowser.selectedTab, "stop");
  gBrowser.webNavigation.stop(Ci.nsIWebNavigation.STOP_ALL);
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
  const reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
  BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadSkipCache() {
  // Bypass proxy and cache.
  const reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
                      Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
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
  var notifyObservers;

  // Home page should open in a new tab when current tab is an app tab
  if (where == "current" &&
      gBrowser &&
      gBrowser.selectedTab.pinned)
    where = "tab";

  // openTrustedLinkIn in utilityOverlay.js doesn't handle loading multiple pages
  switch (where) {
  case "current":
    loadOneOrMoreURIs(homePage, Services.scriptSecurityManager.getSystemPrincipal());
    gBrowser.selectedBrowser.focus();
    notifyObservers = true;
    break;
  case "tabshifted":
  case "tab":
    urls = homePage.split("|");
    var loadInBackground = getBoolPref("browser.tabs.loadBookmarksInBackground", false);
    // The homepage observer event should only be triggered when the homepage opens
    // in the foreground. This is mostly to support the homepage changed by extension
    // doorhanger which doesn't currently support background pages. This may change in
    // bug 1438396.
    notifyObservers = !loadInBackground;
    gBrowser.loadTabs(urls, {
      inBackground: loadInBackground,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    break;
  case "window":
    // OpenBrowserWindow will trigger the observer event, so no need to do so here.
    notifyObservers = false;
    OpenBrowserWindow();
    break;
  }
  if (notifyObservers) {
    // A notification for when a user has triggered their homepage. This is used
    // to display a doorhanger explaining that an extension has modified the
    // homepage, if necessary. Observers are only notified if the homepage
    // becomes the active page.
    Services.obs.notifyObservers(null, "browser-open-homepage-start");
  }
}

function loadOneOrMoreURIs(aURIString, aTriggeringPrincipal) {
  // we're not a browser window, pass the URI string to a new browser window
  if (window.location.href != getBrowserURL()) {
    window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", aURIString);
    return;
  }

  // This function throws for certain malformed URIs, so use exception handling
  // so that we don't disrupt startup
  try {
    gBrowser.loadTabs(aURIString.split("|"), {
      inBackground: false,
      replace: true,
      triggeringPrincipal: aTriggeringPrincipal,
    });
  } catch (e) {
  }
}

/**
 * Focuses the location bar input field and selects its contents.
 *
 * @param [optional] userInitiatedFocus
 *        Whether this focus is caused by an user interaction whose intention
 *        was to use the location bar. For example, using a shortcut to go to
 *        the location bar, or a contextual menu to search from it.
 *        The default is false and should be used in all those cases where the
 *        code focuses the location bar but that's not the primary user
 *        intention, like when opening a new tab.
 */
function focusAndSelectUrlBar(userInitiatedFocus = false) {
  // In customize mode, the url bar is disabled. If a new tab is opened or the
  // user switches to a different tab, this function gets called before we've
  // finished leaving customize mode, and the url bar will still be disabled.
  // We can't focus it when it's disabled, so we need to re-run ourselves when
  // we've finished leaving customize mode.
  if (CustomizationHandler.isExitingCustomizeMode) {
    gNavToolbox.addEventListener("aftercustomization", function() {
      focusAndSelectUrlBar(userInitiatedFocus);
    }, {once: true});

    return true;
  }

  if (gURLBar) {
    if (window.fullScreen)
      FullScreen.showNavToolbox();

    gURLBar.userInitiatedFocus = userInitiatedFocus;
    gURLBar.select();
    gURLBar.userInitiatedFocus = false;
    if (document.activeElement == gURLBar.inputField)
      return true;
  }
  return false;
}

function openLocation() {
  if (focusAndSelectUrlBar(true))
    return;

  if (window.location.href != getBrowserURL()) {
    var win = getTopWin();
    if (win) {
      // If there's an open browser window, it should handle this command
      win.focus();
      win.openLocation();
    } else {
      // If there are no open browser windows, open a new one
      window.openDialog("chrome://browser/content/", "_blank",
                        "chrome,all,dialog=no", BROWSER_NEW_TAB_URL);
    }
  }
}

function BrowserOpenTab(event) {
  // A notification intended to be useful for modular peformance tracking
  // starting as close as is reasonably possible to the time when the user
  // expressed the intent to open a new tab.  Since there are a lot of
  // entry points, this won't catch every single tab created, but most
  // initiated by the user should go through here.
  //
  // This is also used to notify a user that an extension has changed the
  // New Tab page.
  Services.obs.notifyObservers(null, "browser-open-newtab-start");

  let where = "tab";
  let relatedToCurrent = false;

  if (event) {
    where = whereToOpenLink(event, false, true);

    switch (where) {
      case "tab":
      case "tabshifted":
        // When accel-click or middle-click are used, open the new tab as
        // related to the current tab.
        relatedToCurrent = true;
        break;
      case "current":
        where = "tab";
        break;
    }
  }

  openTrustedLinkIn(BROWSER_NEW_TAB_URL, where, {
    relatedToCurrent,
  });
}

var gLastOpenDirectory = {
  _lastDir: null,
  get path() {
    if (!this._lastDir || !this._lastDir.exists()) {
      try {
        this._lastDir = Services.prefs.getComplexValue("browser.open.lastDir",
                                                       Ci.nsIFile);
        if (!this._lastDir.exists())
          this._lastDir = null;
      } catch (e) {}
    }
    return this._lastDir;
  },
  set path(val) {
    try {
      if (!val || !val.isDirectory())
        return;
    } catch (e) {
      return;
    }
    this._lastDir = val.clone();

    // Don't save the last open directory pref inside the Private Browsing mode
    if (!PrivateBrowsingUtils.isWindowPrivate(window))
      Services.prefs.setComplexValue("browser.open.lastDir", Ci.nsIFile,
                                     this._lastDir);
  },
  reset() {
    this._lastDir = null;
  }
};

function BrowserOpenFileWindow() {
  // Get filepicker component.
  try {
    const nsIFilePicker = Ci.nsIFilePicker;
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    let fpCallback = function fpCallback_done(aResult) {
      if (aResult == nsIFilePicker.returnOK) {
        try {
          if (fp.file) {
            gLastOpenDirectory.path =
              fp.file.parent.QueryInterface(Ci.nsIFile);
          }
        } catch (ex) {
        }
        openTrustedLinkIn(fp.fileURL.spec, "current");
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

function BrowserCloseTabOrWindow(event) {
  // If we're not a browser window, just close the window.
  if (window.location.href != getBrowserURL()) {
    closeWindow(true);
    return;
  }

  // Keyboard shortcuts that would close a tab that is pinned select the first
  // unpinned tab instead.
  if (event &&
      (event.ctrlKey || event.metaKey || event.altKey) &&
      gBrowser.selectedTab.pinned) {
    if (gBrowser.visibleTabs.length > gBrowser._numPinnedTabs) {
      gBrowser.tabContainer.selectedIndex = gBrowser._numPinnedTabs;
    }
    return;
  }

  // If the current tab is the last one, this will close the window.
  gBrowser.removeCurrentTab({animate: true});
}

function BrowserTryToCloseWindow() {
  if (WindowIsClosing())
    window.close(); // WindowIsClosing does all the necessary checks
}

function loadURI(uri, referrer, postData, allowThirdPartyFixup, referrerPolicy,
                 userContextId, originPrincipal, forceAboutBlankViewerInCurrent,
                 triggeringPrincipal) {
  try {
    openLinkIn(uri, "current",
               { referrerURI: referrer,
                 referrerPolicy,
                 postData,
                 allowThirdPartyFixup,
                 userContextId,
                 originPrincipal,
                 triggeringPrincipal,
                 forceAboutBlankViewerInCurrent,
               });
  } catch (e) {}
}

/**
 * Given a string, will generate a more appropriate urlbar value if a Places
 * keyword or a search alias is found at the beginning of it.
 *
 * @param url
 *        A string that may begin with a keyword or an alias.
 *
 * @return {Promise}
 * @resolves { url, postData, mayInheritPrincipal }. If it's not possible
 *           to discern a keyword or an alias, url will be the input string.
 */
function getShortcutOrURIAndPostData(url, callback = null) {
  if (callback) {
    Deprecated.warning("Please use the Promise returned by " +
                       "getShortcutOrURIAndPostData() instead of passing a " +
                       "callback",
                       "https://bugzilla.mozilla.org/show_bug.cgi?id=1100294");
  }
  return (async function() {
    let mayInheritPrincipal = false;
    let postData = null;
    // Split on the first whitespace.
    let [keyword, param = ""] = url.trim().split(/\s(.+)/, 2);

    if (!keyword) {
      return { url, postData, mayInheritPrincipal };
    }

    let engine = Services.search.getEngineByAlias(keyword);
    if (engine) {
      let submission = engine.getSubmission(param, null, "keyword");
      return { url: submission.uri.spec,
               postData: submission.postData,
               mayInheritPrincipal };
    }

    // A corrupt Places database could make this throw, breaking navigation
    // from the location bar.
    let entry = null;
    try {
      entry = await PlacesUtils.keywords.fetch(keyword);
    } catch (ex) {
      Cu.reportError(`Unable to fetch Places keyword "${keyword}": ${ex}`);
    }
    if (!entry || !entry.url) {
      // This is not a Places keyword.
      return { url, postData, mayInheritPrincipal };
    }

    try {
      [url, postData] =
        await BrowserUtils.parseUrlAndPostData(entry.url.href,
                                               entry.postData,
                                               param);
      if (postData) {
        postData = getPostDataStream(postData);
      }

      // Since this URL came from a bookmark, it's safe to let it inherit the
      // current document's principal.
      mayInheritPrincipal = true;
    } catch (ex) {
      // It was not possible to bind the param, just use the original url value.
    }

    return { url, postData, mayInheritPrincipal };
  })().then(data => {
    if (callback) {
      callback(data);
    }
    return data;
  });
}

function getPostDataStream(aPostDataString,
                           aType = "application/x-www-form-urlencoded") {
  let dataStream = Cc["@mozilla.org/io/string-input-stream;1"]
                     .createInstance(Ci.nsIStringInputStream);
  dataStream.data = aPostDataString;

  let mimeStream = Cc["@mozilla.org/network/mime-input-stream;1"]
                     .createInstance(Ci.nsIMIMEInputStream);
  mimeStream.addHeader("Content-Type", aType);
  mimeStream.setData(dataStream);
  return mimeStream.QueryInterface(Ci.nsIInputStream);
}

function getLoadContext() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsILoadContext);
}

function readFromClipboard() {
  var url;

  try {
    // Create transferable that will transfer the text.
    var trans = Cc["@mozilla.org/widget/transferable;1"]
                  .createInstance(Ci.nsITransferable);
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
      data = data.value.QueryInterface(Ci.nsISupportsString);
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
async function BrowserViewSourceOfDocument(aArgsOrDocument) {
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

  // Check if external view source is enabled.  If so, try it.  If it fails,
  // fallback to internal view source.
  if (Services.prefs.getBoolPref("view_source.editor.external")) {
    try {
      await top.gViewSourceUtils.openInExternalEditor(args);
      return;
    } catch (data) {}
  }

  let tabBrowser = gBrowser;
  let preferredRemoteType;
  if (args.browser) {
    preferredRemoteType = args.browser.remoteType;
  } else {
    if (!tabBrowser) {
      throw new Error("BrowserViewSourceOfDocument should be passed the " +
                      "subject browser if called from a window without " +
                      "gBrowser defined.");
    }
    // Some internal URLs (such as specific chrome: and about: URLs that are
    // not yet remote ready) cannot be loaded in a remote browser.  View
    // source in tab expects the new view source browser's remoteness to match
    // that of the original URL, so disable remoteness if necessary for this
    // URL.
    preferredRemoteType =
      E10SUtils.getRemoteTypeForURI(args.URL, gMultiProcessBrowser);
  }

  // In the case of popups, we need to find a non-popup browser window.
  if (!tabBrowser || !window.toolbar.visible) {
    // This returns only non-popup browser windows by default.
    let browserWindow = BrowserWindowTracker.getTopWindow();
    tabBrowser = browserWindow.gBrowser;
  }

  // `viewSourceInBrowser` will load the source content from the page
  // descriptor for the tab (when possible) or fallback to the network if
  // that fails.  Either way, the view source module will manage the tab's
  // location, so use "about:blank" here to avoid unnecessary redundant
  // requests.
  let tab = tabBrowser.loadOneTab("about:blank", {
    relatedToCurrent: true,
    inBackground: false,
    preferredRemoteType,
    sameProcessAsFrameLoader: args.browser ? args.browser.frameLoader : null,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  args.viewSourceBrowser = tabBrowser.getBrowserForTab(tab);
  top.gViewSourceUtils.viewSourceInBrowser(args);
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
    browser,
    outerWindowID: browser.outerWindowID,
    URL: browser.currentURI.spec,
  });
}

// documentURL - URL of the document to view, or null for this window's document
// initialTab - name of the initial tab to display, or null for the first tab
// imageElement - image to load in the Media Tab of the Page Info window; can be null/omitted
// frameOuterWindowID - the id of the frame that the context menu opened in; can be null/omitted
// browser - the browser containing the document we're interested in inspecting; can be null/omitted
function BrowserPageInfo(documentURL, initialTab, imageElement, frameOuterWindowID, browser) {
  if (documentURL instanceof HTMLDocument) {
    Deprecated.warning("Please pass the location URL instead of the document " +
                       "to BrowserPageInfo() as the first argument.",
                       "https://bugzilla.mozilla.org/show_bug.cgi?id=1238180");
    documentURL = documentURL.location;
  }

  let args = { initialTab, imageElement, frameOuterWindowID, browser };
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

/**
 * Sets the URI to display in the location bar.
 *
 * @param aURI [optional]
 *        nsIURI to set. If this is unspecified, the current URI will be used.
 */
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
    if (isInitialPage(uri.spec) &&
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

    valid = !isBlankPageURL(uri.spec) || uri.schemeIs("moz-extension");
  } else if (isInitialPage(value) &&
             checkEmptyPageOrigin(gBrowser.selectedBrowser)) {
    value = "";
    valid = true;
  }

  let isDifferentValidValue = valid && value != gURLBar.value;
  gURLBar.value = value;
  gURLBar.valueIsTyped = !valid;
  gURLBar.removeAttribute("usertyping");
  if (isDifferentValidValue) {
    gURLBar.selectionStart = gURLBar.selectionEnd = 0;
  }

  SetPageProxyState(valid ? "valid" : "invalid");
}

function losslessDecodeURI(aURI) {
  let scheme = aURI.scheme;
  if (scheme == "moz-action")
    throw new Error("losslessDecodeURI should never get a moz-action URI");

  var value = aURI.displaySpec;

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
                  // 2. Re-encode select whitespace so that it doesn't get eaten
                  //    away by the location bar (bug 410726). Re-encode all
                  //    adjacent whitespace, to prevent spoofing attempts where
                  //    invisible characters would push part of the URL to
                  //    overflow the location bar (bug 1395508).
                  .replace(/%(?!3B|2F|3F|3A|40|26|3D|2B|24|2C|23)|[\r\n\t]|\s(?=\s)|\s$/ig,
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

function UpdateUrlbarSearchSplitterState() {
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
    splitter.remove();
}

function UpdatePageProxyState() {
  if (gURLBar && gURLBar.value != gLastValidURLStr)
    SetPageProxyState("invalid");
}

/**
 * Updates the user interface to indicate whether the URI in the location bar is
 * different than the loaded page, because it's being edited or because a search
 * result is currently selected and is displayed in the location bar.
 *
 * @param aState
 *        The string "valid" indicates that the security indicators and other
 *        related user interface elments should be shown because the URI in the
 *        location bar matches the loaded page. The string "invalid" indicates
 *        that the URI in the location bar is different than the loaded page.
 */
function SetPageProxyState(aState) {
  if (!gURLBar)
    return;

  let oldPageProxyState = gURLBar.getAttribute("pageproxystate");
  // The "browser_urlbar_stop_pending.js" test uses a MutationObserver to do
  // some verifications at this point, and it breaks if we don't write the
  // attribute, even if it hasn't changed (bug 1338115).
  gURLBar.setAttribute("pageproxystate", aState);

  // the page proxy state is set to valid via OnLocationChange, which
  // gets called when we switch tabs.
  if (aState == "valid") {
    gLastValidURLStr = gURLBar.value;
    gURLBar.addEventListener("input", UpdatePageProxyState);
  } else if (aState == "invalid") {
    gURLBar.removeEventListener("input", UpdatePageProxyState);
  }

  // After we've ensured that we've applied the listeners and updated the value
  // of gLastValidURLStr, return early if the actual state hasn't changed.
  if (oldPageProxyState == aState) {
    return;
  }

  UpdatePopupNotificationsVisibility();
}

function UpdatePopupNotificationsVisibility() {
  // Only need to do something if the PopupNotifications object for this window
  // has already been initialized (i.e. its getter no longer exists).
  if (Object.getOwnPropertyDescriptor(window, "PopupNotifications").get) {
    return;
  }

  // Notify PopupNotifications that the visible anchors may have changed. This
  // also checks the suppression state according to the "shouldSuppress"
  // function defined earlier in this file.
  PopupNotifications.anchorVisibilityChange();
}

function PageProxyClickHandler(aEvent) {
  if (aEvent.button == 1 && Services.prefs.getBoolPref("middlemouse.paste"))
    middleMousePaste(aEvent);
}

// Values for telemtery bins: see TLS_ERROR_REPORT_UI in Histograms.json
const TLS_ERROR_REPORT_TELEMETRY_AUTO_CHECKED   = 2;
const TLS_ERROR_REPORT_TELEMETRY_AUTO_UNCHECKED = 3;

const PREF_SSL_IMPACT_ROOTS = ["security.tls.version.", "security.ssl3."];

/**
 * Handle command events bubbling up from error page content
 * or from about:newtab or from remote error pages that invoke
 * us via async messaging.
 */
var BrowserOnClick = {
  init() {
    let mm = window.messageManager;
    mm.addMessageListener("Browser:CertExceptionError", this);
    mm.addMessageListener("Browser:OpenCaptivePortalPage", this);
    mm.addMessageListener("Browser:SiteBlockedError", this);
    mm.addMessageListener("Browser:EnableOnlineMode", this);
    mm.addMessageListener("Browser:SetSSLErrorReportAuto", this);
    mm.addMessageListener("Browser:ResetSSLPreferences", this);
    mm.addMessageListener("Browser:SSLErrorReportTelemetry", this);
    mm.addMessageListener("Browser:SSLErrorGoBack", this);

    Services.obs.addObserver(this, "captive-portal-login-abort");
    Services.obs.addObserver(this, "captive-portal-login-success");
  },

  uninit() {
    let mm = window.messageManager;
    mm.removeMessageListener("Browser:CertExceptionError", this);
    mm.removeMessageListener("Browser:SiteBlockedError", this);
    mm.removeMessageListener("Browser:EnableOnlineMode", this);
    mm.removeMessageListener("Browser:SetSSLErrorReportAuto", this);
    mm.removeMessageListener("Browser:ResetSSLPreferences", this);
    mm.removeMessageListener("Browser:SSLErrorReportTelemetry", this);
    mm.removeMessageListener("Browser:SSLErrorGoBack", this);

    Services.obs.removeObserver(this, "captive-portal-login-abort");
    Services.obs.removeObserver(this, "captive-portal-login-success");
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "captive-portal-login-abort":
      case "captive-portal-login-success":
        // Broadcast when a captive portal is freed so that error pages
        // can refresh themselves.
        window.messageManager.broadcastAsyncMessage("Browser:CaptivePortalFreed");
      break;
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Browser:CertExceptionError":
        this.onCertError(msg.target, msg.data.elementId,
                         msg.data.isTopFrame, msg.data.location,
                         msg.data.securityInfoAsString,
                         msg.data.frameId);
      break;
      case "Browser:OpenCaptivePortalPage":
        CaptivePortalWatcher.ensureCaptivePortalTab();
      break;
      case "Browser:SiteBlockedError":
        this.onAboutBlocked(msg.data.elementId, msg.data.reason,
                            msg.data.isTopFrame, msg.data.location,
                            msg.data.blockedInfo);
      break;
      case "Browser:EnableOnlineMode":
        if (Services.io.offline) {
          // Reset network state and refresh the page.
          Services.io.offline = false;
          msg.target.reload();
        }
      break;
      case "Browser:ResetSSLPreferences":
        let prefSSLImpact = PREF_SSL_IMPACT_ROOTS.reduce((prefs, root) => {
                return prefs.concat(Services.prefs.getChildList(root));
        }, []);
        for (let prefName of prefSSLImpact) {
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
      case "Browser:SSLErrorGoBack":
        goBackFromErrorPage();
      break;
    }
  },

  onCertError(browser, elementId, isTopFrame, location, securityInfoAsString, frameId) {
    let secHistogram = Services.telemetry.getHistogramById("SECURITY_UI");
    let securityInfo;
    let sslStatus;

    switch (elementId) {
      case "exceptionDialogButton":
        if (isTopFrame) {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_BAD_CERT_TOP_CLICK_ADD_EXCEPTION);
        }

        securityInfo = getSecurityInfo(securityInfoAsString);
        sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                .SSLStatus;
        let params = { exceptionAdded: false,
                       sslStatus };

        try {
          switch (Services.prefs.getIntPref("browser.ssl_override_behavior")) {
            case 2 : // Pre-fetch & pre-populate
              params.prefetchCert = true;
            case 1 : // Pre-populate
              params.location = location;
          }
        } catch (e) {
          Cu.reportError("Couldn't get ssl_override pref: " + e);
        }

        window.openDialog("chrome://pippki/content/exceptionDialog.xul",
                          "", "chrome,centerscreen,modal", params);

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
        sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                .SSLStatus;
        let errorInfo = getDetailedCertErrorInfo(location,
                                                 securityInfo);
        let validityInfo = {
          notAfter: sslStatus.serverCert.validity.notAfter,
          notBefore: sslStatus.serverCert.validity.notBefore,
          notAfterLocalTime: sslStatus.serverCert.validity.notAfterLocalTime,
          notBeforeLocalTime: sslStatus.serverCert.validity.notBeforeLocalTime,
        };
        browser.messageManager.sendAsyncMessage("CertErrorDetails", {
            code: securityInfo.errorCode,
            info: errorInfo,
            codeString: securityInfo.errorCodeString,
            certIsUntrusted: sslStatus.isUntrusted,
            certSubjectAltNames: sslStatus.serverCert.subjectAltNames,
            validity: validityInfo,
            url: location,
            isDomainMismatch: sslStatus.isDomainMismatch,
            isNotValidAtThisTime: sslStatus.isNotValidAtThisTime,
            frameId
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

  onAboutBlocked(elementId, reason, isTopFrame, location, blockedInfo) {
    // Depending on what page we are displaying here (malware/phishing/unwanted)
    // use the right strings and links for each.
    let bucketName = "";
    let sendTelemetry = false;
    if (reason === "malware") {
      sendTelemetry = true;
      bucketName = "WARNING_MALWARE_PAGE_";
    } else if (reason === "phishing") {
      sendTelemetry = true;
      bucketName = "WARNING_PHISHING_PAGE_";
    } else if (reason === "unwanted") {
      sendTelemetry = true;
      bucketName = "WARNING_UNWANTED_PAGE_";
    } else if (reason === "harmful") {
      sendTelemetry = true;
      bucketName = "WARNING_HARMFUL_PAGE_";
    }
    let secHistogram = Services.telemetry.getHistogramById("URLCLASSIFIER_UI_EVENTS");
    let nsISecTel = Ci.IUrlClassifierUITelemetry;
    bucketName += isTopFrame ? "TOP_" : "FRAME_";

    switch (elementId) {
      case "goBackButton":
        if (sendTelemetry) {
          secHistogram.add(nsISecTel[bucketName + "GET_ME_OUT_OF_HERE"]);
        }
        getMeOutOfHere();
        break;
      case "ignore_warning_link":
        if (Services.prefs.getBoolPref("browser.safebrowsing.allowOverride")) {
          if (sendTelemetry) {
            secHistogram.add(nsISecTel[bucketName + "IGNORE_WARNING"]);
          }
          this.ignoreWarningLink(reason, blockedInfo);
        }
        break;
    }
  },

  ignoreWarningLink(reason, blockedInfo) {
    // Allow users to override and continue through to the site,
    // but add a notify bar as a reminder, so that they don't lose
    // track after, e.g., tab switching.
    gBrowser.loadURI(gBrowser.currentURI.spec, {
      flags: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
    });

    Services.perms.add(gBrowser.currentURI, "safe-browsing",
                       Ci.nsIPermissionManager.ALLOW_ACTION,
                       Ci.nsIPermissionManager.EXPIRE_SESSION);

    let buttons = [{
      label: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.label"),
      accessKey: gNavigatorBundle.getString("safebrowsing.getMeOutOfHereButton.accessKey"),
      callback() { getMeOutOfHere(); }
    }];

    let title;
    if (reason === "malware") {
      let reportUrl = gSafeBrowsing.getReportURL("MalwareMistake", blockedInfo);
      title = gNavigatorBundle.getString("safebrowsing.reportedAttackSite");
      // There's no button if we can not get report url, for example if the provider
      // of blockedInfo is not Google
      if (reportUrl) {
        buttons[1] = {
          label: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.label"),
          accessKey: gNavigatorBundle.getString("safebrowsing.notAnAttackButton.accessKey"),
          callback() {
            openTrustedLinkIn(reportUrl, "tab");
          }
        };
      }
    } else if (reason === "phishing") {
      let reportUrl = gSafeBrowsing.getReportURL("PhishMistake", blockedInfo);
      title = gNavigatorBundle.getString("safebrowsing.deceptiveSite");
      // There's no button if we can not get report url, for example if the provider
      // of blockedInfo is not Google
      if (reportUrl) {
        buttons[1] = {
          label: gNavigatorBundle.getString("safebrowsing.notADeceptiveSiteButton.label"),
          accessKey: gNavigatorBundle.getString("safebrowsing.notADeceptiveSiteButton.accessKey"),
          callback() {
            openTrustedLinkIn(reportUrl, "tab");
          }
        };
      }
    } else if (reason === "unwanted") {
      title = gNavigatorBundle.getString("safebrowsing.reportedUnwantedSite");
      // There is no button for reporting errors since Google doesn't currently
      // provide a URL endpoint for these reports.
    } else if (reason === "harmful") {
      title = gNavigatorBundle.getString("safebrowsing.reportedHarmfulSite");
      // There is no button for reporting errors since Google doesn't currently
      // provide a URL endpoint for these reports.
    }

    SafeBrowsingNotificationBox.show(title, buttons);
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
  let state = JSON.parse(SessionStore.getTabState(gBrowser.selectedTab));
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
  if (PrivateBrowsingUtils.isWindowPrivate(window))
    return url;
  try {
    url = prefs.getComplexValue("browser.startup.homepage",
                                Ci.nsIPrefLocalizedString).data;
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.includes("|"))
      url = url.split("|")[0];
  } catch (e) {
    Cu.reportError("Couldn't get homepage pref: " + e);
  }
  return url;
}

function BrowserFullScreen() {
  window.fullScreen = !window.fullScreen;
}

function getWebNavigation() {
  return gBrowser.webNavigation;
}

function BrowserReloadWithFlags(reloadFlags) {
  let url = gBrowser.currentURI.spec;
  if (gBrowser.updateBrowserRemotenessByURL(gBrowser.selectedBrowser, url)) {
    // If the remoteness has changed, the new browser doesn't have any
    // information of what was loaded before, so we need to load the previous
    // URL again.
    gBrowser.loadURI(url, { flags: reloadFlags });
    return;
  }

  // Do this after the above case where we might flip remoteness.
  // Unfortunately, we'll count the remoteness flip case as a
  // "newURL" load, since we're using loadURI, but hopefully
  // that's rare enough to not matter.
  maybeRecordAbandonmentTelemetry(gBrowser.selectedTab, "reload");

  // Reset temporary permissions on the current tab. This is done here
  // because we only want to reset permissions on user reload.
  SitePermissions.clearTemporaryPermissions(gBrowser.selectedBrowser);

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
  // private. Thus we must give isSecureURI correct flags or we
  // might get incorrect results.
  let flags = PrivateBrowsingUtils.isWindowPrivate(window) ?
              Ci.nsISocketProvider.NO_PERMANENT_STORAGE : 0;

  let uri = Services.io.newURI(location);

  let hasHSTS = sss.isSecureURI(sss.HEADER_HSTS, uri, flags);
  let hasHPKP = sss.isSecureURI(sss.HEADER_HPKP, uri, flags);
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
function getDERString(cert) {
  var length = {};
  var derArray = cert.getRawDER(length);
  var derString = "";
  for (var i = 0; i < derArray.length; i++) {
    derString += String.fromCharCode(derArray[i]);
  }
  return derString;
}

function getPEMString(cert) {
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
  _simplifiedPrintPreviewTab: null,
  _tabBeforePrintPreview: null,
  _simplifyPageTab: null,
  _lastRequestedPrintPreviewTab: null,

  _createPPBrowser() {
    let browser = this.getSourceBrowser();
    let preferredRemoteType = browser.remoteType;
    return gBrowser.loadOneTab("about:printpreview", {
      inBackground: true,
      preferredRemoteType,
      sameProcessAsFrameLoader: browser.frameLoader,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  },
  getPrintPreviewBrowser() {
    if (!this._printPreviewTab) {
      this._printPreviewTab = this._createPPBrowser();
    }
    gBrowser._allowTabChange = true;
    this._lastRequestedPrintPreviewTab = gBrowser.selectedTab = this._printPreviewTab;
    gBrowser._allowTabChange = false;
    return gBrowser.getBrowserForTab(this._printPreviewTab);
  },
  getSimplifiedPrintPreviewBrowser() {
    if (!this._simplifiedPrintPreviewTab) {
      this._simplifiedPrintPreviewTab = this._createPPBrowser();
    }
    gBrowser._allowTabChange = true;
    this._lastRequestedPrintPreviewTab = gBrowser.selectedTab = this._simplifiedPrintPreviewTab;
    gBrowser._allowTabChange = false;
    return gBrowser.getBrowserForTab(this._simplifiedPrintPreviewTab);
  },
  createSimplifiedBrowser() {
    let browser = this.getSourceBrowser();
    this._simplifyPageTab = gBrowser.loadOneTab("about:printpreview", {
      inBackground: true,
      sameProcessAsFrameLoader: browser.frameLoader,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
     });
    return this.getSimplifiedSourceBrowser();
  },
  getSourceBrowser() {
    if (!this._tabBeforePrintPreview) {
      this._tabBeforePrintPreview = gBrowser.selectedTab;
    }
    return this._tabBeforePrintPreview.linkedBrowser;
  },
  getSimplifiedSourceBrowser() {
    return this._simplifyPageTab ?
      gBrowser.getBrowserForTab(this._simplifyPageTab) : null;
  },
  getNavToolbox() {
    return gNavToolbox;
  },
  onEnter() {
    // We might have accidentally switched tabs since the user invoked print
    // preview
    if (gBrowser.selectedTab != this._lastRequestedPrintPreviewTab) {
      gBrowser.selectedTab = this._lastRequestedPrintPreviewTab;
    }
    gInPrintPreviewMode = true;
    this._toggleAffectedChrome();
  },
  onExit() {
    gBrowser._allowTabChange = true;
    gBrowser.selectedTab = this._tabBeforePrintPreview;
    gBrowser._allowTabChange = false;
    this._tabBeforePrintPreview = null;
    gInPrintPreviewMode = false;
    this._toggleAffectedChrome();
    let tabsToRemove = ["_simplifyPageTab", "_printPreviewTab", "_simplifiedPrintPreviewTab"];
    for (let tabProp of tabsToRemove) {
      if (this[tabProp]) {
        gBrowser.removeTab(this[tabProp]);
        this[tabProp] = null;
      }
    }
    gBrowser.deactivatePrintPreviewBrowsers();
    this._lastRequestedPrintPreviewTab = null;
  },
  _toggleAffectedChrome() {
    gNavToolbox.collapsed = gInPrintPreviewMode;

    if (gInPrintPreviewMode)
      this._hideChrome();
    else
      this._showChrome();

    TabsInTitlebar.allowedBy("print-preview", !gInPrintPreviewMode);
  },
  _hideChrome() {
    this._chromeState = {};

    this._chromeState.sidebarOpen = SidebarUI.isOpen;
    this._sidebarCommand = SidebarUI.currentID;
    SidebarUI.hide();

    var notificationBox = gBrowser.getNotificationBox();
    this._chromeState.notificationsOpen = !notificationBox.notificationsHidden;
    notificationBox.notificationsHidden = true;

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
  _showChrome() {
    if (this._chromeState.notificationsOpen)
      gBrowser.getNotificationBox().notificationsHidden = false;

    if (this._chromeState.findOpen)
      gLazyFindCommand("open");

    if (this._chromeState.globalNotificationsOpen)
      document.getElementById("global-notificationbox").notificationsHidden = false;

    if (this._chromeState.syncNotificationsOpen)
      document.getElementById("sync-notifications").notificationsHidden = false;

    if (this._chromeState.sidebarOpen)
      SidebarUI.show(this._sidebarCommand);
  },

  activateBrowser(browser) {
    gBrowser.activateBrowserForPrintPreview(browser);
  },
};

var browserDragAndDrop = {
  canDropLink: aEvent => Services.droppedLinkHandler.canDropLink(aEvent, true),

  dragOver(aEvent) {
    if (this.canDropLink(aEvent)) {
      aEvent.preventDefault();
    }
  },

  getTriggeringPrincipal(aEvent) {
    return Services.droppedLinkHandler.getTriggeringPrincipal(aEvent);
  },

  validateURIsForDrop(aEvent, aURIsCount, aURIs) {
    return Services.droppedLinkHandler.validateURIsForDrop(aEvent,
                                                           aURIsCount,
                                                           aURIs);
  },

  dropLinks(aEvent, aDisallowInherit) {
    return Services.droppedLinkHandler.dropLinks(aEvent, aDisallowInherit);
  }
};

var homeButtonObserver = {
  onDrop(aEvent) {
      // disallow setting home pages that inherit the principal
      let links = browserDragAndDrop.dropLinks(aEvent, true);
      if (links.length) {
        let urls = [];
        for (let link of links) {
          if (link.url.includes("|")) {
            urls.push(...link.url.split("|"));
          } else {
            urls.push(link.url);
          }
        }

        try {
          browserDragAndDrop.validateURIsForDrop(aEvent, urls.length, urls);
        } catch (e) {
          return;
        }

        setTimeout(openHomeDialog, 0, urls.join("|"));
      }
    },

  onDragOver(aEvent) {
      if (Services.prefs.prefIsLocked("browser.startup.homepage")) {
        return;
      }
      browserDragAndDrop.dragOver(aEvent);
      aEvent.dropEffect = "link";
    },
  onDragExit(aEvent) {
    }
};

function openHomeDialog(aURL) {
  var promptTitle = gNavigatorBundle.getString("droponhometitle");
  var promptMsg;
  if (aURL.includes("|")) {
    promptMsg = gNavigatorBundle.getString("droponhomemsgMultiple");
  } else {
    promptMsg = gNavigatorBundle.getString("droponhomemsg");
  }

  var pressedVal  = Services.prompt.confirmEx(window, promptTitle, promptMsg,
                          Services.prompt.STD_YES_NO_BUTTONS,
                          null, null, null, null, {value: 0});

  if (pressedVal == 0) {
    try {
      Services.prefs.setStringPref("browser.startup.homepage", aURL);
    } catch (ex) {
      dump("Failed to set the home page.\n" + ex + "\n");
    }
  }
}

var newTabButtonObserver = {
  onDragOver(aEvent) {
    browserDragAndDrop.dragOver(aEvent);
  },
  onDragExit(aEvent) {},
  async onDrop(aEvent) {
    let shiftKey = aEvent.shiftKey;
    let links = browserDragAndDrop.dropLinks(aEvent);
    let triggeringPrincipal = browserDragAndDrop.getTriggeringPrincipal(aEvent);

    if (links.length >= Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")) {
      // Sync dialog cannot be used inside drop event handler.
      let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(links.length,
                                                                  window);
      if (!answer) {
        return;
      }
    }

    for (let link of links) {
      if (link.url) {
        let data = await getShortcutOrURIAndPostData(link.url);
        // Allow third-party services to fixup this URL.
        openNewTabWith(data.url, shiftKey, {
          postData: data.postData,
          allowThirdPartyFixup: true,
          triggeringPrincipal,
        });
      }
    }
  }
};

var newWindowButtonObserver = {
  onDragOver(aEvent) {
    browserDragAndDrop.dragOver(aEvent);
  },
  onDragExit(aEvent) {},
  async onDrop(aEvent) {
    let links = browserDragAndDrop.dropLinks(aEvent);
    let triggeringPrincipal = browserDragAndDrop.getTriggeringPrincipal(aEvent);

    if (links.length >= Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")) {
      // Sync dialog cannot be used inside drop event handler.
      let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(links.length,
                                                                  window);
      if (!answer) {
        return;
      }
    }

    for (let link of links) {
      if (link.url) {
        let data = await getShortcutOrURIAndPostData(link.url);
        // Allow third-party services to fixup this URL.
        openNewWindowWith(data.url, {
          postData: data.postData,
          allowThirdPartyFixup: true,
          triggeringPrincipal,
        });
      }
    }
  }
};
const DOMEventHandler = {
  init() {
    let mm = window.messageManager;
    mm.addMessageListener("Link:AddFeed", this);
    mm.addMessageListener("Link:SetIcon", this);
    mm.addMessageListener("Link:AddSearch", this);
    mm.addMessageListener("Meta:SetPageInfo", this);
  },

  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "Link:AddFeed":
        let link = {type: aMsg.data.type, href: aMsg.data.href, title: aMsg.data.title};
        FeedHandler.addFeed(link, aMsg.target);
        break;

      case "Link:SetIcon":
        this.setIcon(aMsg.target, aMsg.data.url, aMsg.data.loadingPrincipal,
                     aMsg.data.requestContextID, aMsg.data.canUseForTab);
        break;

      case "Link:AddSearch":
        this.addSearch(aMsg.target, aMsg.data.engine, aMsg.data.url);
        break;

      case "Meta:SetPageInfo":
        this.setPageInfo(aMsg.data);
        break;
    }
  },

  setPageInfo(aData) {
    const {url, description, previewImageURL} = aData;
    gBrowser.setPageInfo(url, description, previewImageURL);
    return true;
  },

  setIcon(aBrowser, aURL, aLoadingPrincipal, aRequestContextID = 0, aCanUseForTab = true) {
    if (gBrowser.isFailedIcon(aURL))
      return false;

    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab)
      return false;

    let loadingPrincipal = aLoadingPrincipal ||
                           Services.scriptSecurityManager.getSystemPrincipal();
    if (aURL) {
      gBrowser.storeIcon(aBrowser, aURL, loadingPrincipal, aRequestContextID);
    }
    if (aCanUseForTab) {
      gBrowser.setIcon(tab, aURL, loadingPrincipal, aRequestContextID);
    }
    return true;
  },

  addSearch(aBrowser, aEngine, aURL) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab)
      return;

    BrowserSearch.addEngine(aBrowser, aEngine, makeURI(aURL));
  },
};

const BrowserSearch = {
  init() {
    Services.obs.addObserver(this, "browser-search-engine-modified");
  },

  uninit() {
    Services.obs.removeObserver(this, "browser-search-engine-modified");
  },

  observe(engine, topic, data) {
    // There are two kinds of search engine objects, nsISearchEngine objects and
    // plain { uri, title, icon } objects.  `engine` in this method is the
    // former.  The browser.engines and browser.hiddenEngines arrays are the
    // latter, and they're the engines offered by the the page in the browser.
    //
    // The two types of engines are currently related by their titles/names,
    // although that may change; see bug 335102.
    let engineName = engine.wrappedJSObject.name;
    switch (data) {
    case "engine-removed":
      // An engine was removed from the search service.  If a page is offering
      // the engine, then the engine needs to be added back to the corresponding
      // browser's offered engines.
      this._addMaybeOfferedEngine(engineName);
      break;
    case "engine-added":
      // An engine was added to the search service.  If a page is offering the
      // engine, then the engine needs to be removed from the corresponding
      // browser's offered engines.
      this._removeMaybeOfferedEngine(engineName);
      break;
    }
  },

  _addMaybeOfferedEngine(engineName) {
    let selectedBrowserOffersEngine = false;
    for (let browser of gBrowser.browsers) {
      for (let i = 0; i < (browser.hiddenEngines || []).length; i++) {
        if (browser.hiddenEngines[i].title == engineName) {
          if (!browser.engines) {
            browser.engines = [];
          }
          browser.engines.push(browser.hiddenEngines[i]);
          browser.hiddenEngines.splice(i, 1);
          if (browser == gBrowser.selectedBrowser) {
            selectedBrowserOffersEngine = true;
          }
          break;
        }
      }
    }
    if (selectedBrowserOffersEngine) {
      this.updateOpenSearchBadge();
    }
  },

  _removeMaybeOfferedEngine(engineName) {
    let selectedBrowserOffersEngine = false;
    for (let browser of gBrowser.browsers) {
      for (let i = 0; i < (browser.engines || []).length; i++) {
        if (browser.engines[i].title == engineName) {
          if (!browser.hiddenEngines) {
            browser.hiddenEngines = [];
          }
          browser.hiddenEngines.push(browser.engines[i]);
          browser.engines.splice(i, 1);
          if (browser == gBrowser.selectedBrowser) {
            selectedBrowserOffersEngine = true;
          }
          break;
        }
      }
    }
    if (selectedBrowserOffersEngine) {
      this.updateOpenSearchBadge();
    }
  },

  addEngine(browser, engine, uri) {
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
  updateOpenSearchBadge() {
    BrowserPageActions.addSearchEngine.updateEngines();

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
        var observer = function(subject, topic, data) {
          if (subject == win) {
            BrowserSearch.webSearch();
            Services.obs.removeObserver(observer, "browser-delayed-startup-finished");
          }
        };
        win = window.openDialog(getBrowserURL(), "_blank",
                                "chrome,all,dialog=no", "about:blank");
        Services.obs.addObserver(observer, "browser-delayed-startup-finished");
      }
      return;
    }

    let focusUrlBarIfSearchFieldIsNotActive = function(aSearchBar) {
      if (!aSearchBar || document.activeElement != aSearchBar.textbox.inputField) {
        focusAndSelectUrlBar(true);
      }
    };

    let searchBar = this.searchBar;
    let placement = CustomizableUI.getPlacementOfWidget("search-container");
    let focusSearchBar = () => {
      searchBar = this.searchBar;
      searchBar.select();
      focusUrlBarIfSearchFieldIsNotActive(searchBar);
    };
    if (placement && searchBar &&
        ((searchBar.parentNode.getAttribute("overflowedItem") == "true" &&
          placement.area == CustomizableUI.AREA_NAVBAR) ||
         placement.area == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL)) {
      let navBar = document.getElementById(CustomizableUI.AREA_NAVBAR);
      navBar.overflowable.show().then(focusSearchBar);
      return;
    }
    if (searchBar) {
      if (window.fullScreen)
        FullScreen.showNavToolbox();
      searchBar.select();
    }
    focusUrlBarIfSearchFieldIsNotActive(searchBar);
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
  _loadSearch(searchText, useNewTab, purpose) {
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
                 inBackground,
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
  loadSearchFromContext(terms) {
    let engine = BrowserSearch._loadSearch(terms, true, "contextmenu");
    if (engine) {
      BrowserSearch.recordSearchInTelemetry(engine, "contextmenu");
    }
  },

  pasteAndSearch(event) {
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

  get searchEnginesURL() {
    return formatURL("browser.search.searchEnginesURL", true);
  },

  loadAddEngines: function BrowserSearch_loadAddEngines() {
    var newWindowPref = Services.prefs.getIntPref("browser.link.open_newwindow");
    var where = newWindowPref == 3 ? "tab" : "window";
    openTrustedLinkIn(this.searchEnginesURL, where);
  },

  _getSearchEngineId(engine) {
    if (engine && engine.identifier) {
      return engine.identifier;
    }

    if (!engine || (engine.name === undefined))
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
   *        (string) Where the search originated from. See BrowserUsageTelemetry for
   *        allowed values.
   * @param details [optional]
   *        An optional parameter passed to |BrowserUsageTelemetry.recordSearch|.
   *        See its documentation for allowed options.
   *        Additionally, if the search was a suggested search, |details.selection|
   *        indicates where the item was in the suggestion list and how the user
   *        selected it: {selection: {index: The selected index, kind: "key" or "mouse"}}
   */
  recordSearchInTelemetry(engine, source, details = {}) {
    BrowserUITelemetry.countSearchEvent(source, null, details.selection);
    try {
      BrowserUsageTelemetry.recordSearch(engine, source, details);
    } catch (ex) {
      Cu.reportError(ex);
    }
  },

  /**
   * Helper to record a one-off search with Telemetry.
   *
   * Telemetry records only search counts and nothing pertaining to the search itself.
   *
   * @param engine
   *        (nsISearchEngine) The engine handling the search.
   * @param source
   *        (string) Where the search originated from. See BrowserUsageTelemetry for
   *        allowed values.
   * @param type
   *        (string) Indicates how the user selected the search item.
   * @param where
   *        (string) Where was the search link opened (e.g. new tab, current tab, ..).
   */
  recordOneoffSearchInTelemetry(engine, source, type, where) {
    let id = this._getSearchEngineId(engine) + "." + source;
    BrowserUITelemetry.countOneoffSearchEvent(id, type, where);
    try {
      const details = {type, isOneOff: true};
      BrowserUsageTelemetry.recordSearch(engine, source, details);
    } catch (ex) {
      Cu.reportError(ex);
    }
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
    });
    aParent.addEventListener("DOMMenuItemInactive", function() {
      XULBrowserWindow.setOverLink("");
    });

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

  function updateSessionHistory(sessionHistory, initial) {
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

      item.setAttribute("uri", uri);
      item.setAttribute("label", entry.title || uri);
      item.setAttribute("index", j);

      // Cache this so that gotoHistoryIndex doesn't need the original index
      item.setAttribute("historyindex", j - index);

      if (j != index) {
        // Use list-style-image rather than the image attribute in order to
        // allow CSS to override this.
        item.style.listStyleImage = `url(page-icon:${uri})`;
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
      !/[\x00-\x1F]/.test(aUrlToAdd)) // eslint-disable-line no-control-regex
    PlacesUIUtils.markPageAsTyped(aUrlToAdd);
}

function BrowserDownloadsUI() {
  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    openTrustedLinkIn("about:downloads", "tab");
  } else {
    PlacesCommandHook.showPlacesOrganizer("Downloads");
  }
}

function toOpenWindowByType(inType, uri, features) {
  var topWindow = Services.wm.getMostRecentWindow(inType);

  if (topWindow)
    topWindow.focus();
  else if (features)
    window.open(uri, "_blank", features);
  else
    window.open(uri, "_blank", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}

function OpenBrowserWindow(options) {
  var telemetryObj = {};
  TelemetryStopwatch.start("FX_NEW_WINDOW_MS", telemetryObj);

  var handler = Cc["@mozilla.org/browser/clh;1"]
                  .getService(Ci.nsIBrowserHandler);
  var defaultArgs = handler.defaultArgs;
  var wintype = document.documentElement.getAttribute("windowtype");

  var extraFeatures = "";
  if (options && options.private && PrivateBrowsingUtils.enabled) {
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

  // If the window is maximized, we want to skip the animation, since we're
  // going to be taking up most of the screen anyways, and we want to optimize
  // for showing the user a useful window as soon as possible.
  if (window.windowState == window.STATE_MAXIMIZED) {
    extraFeatures += ",suppressanimation";
  }

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  var win;
  if (window && (wintype == "navigator:browser") && window.content && window.content.document) {
    var DocCharset = window.content.document.characterSet;
    let charsetArg = "charset=" + DocCharset;

    // we should "inherit" the charset menu setting in a new window
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no" + extraFeatures, defaultArgs, charsetArg);
  } else {
    // forget about the charset information.
    win = window.openDialog("chrome://browser/content/", "_blank", "chrome,all,dialog=no" + extraFeatures, defaultArgs);
  }

  win.addEventListener("MozAfterPaint", () => {
    TelemetryStopwatch.finish("FX_NEW_WINDOW_MS", telemetryObj);
    if (Services.prefs.getIntPref("browser.startup.page") == 1
        && defaultArgs == handler.startPage) {
      // A notification for when a user has triggered their homepage. This is used
      // to display a doorhanger explaining that an extension has modified the
      // homepage, if necessary.
      Services.obs.notifyObservers(win, "browser-open-homepage-start");
    }
  }, {once: true});

  return win;
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
function updateEditUIVisibility() {
  if (AppConstants.platform == "macosx")
    return;

  let editMenuPopupState = document.getElementById("menu_EditPopup").state;
  let contextMenuPopupState = document.getElementById("contentAreaContextMenu").state;
  let placesContextMenuPopupState = document.getElementById("placesContext").state;

  let oldVisible = gEditUIVisible;

  // The UI is visible if the Edit menu is opening or open, if the context menu
  // is open, or if the toolbar has been customized to include the Cut, Copy,
  // or Paste toolbar buttons.
  gEditUIVisible = editMenuPopupState == "showing" ||
                   editMenuPopupState == "open" ||
                   contextMenuPopupState == "showing" ||
                   contextMenuPopupState == "open" ||
                   placesContextMenuPopupState == "showing" ||
                   placesContextMenuPopupState == "open";
  const kOpenPopupStates = ["showing", "open"];
  if (!gEditUIVisible) {
    // Now check the edit-controls toolbar buttons.
    let placement = CustomizableUI.getPlacementOfWidget("edit-controls");
    let areaType = placement ? CustomizableUI.getAreaType(placement.area) : "";
    if (areaType == CustomizableUI.TYPE_MENU_PANEL) {
      let customizablePanel = PanelUI.overflowPanel;
      gEditUIVisible = kOpenPopupStates.includes(customizablePanel.state);
    } else if (areaType == CustomizableUI.TYPE_TOOLBAR && window.toolbar.visible) {
      // The edit controls are on a toolbar, so they are visible,
      // unless they're in a panel that isn't visible...
      if (placement.area == "nav-bar") {
        let editControls = document.getElementById("edit-controls");
        gEditUIVisible = !editControls.hasAttribute("overflowedItem") ||
                          kOpenPopupStates.includes(document.getElementById("widget-overflow").state);
      } else {
        gEditUIVisible = true;
      }
    }
  }

  // Now check the main menu panel
  if (!gEditUIVisible) {
    gEditUIVisible = kOpenPopupStates.includes(PanelUI.panel.state);
  }

  // No need to update commands if the edit UI visibility has not changed.
  if (gEditUIVisible == oldVisible) {
    return;
  }

  // If UI is visible, update the edit commands' enabled state to reflect
  // whether or not they are actually enabled for the current focus/selection.
  if (gEditUIVisible) {
    goUpdateGlobalEditMenuItems();
  } else {
    // Otherwise, enable all commands, so that keyboard shortcuts still work,
    // then lazily determine their actual enabled state when the user presses
    // a keyboard shortcut.
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
function openNewUserContextTab(event) {
  openTrustedLinkIn(BROWSER_NEW_TAB_URL, "tab", {
    userContextId: parseInt(event.target.getAttribute("data-usercontextid")),
  });
}

/**
 * Updates File Menu User Context UI visibility depending on
 * privacy.userContext.enabled pref state.
 */
function updateUserContextUIVisibility() {
  let menu = document.getElementById("menu_newUserContext");
  menu.hidden = !Services.prefs.getBoolPref("privacy.userContext.enabled");
  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    menu.setAttribute("disabled", "true");
  }
}

/**
 * Updates the User Context UI indicators if the browser is in a non-default context
 */
function updateUserContextUIIndicator() {
  let hbox = document.getElementById("userContext-icons");

  let userContextId = gBrowser.selectedBrowser.getAttribute("usercontextid");
  if (!userContextId) {
    hbox.setAttribute("data-identity-color", "");
    hbox.hidden = true;
    return;
  }

  let identity = ContextualIdentityService.getPublicIdentityFromId(userContextId);
  if (!identity) {
    hbox.setAttribute("data-identity-color", "");
    hbox.hidden = true;
    return;
  }

  hbox.setAttribute("data-identity-color", identity.color);

  let label = document.getElementById("userContext-label");
  label.setAttribute("value", ContextualIdentityService.getUserContextLabel(userContextId));

  let indicator = document.getElementById("userContext-indicator");
  indicator.setAttribute("data-identity-icon", identity.icon);

  hbox.hidden = false;
}

/**
 * Makes the Character Encoding menu enabled or disabled as appropriate.
 * To be called when the View menu or the app menu is opened.
 */
function updateCharacterEncodingMenuState() {
  let charsetMenu = document.getElementById("charsetMenu");
  // gBrowser is null on Mac when the menubar shows in the context of
  // non-browser windows. The above elements may be null depending on
  // what parts of the menubar are present. E.g. no app menu on Mac.
  if (gBrowser && gBrowser.selectedBrowser.mayEnableCharacterEncodingMenu) {
    if (charsetMenu) {
      charsetMenu.removeAttribute("disabled");
    }
  } else if (charsetMenu) {
    charsetMenu.setAttribute("disabled", "true");
  }
}

var XULBrowserWindow = {
  // Stored Status, Link and Loading values
  status: "",
  defaultStatus: "",
  overLink: "",
  startTime: 0,
  isBusy: false,
  busyUI: false,

  QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsIXULBrowserWindow) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  },

  get stopCommand() {
    delete this.stopCommand;
    return this.stopCommand = document.getElementById("Browser:Stop");
  },
  get reloadCommand() {
    delete this.reloadCommand;
    return this.reloadCommand = document.getElementById("Browser:Reload");
  },
  get isImage() {
    delete this.isImage;
    return this.isImage = document.getElementById("isImage");
  },
  get canViewSource() {
    delete this.canViewSource;
    return this.canViewSource = document.getElementById("canViewSource");
  },

  setJSStatus() {
    // unsupported
  },

  forceInitialBrowserRemote(aRemoteType) {
    gBrowser.updateBrowserRemoteness(gBrowser.initialBrowser, true, { remoteType: aRemoteType });
  },

  forceInitialBrowserNonRemote(aOpener) {
    gBrowser.updateBrowserRemoteness(gBrowser.initialBrowser, false, { opener: aOpener });
  },

  setDefaultStatus(status) {
    this.defaultStatus = status;
    StatusPanel.update();
  },

  setOverLink(url, anchorElt) {
    if (url) {
      url = Services.textToSubURI.unEscapeURIForUI("UTF-8", url);

      // Encode bidirectional formatting characters.
      // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
      url = url.replace(/[\u200e\u200f\u202a\u202b\u202c\u202d\u202e]/g,
                        encodeURIComponent);

      if (gURLBar && gURLBar._mayTrimURLs /* corresponds to browser.urlbar.trimURLs */)
        url = trimURL(url);
    }

    this.overLink = url;
    LinkTargetDisplay.update();
  },

  showTooltip(x, y, tooltip, direction, browser) {
    if (Cc["@mozilla.org/widget/dragservice;1"].getService(Ci.nsIDragService).
        getCurrentSession()) {
      return;
    }

    // The x,y coordinates are relative to the <browser> element using
    // the chrome zoom level.
    let elt = document.getElementById("remoteBrowserTooltip");
    elt.label = tooltip;
    elt.style.direction = direction;

    elt.openPopupAtScreen(browser.boxObject.screenX + x, browser.boxObject.screenY + y, false, null);
  },

  hideTooltip() {
    let elt = document.getElementById("remoteBrowserTooltip");
    elt.hidePopup();
  },

  getTabCount() {
    return gBrowser.tabs.length;
  },

  // Called before links are navigated to to allow us to retarget them if needed.
  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
    return BrowserUtils.onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab);
  },

  // Check whether this URI should load in the current process
  shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData, aTriggeringPrincipal) {
    if (!gMultiProcessBrowser)
      return true;

    let browser = aDocShell.QueryInterface(Ci.nsIDocShellTreeItem)
                           .sameTypeRootTreeItem
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler;

    // Ignore loads that aren't in the main tabbrowser
    if (browser.localName != "browser" || !browser.getTabBrowser || browser.getTabBrowser() != gBrowser)
      return true;

    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aReferrer, aHasPostData)) {
      // XXX: Do we want to complain if we have post data but are still
      // redirecting the load? Perhaps a telemetry probe? Theoretically we
      // shouldn't do this, as it throws out data. See bug 1348018.
      E10SUtils.redirectLoad(aDocShell, aURI, aReferrer, aTriggeringPrincipal, false);
      return false;
    }

    return true;
  },

  onProgressChange(aWebProgress, aRequest,
                             aCurSelfProgress, aMaxSelfProgress,
                             aCurTotalProgress, aMaxTotalProgress) {
    // Do nothing.
  },

  onProgressChange64(aWebProgress, aRequest,
                               aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress) {
    return this.onProgressChange(aWebProgress, aRequest,
      aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress,
      aMaxTotalProgress);
  },

  // This function fires only for the currently selected tab.
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
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
        this.busyUI = true;

        // XXX: This needs to be based on window activity...
        this.stopCommand.removeAttribute("disabled");
        CombinedStopReload.switchToStop(aRequest, aWebProgress);
      }
    } else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
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

          canViewSource = location.scheme != "view-source";

          if (location.spec != "about:blank") {
            switch (aStatus) {
              case Cr.NS_ERROR_NET_TIMEOUT:
                msg = gNavigatorBundle.getString("nv_timeout");
                break;
            }
          }
        }

        this.status = "";
        this.setDefaultStatus(msg);

        // Disable menu entries for images, enable otherwise
        if (browser.documentContentType && BrowserUtils.mimeTypeIsTextBased(browser.documentContentType)) {
          this.isImage.removeAttribute("disabled");
        } else {
          canViewSource = false;
          this.isImage.setAttribute("disabled", "true");
        }

        if (canViewSource) {
          this.canViewSource.removeAttribute("disabled");
        } else {
          this.canViewSource.setAttribute("disabled", "true");
        }
      }

      this.isBusy = false;

      if (this.busyUI) {
        this.busyUI = false;

        this.stopCommand.setAttribute("disabled", "true");
        CombinedStopReload.switchToReload(aRequest, aWebProgress);
      }
    }
  },

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    var location = aLocationURI ? aLocationURI.spec : "";

    let pageTooltip = document.getElementById("aHTMLTooltip");
    let tooltipNode = pageTooltip.triggerNode;
    if (tooltipNode) {
      // Optimise for the common case
      if (aWebProgress.isTopLevel) {
        pageTooltip.hidePopup();
      } else {
        for (let tooltipWindow = tooltipNode.ownerGlobal;
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
      this.isImage.removeAttribute("disabled");
    else
      this.isImage.setAttribute("disabled", "true");

    this.hideOverLinkImmediately = true;
    this.setOverLink("", null);
    this.hideOverLinkImmediately = false;

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation

    if (aWebProgress.isTopLevel) {
      if ((location == "about:blank" && checkEmptyPageOrigin()) ||
          location == "") { // Second condition is for new tabs, otherwise
                             // reload function is enabled until tab is refreshed.
        this.reloadCommand.setAttribute("disabled", "true");
      } else {
        this.reloadCommand.removeAttribute("disabled");
      }

      URLBarSetURI(aLocationURI);

      BookmarkingUI.onLocationChange();

      gIdentityHandler.onLocationChange();

      BrowserPageActions.onLocationChange();

      SafeBrowsingNotificationBox.onLocationChange(aLocationURI);

      gTabletModePageCounter.inc();

      // Utility functions for disabling find
      var shouldDisableFind = function(aDocument) {
        let docElt = aDocument.documentElement;
        return docElt && docElt.getAttribute("disablefastfind") == "true";
      };

      var disableFindCommands = function(aDisable) {
        let findCommands = [document.getElementById("cmd_find"),
                            document.getElementById("cmd_findAgain"),
                            document.getElementById("cmd_findPrevious")];
        for (let elt of findCommands) {
          if (aDisable)
            elt.setAttribute("disabled", "true");
          else
            elt.removeAttribute("disabled");
        }
      };

      var onContentRSChange = function(e) {
        if (e.target.readyState != "interactive" && e.target.readyState != "complete")
          return;

        e.target.removeEventListener("readystatechange", onContentRSChange);
        disableFindCommands(shouldDisableFind(e.target));
      };

      // Disable find commands in documents that ask for them to be disabled.
      if (!gMultiProcessBrowser && aLocationURI &&
          (aLocationURI.schemeIs("about") || aLocationURI.schemeIs("chrome"))) {
        // Don't need to re-enable/disable find commands for same-document location changes
        // (e.g. the replaceStates in about:addons)
        if (!(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) {
          if (window.content.document.readyState == "interactive" || window.content.document.readyState == "complete")
            disableFindCommands(shouldDisableFind(window.content.document));
          else {
            window.content.document.addEventListener("readystatechange", onContentRSChange);
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

    if (!gMultiProcessBrowser) // Bug 1108553 - Cannot rotate images with e10s
      gGestureSupport.restoreRotationState();

    // See bug 358202, when tabs are switched during a drag operation,
    // timers don't fire on windows (bug 203573)
    if (aRequest)
      setTimeout(function() { XULBrowserWindow.asyncUpdateUI(); }, 0);
    else
      this.asyncUpdateUI();

    if (AppConstants.MOZ_CRASHREPORTER && aLocationURI) {
      let uri = aLocationURI;
      try {
        // If the current URI contains a username/password, remove it.
        uri = aLocationURI.mutate()
                          .setUserPass("")
                          .finalize();
      } catch (ex) { /* Ignore failures on about: URIs. */ }

      try {
        gCrashReporter.annotateCrashReport("URL", uri.spec);
      } catch (ex) {
        // Don't make noise when the crash reporter is built but not enabled.
        if (ex.result != Cr.NS_ERROR_NOT_INITIALIZED) {
          throw ex;
        }
      }
    }
  },

  asyncUpdateUI() {
    FeedHandler.updateFeeds();
    BrowserSearch.updateOpenSearchBadge();
  },

  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    this.status = aMessage;
    StatusPanel.update();
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
  onSecurityChange(aWebProgress, aRequest, aState, aIsSimulated) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    let uri = gBrowser.currentURI;
    let spec = uri.spec;
    if (this._state == aState &&
        this._lastLocation == spec) {
      // Switching to a tab of the same URL doesn't change most security
      // information, but tab specific permissions may be different.
      gIdentityHandler.refreshIdentityBlock();
      return;
    }
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

    CombinedStopReload.onTabSwitch();

    var nsIWebProgressListener = Ci.nsIWebProgressListener;
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
  },

  navigateAndRestoreByIndex: function XWB_navigateAndRestoreByIndex(aBrowser, aIndex) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (tab) {
      SessionStore.navigateAndRestore(tab, {}, aIndex);
      return;
    }

    throw new Error("Trying to navigateAndRestore a browser which was " +
                    "not attached to this tabbrowser is unsupported");
  }
};

var LinkTargetDisplay = {
  get DELAY_SHOW() {
     delete this.DELAY_SHOW;
     return this.DELAY_SHOW = Services.prefs.getIntPref("browser.overlink-delay");
  },

  DELAY_HIDE: 250,
  _timer: 0,

  update() {
    clearTimeout(this._timer);
    window.removeEventListener("mousemove", this, true);

    if (!XULBrowserWindow.overLink) {
      if (XULBrowserWindow.hideOverLinkImmediately)
        this._hide();
      else
        this._timer = setTimeout(this._hide.bind(this), this.DELAY_HIDE);
      return;
    }

    if (StatusPanel.isVisible) {
      StatusPanel.update();
    } else {
      // Let the display appear when the mouse doesn't move within the delay
      this._showDelayed();
      window.addEventListener("mousemove", this, true);
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "mousemove":
        // Restart the delay since the mouse was moved
        clearTimeout(this._timer);
        this._showDelayed();
        break;
    }
  },

  _showDelayed() {
    this._timer = setTimeout(function(self) {
      StatusPanel.update();
      window.removeEventListener("mousemove", self, true);
    }, this.DELAY_SHOW, this);
  },

  _hide() {
    clearTimeout(this._timer);

    StatusPanel.update();
  }
};

var CombinedStopReload = {
  // Try to initialize. Returns whether initialization was successful, which
  // may mean we had already initialized.
  ensureInitialized() {
    if (this._initialized)
      return true;
    if (this._destroyed)
      return false;

    let reload = document.getElementById("reload-button");
    let stop = document.getElementById("stop-button");
    // It's possible the stop/reload buttons have been moved to the palette.
    // They may be reinserted later, so we will retry initialization if/when
    // we get notified of document loads.
    if (!stop || !reload)
      return false;

    this._initialized = true;
    if (XULBrowserWindow.stopCommand.getAttribute("disabled") != "true")
      reload.setAttribute("displaystop", "true");
    stop.addEventListener("click", this);

    // Removing attributes based on the observed command doesn't happen if the button
    // is in the palette when the command's attribute is removed (cf. bug 309953)
    for (let button of [stop, reload]) {
      if (button.hasAttribute("disabled")) {
        let command = document.getElementById(button.getAttribute("command"));
        if (!command.hasAttribute("disabled")) {
          button.removeAttribute("disabled");
        }
      }
    }

    this.reload = reload;
    this.stop = stop;
    this.stopReloadContainer = this.reload.parentNode;
    this.timeWhenSwitchedToStop = 0;

    if (this._shouldStartPrefMonitoring) {
      this.startAnimationPrefMonitoring();
    }
    return true;
  },

  uninit() {
    this._destroyed = true;

    if (!this._initialized)
      return;

    Services.prefs.removeObserver("toolkit.cosmeticAnimations.enabled", this);
    this._cancelTransition();
    this.stop.removeEventListener("click", this);
    this.stopReloadContainer.removeEventListener("animationend", this);
    this.stopReloadContainer = null;
    this.reload = null;
    this.stop = null;
  },

  handleEvent(event) {
    switch (event.type) {
      case "click":
        if (event.button == 0 &&
            !this.stop.disabled) {
          this._stopClicked = true;
        }
      case "animationend": {
        if (event.target.classList.contains("toolbarbutton-animatable-image") &&
            (event.animationName == "reload-to-stop" ||
             event.animationName == "stop-to-reload" ||
             event.animationName == "reload-to-stop-rtl" ||
             event.animationName == "stop-to-reload-rtl")) {
          this.stopReloadContainer.removeAttribute("animate");
        }
      }
    }
  },

  observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      this.animate = Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled");
    }
  },

  startAnimationPrefMonitoring() {
    // CombinedStopReload may have been uninitialized before the idleCallback is executed.
    if (this._destroyed)
      return;
    if (!this.ensureInitialized()) {
      this._shouldStartPrefMonitoring = true;
      return;
    }
    this.animate = Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled") &&
                   Services.prefs.getBoolPref("browser.stopReloadAnimation.enabled");
    Services.prefs.addObserver("toolkit.cosmeticAnimations.enabled", this);
    this.stopReloadContainer.addEventListener("animationend", this);
  },

  onTabSwitch() {
    // Reset the time in the event of a tabswitch since the stored time
    // would have been associated with the previous tab, so the animation will
    // still run if the page has been loading until long after the tab switch.
    this.timeWhenSwitchedToStop = window.performance.now();
  },

  switchToStop(aRequest, aWebProgress) {
    if (!this.ensureInitialized() || !this._shouldSwitch(aRequest, aWebProgress)) {
      return;
    }

    // Store the time that we switched to the stop button only if a request
    // is active. Requests are null if the switch is related to a tabswitch.
    // This is used to determine if we should show the stop->reload animation.
    if (aRequest instanceof Ci.nsIRequest) {
      this.timeWhenSwitchedToStop = window.performance.now();
    }

    let shouldAnimate = aRequest instanceof Ci.nsIRequest &&
                        aWebProgress.isTopLevel &&
                        aWebProgress.isLoadingDocument &&
                        !gBrowser.tabAnimationsInProgress &&
                        this.stopReloadContainer.closest("#nav-bar-customization-target") &&
                        this.animate;

    this._cancelTransition();
    if (shouldAnimate) {
      BrowserUtils.setToolbarButtonHeightProperty(this.stopReloadContainer);
      this.stopReloadContainer.setAttribute("animate", "true");
    } else {
      this.stopReloadContainer.removeAttribute("animate");
    }
    this.reload.setAttribute("displaystop", "true");
  },

  switchToReload(aRequest, aWebProgress) {
    if (!this.ensureInitialized() || !this.reload.hasAttribute("displaystop")) {
      return;
    }

    let shouldAnimate = aRequest instanceof Ci.nsIRequest &&
                        aWebProgress.isTopLevel &&
                        !aWebProgress.isLoadingDocument &&
                        !gBrowser.tabAnimationsInProgress &&
                        this._loadTimeExceedsMinimumForAnimation() &&
                        this.stopReloadContainer.closest("#nav-bar-customization-target") &&
                        this.animate;

    if (shouldAnimate) {
      BrowserUtils.setToolbarButtonHeightProperty(this.stopReloadContainer);
      this.stopReloadContainer.setAttribute("animate", "true");
    } else {
      this.stopReloadContainer.removeAttribute("animate");
    }

    this.reload.removeAttribute("displaystop");

    if (!shouldAnimate || this._stopClicked) {
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
    this._timer = setTimeout(function(self) {
      self._timer = 0;
      self.reload.disabled = XULBrowserWindow.reloadCommand
                                             .getAttribute("disabled") == "true";
    }, 650, this);
  },

  _loadTimeExceedsMinimumForAnimation() {
    // If the time between switching to the stop button then switching to
    // the reload button exceeds 150ms, then we will show the animation.
    // If we don't know when we switched to stop (switchToStop is called
    // after init but before switchToReload), then we will prevent the
    // animation from occuring.
    return this.timeWhenSwitchedToStop &&
           window.performance.now() - this.timeWhenSwitchedToStop > 150;
  },

  _shouldSwitch(aRequest, aWebProgress) {
    if (aRequest &&
        aRequest.originalURI &&
        (aRequest.originalURI.schemeIs("chrome") ||
         (aRequest.originalURI.schemeIs("about") &&
          aWebProgress.isTopLevel &&
          !aRequest.originalURI.spec.startsWith("about:reader")))) {
      return false;
    }

    return true;
  },

  _cancelTransition() {
    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = 0;
    }
  }
};

var TabsProgressListener = {
  onStateChange(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    // Collect telemetry data about tab load times.
    if (aWebProgress.isTopLevel && (!aRequest.originalURI || aRequest.originalURI.spec.scheme != "about")) {
      let stopwatchRunning = TelemetryStopwatch.running("FX_PAGE_LOAD_MS", aBrowser);

      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
          if (stopwatchRunning) {
            // Oops, we're seeing another start without having noticed the previous stop.
            TelemetryStopwatch.cancel("FX_PAGE_LOAD_MS", aBrowser);
          }
          TelemetryStopwatch.start("FX_PAGE_LOAD_MS", aBrowser);
          Services.telemetry.getHistogramById("FX_TOTAL_TOP_VISITS").add(true);
        } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                   stopwatchRunning /* we won't see STATE_START events for pre-rendered tabs */) {
          TelemetryStopwatch.finish("FX_PAGE_LOAD_MS", aBrowser);
        }
      } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                 aStatus == Cr.NS_BINDING_ABORTED &&
                 stopwatchRunning /* we won't see STATE_START events for pre-rendered tabs */) {
        TelemetryStopwatch.cancel("FX_PAGE_LOAD_MS", aBrowser);
      }
    }

    // We used to listen for clicks in the browser here, but when that
    // became unnecessary, removing the code below caused focus issues.
    // This code should be removed. Tracked in bug 1337794.
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
      aBrowser.addEventListener("pagehide", function onPageHide(event) {
        if (event.target.defaultView.frameElement)
          return;
        aBrowser.removeEventListener("pagehide", onPageHide, true);
        if (event.target.documentElement)
          event.target.documentElement.removeAttribute("hasBrowserHandlers");
      }, true);
    }
  },

  onLocationChange(aBrowser, aWebProgress, aRequest, aLocationURI, aFlags) {
    // Filter out location changes caused by anchor navigation
    // or history.push/pop/replaceState.
    if (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      // Reader mode cares about history.pushState and friends.
      // FIXME: The content process should manage this directly (bug 1445351).
      aBrowser.messageManager.sendAsyncMessage("Reader:PushState", {
        isArticle: aBrowser.isArticle,
      });
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

    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (tab && tab._sharingState) {
      gBrowser.setBrowserSharing(aBrowser, {});
    }
    webrtcUI.forgetStreamsFromBrowser(aBrowser);

    gBrowser.getNotificationBox(aBrowser).removeTransientNotifications();

    FullZoom.onLocationChange(aLocationURI, false, aBrowser);
  },
};

function nsBrowserAccess() { }

nsBrowserAccess.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserDOMWindow, Ci.nsISupports]),

  _openURIInNewTab(aURI, aReferrer, aReferrerPolicy, aIsPrivate,
                   aIsExternal, aForceNotRemote = false,
                   aUserContextId = Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID,
                   aOpenerWindow = null, aOpenerBrowser = null,
                   aTriggeringPrincipal = null, aNextTabParentId = 0, aName = "") {
    let win, needToFocusWin;

    // try the current window.  if we're in a popup, fall back on the most recent browser window
    if (window.toolbar.visible)
      win = window;
    else {
      win = BrowserWindowTracker.getTopWindow({private: aIsPrivate});
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

    let loadInBackground = Services.prefs.getBoolPref("browser.tabs.loadDivertedInBackground");

    let tab = win.gBrowser.loadOneTab(aURI ? aURI.spec : "about:blank", {
                                      triggeringPrincipal: aTriggeringPrincipal,
                                      referrerURI: aReferrer,
                                      referrerPolicy: aReferrerPolicy,
                                      userContextId: aUserContextId,
                                      fromExternal: aIsExternal,
                                      inBackground: loadInBackground,
                                      forceNotRemote: aForceNotRemote,
                                      opener: aOpenerWindow,
                                      openerBrowser: aOpenerBrowser,
                                      nextTabParentId: aNextTabParentId,
                                      name: aName,
                                      });
    let browser = win.gBrowser.getBrowserForTab(tab);

    if (needToFocusWin || (!loadInBackground && aIsExternal))
      win.focus();

    return browser;
  },

  createContentWindow(aURI, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    return this.getContentWindowOrOpenURI(null, aOpener, aWhere, aFlags,
                                          aTriggeringPrincipal);
  },

  openURI(aURI, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    if (!aURI) {
      Cu.reportError("openURI should only be called with a valid URI");
      throw Cr.NS_ERROR_FAILURE;
    }
    return this.getContentWindowOrOpenURI(aURI, aOpener, aWhere, aFlags,
                                          aTriggeringPrincipal);
  },

  getContentWindowOrOpenURI(aURI, aOpener, aWhere, aFlags, aTriggeringPrincipal) {
    // This function should only ever be called if we're opening a URI
    // from a non-remote browser window (via nsContentTreeOwner).
    if (aOpener && Cu.isCrossProcessWrapper(aOpener)) {
      Cu.reportError("nsBrowserAccess.openURI was passed a CPOW for aOpener. " +
                     "openURI should only ever be called from non-remote browsers.");
      throw Cr.NS_ERROR_FAILURE;
    }

    var newWindow = null;
    var isExternal = !!(aFlags & Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

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
          Services.prefs.prefHasUserValue("browser.link.open_newwindow.override.external"))
        aWhere = Services.prefs.getIntPref("browser.link.open_newwindow.override.external");
      else
        aWhere = Services.prefs.getIntPref("browser.link.open_newwindow");
    }

    let referrer = aOpener ? makeURI(aOpener.location.href) : null;
    let referrerPolicy = Ci.nsIHttpChannel.REFERRER_POLICY_UNSET;
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
        let openerWindow = (aFlags & Ci.nsIBrowserDOMWindow.OPEN_NO_OPENER) ? null : aOpener;
        let browser = this._openURIInNewTab(aURI, referrer, referrerPolicy,
                                            isPrivate, isExternal,
                                            forceNotRemote, userContextId,
                                            openerWindow, null, aTriggeringPrincipal);
        if (browser)
          newWindow = browser.contentWindow;
        break;
      default : // OPEN_CURRENTWINDOW or an illegal value
        newWindow = window.content;
        if (aURI) {
          let loadflags = isExternal ?
                            Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                            Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          gBrowser.loadURI(aURI.spec, {
            triggeringPrincipal: aTriggeringPrincipal,
            flags: loadflags,
            referrerURI: referrer,
            referrerPolicy,
          });
        }
        if (!Services.prefs.getBoolPref("browser.tabs.loadDivertedInBackground"))
          window.focus();
    }
    return newWindow;
  },

  createContentWindowInFrame: function browser_createContentWindowInFrame(
                              aURI, aParams, aWhere, aFlags, aNextTabParentId,
                              aName) {
    // Passing a null-URI to only create the content window.
    return this.getContentWindowOrOpenURIInFrame(null, aParams, aWhere, aFlags,
                                                 aNextTabParentId, aName);
  },

  openURIInFrame: function browser_openURIInFrame(aURI, aParams, aWhere, aFlags,
                                                  aNextTabParentId, aName) {
    return this.getContentWindowOrOpenURIInFrame(aURI, aParams, aWhere, aFlags,
                                                 aNextTabParentId, aName);
  },

  getContentWindowOrOpenURIInFrame: function browser_getContentWindowOrOpenURIInFrame(
                                    aURI, aParams, aWhere, aFlags,
                                    aNextTabParentId, aName) {
    if (aWhere != Ci.nsIBrowserDOMWindow.OPEN_NEWTAB) {
      dump("Error: openURIInFrame can only open in new tabs");
      return null;
    }

    var isExternal = !!(aFlags & Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    var userContextId = aParams.openerOriginAttributes &&
                        ("userContextId" in aParams.openerOriginAttributes)
                          ? aParams.openerOriginAttributes.userContextId
                          : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;

    let referrer = aParams.referrer ? makeURI(aParams.referrer) : null;
    return this._openURIInNewTab(aURI, referrer,
                                 aParams.referrerPolicy,
                                 aParams.isPrivate,
                                 isExternal, false,
                                 userContextId, null, aParams.openerBrowser,
                                 aParams.triggeringPrincipal,
                                 aNextTabParentId, aName);
  },

  isTabContentWindow(aWindow) {
    return gBrowser.browsers.some(browser => browser.contentWindow == aWindow);
  },

  canClose() {
    return CanCloseWindow();
  },
};

function onViewToolbarsPopupShowing(aEvent, aInsertPoint) {
  var popup = aEvent.target;
  if (popup != aEvent.currentTarget)
    return;

  // Empty the menu
  for (var i = popup.childNodes.length - 1; i >= 0; --i) {
    var deadItem = popup.childNodes[i];
    if (deadItem.hasAttribute("toolbarId"))
      popup.removeChild(deadItem);
  }

  var firstMenuItem = aInsertPoint || popup.firstChild;

  let toolbarNodes = gNavToolbox.childNodes;

  for (let toolbar of toolbarNodes) {
    if (!toolbar.hasAttribute("toolbarname")) {
      continue;
    }

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

    menuItem.addEventListener("command", onViewToolbarCommand);
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
      if (parent.nodeType !== Node.ELEMENT_NODE ||
          (parent.classList && parent.classList.contains("customization-target")) ||
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
  let isSpecial = toolbarItem && CustomizableUI.isSpecialWidget(toolbarItem.id);
  if (movable) {
    if (isSpecial) {
      moveToPanel.setAttribute("disabled", true);
    } else {
      moveToPanel.removeAttribute("disabled");
    }
    removeFromToolbar.removeAttribute("disabled");
  } else {
    moveToPanel.setAttribute("disabled", true);
    removeFromToolbar.setAttribute("disabled", true);
  }
}

function onViewToolbarCommand(aEvent) {
  let node = aEvent.originalTarget;
  let toolbarId = node.getAttribute("toolbarId");
  let isVisible = node.getAttribute("checked") == "true";
  CustomizableUI.setToolbarVisibility(toolbarId, isVisible);
  updateToggleControlLabel(node);
}

function setToolbarVisibility(toolbar, isVisible, persist = true) {
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

  if (toolbar.getAttribute("type") == "menubar" && CustomizationHandler.isCustomizing()) {
    gCustomizeMode._updateDragSpaceCheckbox();
  }
}

function updateToggleControlLabel(control) {
  if (!control.hasAttribute("label-checked")) {
    return;
  }

  if (!control.hasAttribute("label-unchecked")) {
    control.setAttribute("label-unchecked", control.getAttribute("label"));
  }
  let prefix = (control.getAttribute("checked") == "true") ? "" : "un";
  control.setAttribute("label", control.getAttribute(`label-${prefix}checked`));
}

var TabletModeUpdater = {
  init() {
    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      this.update(WindowsUIUtils.inTabletMode);
      Services.obs.addObserver(this, "tablet-mode-change");
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
      gUIDensity.update();
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

function displaySecurityInfo() {
  BrowserPageInfo(null, "securityTab");
}

// Updates the UI density (for touch and compact mode) based on the uidensity pref.
var gUIDensity = {
  MODE_NORMAL: 0,
  MODE_COMPACT: 1,
  MODE_TOUCH: 2,
  uiDensityPref: "browser.uidensity",
  autoTouchModePref: "browser.touchmode.auto",

  init() {
    this.update();
    Services.prefs.addObserver(this.uiDensityPref, this);
    Services.prefs.addObserver(this.autoTouchModePref, this);
  },

  uninit() {
    Services.prefs.removeObserver(this.uiDensityPref, this);
    Services.prefs.removeObserver(this.autoTouchModePref, this);
  },

  observe(aSubject, aTopic, aPrefName) {
    if (aTopic != "nsPref:changed" ||
        (aPrefName != this.uiDensityPref &&
         aPrefName != this.autoTouchModePref)) {
      return;
    }

    this.update();
  },

  getCurrentDensity() {
    // Automatically override the uidensity to touch in Windows tablet mode.
    if (AppConstants.isPlatformAndVersionAtLeast("win", "10") &&
        WindowsUIUtils.inTabletMode &&
        Services.prefs.getBoolPref(this.autoTouchModePref)) {
      return { mode: this.MODE_TOUCH, overridden: true };
    }
    return { mode: Services.prefs.getIntPref(this.uiDensityPref), overridden: false };
  },

  update(mode) {
    if (mode == null) {
      mode = this.getCurrentDensity().mode;
    }

    let docs = [document.documentElement];
    let shouldUpdateSidebar = SidebarUI.initialized && SidebarUI.isOpen;
    if (shouldUpdateSidebar) {
      docs.push(SidebarUI.browser.contentDocument.documentElement);
    }
    for (let doc of docs) {
      switch (mode) {
      case this.MODE_COMPACT:
        doc.setAttribute("uidensity", "compact");
        break;
      case this.MODE_TOUCH:
        doc.setAttribute("uidensity", "touch");
        break;
      default:
        doc.removeAttribute("uidensity");
        break;
      }
    }
    if (shouldUpdateSidebar) {
      let tree = SidebarUI.browser.contentDocument.querySelector(".sidebar-placesTree");
      if (tree) {
        // Tree items don't update their styles without changing some property on the
        // parent tree element, like background-color or border. See bug 1407399.
        tree.style.border = "1px";
        tree.style.border = "";
      }
    }

    TabsInTitlebar.update();
    gBrowser.tabContainer.uiDensityChanged();
  },
};

var gHomeButton = {
  prefDomain: "browser.startup.homepage",
  observe(aSubject, aTopic, aPrefName) {
    if (aTopic != "nsPref:changed" || aPrefName != this.prefDomain)
      return;

    this.updateTooltip();
  },

  updateTooltip(homeButton) {
    if (!homeButton)
      homeButton = document.getElementById("home-button");
    if (homeButton) {
      var homePage = this.getHomePage();
      homePage = homePage.replace(/\|/g, ", ");
      if (["about:home", "about:newtab"].includes(homePage.toLowerCase()))
        homeButton.setAttribute("tooltiptext", homeButton.getAttribute("aboutHomeOverrideTooltip"));
      else
        homeButton.setAttribute("tooltiptext", homePage);
    }
  },

  getHomePage() {
    var url;
    try {
      url = Services.prefs.getComplexValue(this.prefDomain,
                                  Ci.nsIPrefLocalizedString).data;
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
  "context-reload": "reloadButton.tooltip",
  "context-stop": "stopButton.tooltip",
  "downloads-button": "downloads.tooltip",
  "fullscreen-button": "fullscreenButton.tooltip",
  "appMenu-fullscreen-button": "fullscreenButton.tooltip",
  "new-window-button": "newWindowButton.tooltip",
  "new-tab-button": "newTabButton.tooltip",
  "tabs-newtab-button": "newTabButton.tooltip",
  "reload-button": "reloadButton.tooltip",
  "stop-button": "stopButton.tooltip",
  "urlbar-zoom-button": "urlbar-zoom-button.tooltip",
  "appMenu-cut-button": "cut-button.tooltip",
  "appMenu-copy-button": "copy-button.tooltip",
  "appMenu-paste-button": "paste-button.tooltip",
  "appMenu-zoomEnlarge-button": "zoomEnlarge-button.tooltip",
  "appMenu-zoomReset-button": "zoomReset-button.tooltip",
  "appMenu-zoomReduce-button": "zoomReduce-button.tooltip",
  "reader-mode-button": "reader-mode-button.tooltip",
};
const nodeToShortcutMap = {
  "bookmarks-menu-button": "manBookmarkKb",
  "context-reload": "key_reload",
  "context-stop": "key_stop",
  "downloads-button": "key_openDownloads",
  "fullscreen-button": "key_fullScreen",
  "appMenu-fullscreen-button": "key_fullScreen",
  "new-window-button": "key_newNavigator",
  "new-tab-button": "key_newNavigatorTab",
  "tabs-newtab-button": "key_newNavigatorTab",
  "reload-button": "key_reload",
  "stop-button": "key_stop",
  "urlbar-zoom-button": "key_fullZoomReset",
  "appMenu-cut-button": "key_cut",
  "appMenu-copy-button": "key_copy",
  "appMenu-paste-button": "key_paste",
  "appMenu-zoomEnlarge-button": "key_fullZoomEnlarge",
  "appMenu-zoomReset-button": "key_fullZoomReset",
  "appMenu-zoomReduce-button": "key_fullZoomReduce",
  "reader-mode-button": "key_toggleReaderMode",
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
    SidebarUI.setWebPageIcon(gWebPanelURI);
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
function hrefAndLinkNodeForClickEvent(event) {
  function isHTMLLink(aNode) {
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
      href = node.getAttribute("href") ||
             node.getAttributeNS("http://www.w3.org/1999/xlink", "href");

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
function contentAreaClick(event, isPanelClick) {
  if (!event.isTrusted || event.defaultPrevented || event.button == 2)
    return;

  let [href, linkNode] = hrefAndLinkNodeForClickEvent(event);
  if (!href) {
    // Not a link, handle middle mouse navigation.
    if (event.button == 1 &&
        Services.prefs.getBoolPref("middlemouse.contentLoadURL") &&
        !Services.prefs.getBoolPref("general.autoScroll")) {
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
    let mainTarget = !target || target == "_content" || target == "_main";
    if (isPanelClick && mainTarget) {
      // javascript and data links should be executed in the current browser.
      if (linkNode.getAttribute("onclick") ||
          href.startsWith("javascript:") ||
          href.startsWith("data:"))
        return;

      try {
        urlSecurityCheck(href, linkNode.ownerDocument.nodePrincipal);
      } catch (ex) {
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
      PlacesUIUtils.showBookmarkDialog({ action: "add",
                                         type: "bookmark",
                                         uri: makeURI(href),
                                         title: linkNode.getAttribute("title"),
                                         loadBookmarkInSidebar: true,
                                         hiddenRows: [ "description",
                                                       "location",
                                                       "keyword" ]
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
    } catch (e) { }
  }

  // first get document wide referrer policy, then
  // get referrer attribute from clicked link and parse it and
  // allow per element referrer to overrule the document wide referrer if enabled
  let referrerPolicy = doc.referrerPolicy;
  if (linkNode) {
    let referrerAttrValue = Services.netUtils.parseAttributePolicyString(linkNode.
                            getAttribute("referrerpolicy"));
    if (referrerAttrValue != Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
      referrerPolicy = referrerAttrValue;
    }
  }

  let frameOuterWindowID = WebNavigationFrames.getFrameId(doc.defaultView);

  urlSecurityCheck(href, doc.nodePrincipal);
  let params = {
    charset: doc.characterSet,
    allowMixedContent: persistAllowMixedContentInChildTab,
    referrerURI,
    referrerPolicy,
    noReferrer: BrowserUtils.linkHasNoReferrer(linkNode),
    originPrincipal: doc.nodePrincipal,
    triggeringPrincipal: doc.nodePrincipal,
    frameOuterWindowID,
  };

  // The new tab/window must use the same userContextId
  if (doc.nodePrincipal.originAttributes.userContextId) {
    params.userContextId = doc.nodePrincipal.originAttributes.userContextId;
  }

  openLinkIn(href, where, params);
  event.preventDefault();
  return true;
}

/**
 * Handles paste on middle mouse clicks.
 *
 * @param event {Event | Object} Event or JSON object.
 */
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
                   disallowInheritPrincipal: !data.mayInheritPrincipal,
                   triggeringPrincipal: gBrowser.selectedBrowser.contentPrincipal,
                 });
    }
  });

  if (event instanceof Event) {
    event.stopPropagation();
  }
}

function stripUnsafeProtocolOnPaste(pasteData) {
  // Don't allow pasting javascript URIs since we don't support
  // LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL for those.
  while (true) {
    let scheme = "";
    try {
      scheme = Services.io.extractScheme(pasteData);
    } catch (ex) { }
    if (scheme != "javascript") {
      break;
    }

    pasteData = pasteData.substring(pasteData.indexOf(":") + 1);
  }
  return pasteData;
}

// handleDroppedLink has the following 2 overloads:
//   handleDroppedLink(event, url, name, triggeringPrincipal)
//   handleDroppedLink(event, links, triggeringPrincipal)
function handleDroppedLink(event, urlOrLinks, nameOrTriggeringPrincipal, triggeringPrincipal) {
  let links;
  if (Array.isArray(urlOrLinks)) {
    links = urlOrLinks;
    triggeringPrincipal = nameOrTriggeringPrincipal;
  } else {
    links = [{ url: urlOrLinks, nameOrTriggeringPrincipal, type: "" }];
  }

  let lastLocationChange = gBrowser.selectedBrowser.lastLocationChange;

  let userContextId = gBrowser.selectedBrowser.getAttribute("usercontextid");

  // event is null if links are dropped in content process.
  // inBackground should be false, as it's loading into current browser.
  let inBackground = false;
  if (event) {
    inBackground = Services.prefs.getBoolPref("browser.tabs.loadInBackground");
    if (event.shiftKey)
      inBackground = !inBackground;
  }

  (async function() {
    if (links.length >= Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")) {
      // Sync dialog cannot be used inside drop event handler.
      let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(links.length,
                                                                  window);
      if (!answer) {
        return;
      }
    }

    let urls = [];
    let postDatas = [];
    for (let link of links) {
      let data = await getShortcutOrURIAndPostData(link.url);
      urls.push(data.url);
      postDatas.push(data.postData);
    }
    if (lastLocationChange == gBrowser.selectedBrowser.lastLocationChange) {
      gBrowser.loadTabs(urls, {
        inBackground,
        replace: true,
        allowThirdPartyFixup: false,
        postDatas,
        userContextId,
        triggeringPrincipal,
      });
    }
  })();

  // If links are dropped in content process, event.preventDefault() should be
  // called in content process.
  if (event) {
    // Keep the event from being handled by the dragDrop listeners
    // built-in to gecko if they happen to be above us.
    event.preventDefault();
  }
}

function BrowserSetForcedCharacterSet(aCharset) {
  if (aCharset) {
    gBrowser.selectedBrowser.characterSet = aCharset;
    // Save the forced character-set
    if (!PrivateBrowsingUtils.isWindowPrivate(window))
      PlacesUtils.setCharsetForURI(getWebNavigation().currentURI, aCharset);
  }
  BrowserCharsetReload();
}

function BrowserCharsetReload() {
  BrowserReloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
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

  init() {
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
  getBrowserStyleSheets(browser) {
    if (!browser) {
      browser = gBrowser.selectedBrowser;
    }

    let data = this._pageStyleSheets.get(browser.permanentKey);
    if (!data) {
      return [];
    }
    return data.filteredStyleSheets;
  },

  _getStyleSheetInfo(browser) {
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

  fillPopup(menuPopup) {
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

  switchStyleSheet(title) {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.sendAsyncMessage("PageStyle:Switch", {title});
  },

  disableStyle() {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.sendAsyncMessage("PageStyle:Disable");
  },
};

var LanguageDetectionListener = {
  init() {
    window.messageManager.addMessageListener("Translation:DocumentState", msg => {
      Translation.documentStateReceived(msg.target, msg.data);
    });
  }
};


var BrowserOffline = {
  _inited: false,

  // BrowserOffline Public Methods
  init() {
    if (!this._uiElement)
      this._uiElement = document.getElementById("workOfflineMenuitemState");

    Services.obs.addObserver(this, "network:offline-status-changed");

    this._updateOfflineUI(Services.io.offline);

    this._inited = true;
  },

  uninit() {
    if (this._inited) {
      Services.obs.removeObserver(this, "network:offline-status-changed");
    }
  },

  toggleOfflineStatus() {
    var ioService = Services.io;

    if (!ioService.offline && !this._canGoOffline()) {
      this._updateOfflineUI(false);
      return;
    }

    ioService.offline = !ioService.offline;
  },

  // nsIObserver
  observe(aSubject, aTopic, aState) {
    if (aTopic != "network:offline-status-changed")
      return;

    // This notification is also received because of a loss in connectivity,
    // which we ignore by updating the UI to the current value of io.offline
    this._updateOfflineUI(Services.io.offline);
  },

  // BrowserOffline Implementation Methods
  _canGoOffline() {
    try {
      var cancelGoOffline = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelGoOffline, "offline-requested");

      // Something aborted the quit process.
      if (cancelGoOffline.data)
        return false;
    } catch (ex) {
    }

    return true;
  },

  _uiElement: null,
  _updateOfflineUI(aOffline) {
    var offlineLocked = Services.prefs.prefIsLocked("network.online");
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
    let options = {
      persistent: true,
      hideClose: true,
    };
    PopupNotifications.show(browser, "offline-app-usage", message,
                            anchorID, mainAction, null, options);

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
      let uri = Services.io.newURI(group);
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
        label: gNavigatorBundle.getString("offlineApps.allowStoring.label"),
        accessKey: gNavigatorBundle.getString("offlineApps.allowStoring.accesskey"),
        callback() {
          for (let [ciBrowser, ciDocId, ciUri] of notification.options.controlledItems) {
            OfflineApps.allowSite(ciBrowser, ciDocId, ciUri);
          }
        }
      };
      let secondaryActions = [{
        label: gNavigatorBundle.getString("offlineApps.dontAllow.label"),
        accessKey: gNavigatorBundle.getString("offlineApps.dontAllow.accesskey"),
        callback() {
          for (let [, , ciUri] of notification.options.controlledItems) {
            OfflineApps.disallowSite(ciUri);
          }
        }
      }];
      let message = gNavigatorBundle.getFormattedString("offlineApps.available2",
                                                        [host]);
      let anchorID = "indexedDB-notification-icon";
      let options = {
        persistent: true,
        hideClose: true,
        controlledItems: [[Cu.getWeakReference(browser), docId, uri]]
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
    openPreferences("panePrivacy", { origin: "offlineApps" });
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
    Services.obs.addObserver(this, this._permissionsPrompt);
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
    if (browser.ownerGlobal != window) {
      // Only listen for notifications for browsers in our chrome window.
      return;
    }

    // Get the host name if available or the file path otherwise.
    var host = browser.currentURI.asciiHost || browser.currentURI.pathQueryRef;

    var message;
    var responseTopic;
    if (topic == this._permissionsPrompt) {
      message = gNavigatorBundle.getFormattedString("offlineApps.available2",
                                                    [ host ]);
      responseTopic = this._permissionsResponse;
    }

    var observer = requestor.getInterface(Ci.nsIObserver);

    var mainAction = {
      label: gNavigatorBundle.getString("offlineApps.allowStoring.label"),
      accessKey: gNavigatorBundle.getString("offlineApps.allowStoring.accesskey"),
      callback() {
        observer.observe(null, responseTopic,
                         Ci.nsIPermissionManager.ALLOW_ACTION);
      }
    };

    var secondaryActions = [
      {
        label: gNavigatorBundle.getString("offlineApps.dontAllow.label"),
        accessKey: gNavigatorBundle.getString("offlineApps.dontAllow.accesskey"),
        callback() {
          observer.observe(null, responseTopic,
                           Ci.nsIPermissionManager.DENY_ACTION);
        }
      }
    ];

    PopupNotifications.show(
      browser, topic, message, this._notificationIcon, mainAction, secondaryActions,
      {
        persistent: true,
        hideClose: !Services.prefs.getBoolPref("privacy.permissionPrompts.showCloseButton"),
      });
  }
};

var CanvasPermissionPromptHelper = {
  _permissionsPrompt: "canvas-permissions-prompt",
  _notificationIcon: "canvas-notification-icon",

  init() {
    Services.obs.addObserver(this, this._permissionsPrompt);
  },

  uninit() {
    Services.obs.removeObserver(this, this._permissionsPrompt);
  },

  // aSubject is an nsIBrowser (e10s) or an nsIDOMWindow (non-e10s).
  // aData is an URL string.
  observe(aSubject, aTopic, aData) {
    if (aTopic != this._permissionsPrompt) {
      return;
    }

    let browser;
    if (aSubject instanceof Ci.nsIDOMWindow) {
      let contentWindow = aSubject.QueryInterface(Ci.nsIDOMWindow);
      browser = gBrowser.getBrowserForContentWindow(contentWindow);
    } else {
      browser = aSubject.QueryInterface(Ci.nsIBrowser);
    }

    let uri = Services.io.newURI(aData);
    if (gBrowser.selectedBrowser !== browser) {
      // Must belong to some other window.
      return;
    }

    let message = gNavigatorBundle.getFormattedString("canvas.siteprompt", ["<>"], 1);

    function setCanvasPermission(aURI, aPerm, aPersistent) {
      Services.perms.add(aURI, "canvas", aPerm,
                          aPersistent ? Ci.nsIPermissionManager.EXPIRE_NEVER
                                      : Ci.nsIPermissionManager.EXPIRE_SESSION);
    }

    let mainAction = {
      label: gNavigatorBundle.getString("canvas.allow"),
      accessKey: gNavigatorBundle.getString("canvas.allow.accesskey"),
      callback(state) {
        setCanvasPermission(uri, Ci.nsIPermissionManager.ALLOW_ACTION,
                            state && state.checkboxChecked);
      }
    };

    let secondaryActions = [{
      label: gNavigatorBundle.getString("canvas.notAllow"),
      accessKey: gNavigatorBundle.getString("canvas.notAllow.accesskey"),
      callback(state) {
        setCanvasPermission(uri, Ci.nsIPermissionManager.DENY_ACTION,
                            state && state.checkboxChecked);
      }
    }];

    let checkbox = {
      // In PB mode, we don't want the "always remember" checkbox
      show: !PrivateBrowsingUtils.isWindowPrivate(window)
    };
    if (checkbox.show) {
      checkbox.checked = true;
      checkbox.label = gBrowserBundle.GetStringFromName("canvas.remember");
    }

    let options = {
      checkbox,
      name: uri.asciiHost,
      learnMoreURL: Services.urlFormatter.formatURLPref("app.support.baseURL") + "fingerprint-permission",
    };
    PopupNotifications.show(browser, aTopic, message, this._notificationIcon,
                            mainAction, secondaryActions, options);
  }
};

var WebAuthnPromptHelper = {
  _icon: "webauthn-notification-icon",
  _topic: "webauthn-prompt",

  // The current notification, if any. The U2F manager is a singleton, we will
  // never allow more than one active request. And thus we'll never have more
  // than one notification either.
  _current: null,

  // The current transaction ID. Will be checked when we're notified of the
  // cancellation of an ongoing WebAuthhn request.
  _tid: 0,

  init() {
    Services.obs.addObserver(this, this._topic);
  },

  uninit() {
    Services.obs.removeObserver(this, this._topic);
  },

  observe(aSubject, aTopic, aData) {
    let mgr = aSubject.QueryInterface(Ci.nsIU2FTokenManager);
    let data = JSON.parse(aData);

    if (data.action == "register") {
      this.register(mgr, data);
    } else if (data.action == "register-direct") {
      this.registerDirect(mgr, data);
    } else if (data.action == "sign") {
      this.sign(mgr, data);
    } else if (data.action == "cancel") {
      this.cancel(data);
    }
  },

  register(mgr, {origin, tid}) {
    let mainAction = this.buildCancelAction(mgr, tid);
    this.show(tid, "register", "webauthn.registerPrompt", origin, mainAction);
  },

  registerDirect(mgr, {origin, tid}) {
    let mainAction = this.buildProceedAction(mgr, tid);
    let secondaryActions = [this.buildCancelAction(mgr, tid)];

    let learnMoreURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "webauthn-direct-attestation";

    let options = {
      learnMoreURL,
      checkbox: {
        label: gNavigatorBundle.getString("webauthn.anonymize")
      }
    };

    this.show(tid, "register-direct", "webauthn.registerDirectPrompt",
              origin, mainAction, secondaryActions, options);
  },

  sign(mgr, {origin, tid}) {
    let mainAction = this.buildCancelAction(mgr, tid);
    this.show(tid, "sign", "webauthn.signPrompt", origin, mainAction);
  },

  show(tid, id, stringId, origin, mainAction, secondaryActions = [], options = {}) {
    this.reset();

    try {
      origin = Services.io.newURI(origin).asciiHost;
    } catch (e) {
      /* Might fail for arbitrary U2F RP IDs. */
    }

    let brandShortName =
      document.getElementById("bundle_brand").getString("brandShortName");
    let message =
      gNavigatorBundle.getFormattedString(stringId, ["<>", brandShortName], 1);

    options.name = origin;
    options.hideClose = true;
    options.eventCallback = event => {
      if (event == "removed") {
        this._current = null;
        this._tid = 0;
      }
    };

    this._tid = tid;
    this._current = PopupNotifications.show(
      gBrowser.selectedBrowser, `webauthn-prompt-${id}`, message,
      this._icon, mainAction, secondaryActions, options);
  },

  cancel({tid}) {
    if (this._tid == tid) {
      this.reset();
    }
  },

  reset() {
    if (this._current) {
      this._current.remove();
    }
  },

  buildProceedAction(mgr, tid) {
    return {
      label: gNavigatorBundle.getString("webauthn.proceed"),
      accessKey: gNavigatorBundle.getString("webauthn.proceed.accesskey"),
      callback(state) {
        mgr.resumeRegister(tid, !state.checkboxChecked);
      }
    };
  },

  buildCancelAction(mgr, tid) {
    return {
      label: gNavigatorBundle.getString("webauthn.cancel"),
      accessKey: gNavigatorBundle.getString("webauthn.cancel.accesskey"),
      callback() {
        mgr.cancel(tid);
      }
    };
  },
};

function CanCloseWindow() {
  // Avoid redundant calls to canClose from showing multiple
  // PermitUnload dialogs.
  if (Services.startup.shuttingDown || window.skipNextCanClose) {
    return true;
  }

  let timedOutProcesses = new WeakSet();

  for (let browser of gBrowser.browsers) {
    // Don't instantiate lazy browsers.
    if (!browser.isConnected) {
      continue;
    }

    let pmm = browser.messageManager.processMessageManager;

    if (timedOutProcesses.has(pmm)) {
      continue;
    }

    let {permitUnload, timedOut} = browser.permitUnload();

    if (timedOut) {
      timedOutProcesses.add(pmm);
      continue;
    }

    if (!permitUnload) {
      return false;
    }
  }
  return true;
}

function WindowIsClosing() {
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
                                 "last-pb-context-exiting");
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
                     "browser-lastwindow-close-requested");
  if (closingCanceled.data)
    return false;

  os.notifyObservers(null, "browser-lastwindow-close-granted");

  // OS X doesn't quit the application when the last window is closed, but keeps
  // the session alive. Hence don't prompt users to save tabs, but warn about
  // closing multiple tabs.
  return AppConstants.platform != "macosx"
         || (isPBWindow || gBrowser.warnAboutClosingTabs(gBrowser.closingTabsEnum.ALL));
}

var MailIntegration = {
  sendLinkForBrowser(aBrowser) {
    this.sendMessage(gURLBar.makeURIReadable(aBrowser.currentURI).displaySpec, aBrowser.contentTitle);
  },

  sendMessage(aBody, aSubject) {
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
  _launchExternalUrl(aURL) {
    var extProtocolSvc =
       Cc["@mozilla.org/uriloader/external-protocol-service;1"]
         .getService(Ci.nsIExternalProtocolService);
    if (extProtocolSvc)
      extProtocolSvc.loadURI(aURL);
  }
};

function BrowserOpenAddonsMgr(aView) {
  return new Promise(resolve => {
    let emWindow;
    let browserWindow;

    var receivePong = function(aSubject, aTopic, aData) {
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
    };
    Services.obs.addObserver(receivePong, "EM-pong");
    Services.obs.notifyObservers(null, "EM-ping");
    Services.obs.removeObserver(receivePong, "EM-pong");

    if (emWindow) {
      if (aView) {
        emWindow.loadView(aView);
      }
      browserWindow.gBrowser.selectedTab =
        browserWindow.gBrowser._getTabForContentWindow(emWindow);
      emWindow.focus();
      resolve(emWindow);
      return;
    }

    // This must be a new load, else the ping/pong would have
    // found the window above.
    let whereToOpen = (window.gBrowser && isTabEmpty(gBrowser.selectedTab)) ?
                      "current" :
                      "tab";
    openTrustedLinkIn("about:addons", whereToOpen);

    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      if (aView) {
        aSubject.loadView(aView);
      }
      aSubject.QueryInterface(Ci.nsIDOMWindow);
      aSubject.focus();
      resolve(aSubject);
    }, "EM-loaded");
  });
}

function AddKeywordForSearchField() {
  let mm = gBrowser.selectedBrowser.messageManager;

  let onMessage = (message) => {
    mm.removeMessageListener("ContextMenu:SearchFieldBookmarkData:Result", onMessage);

    let bookmarkData = message.data;
    let title = gNavigatorBundle.getFormattedString("addKeywordTitleAutoFill",
                                                    [bookmarkData.title]);
    PlacesUIUtils.showBookmarkDialog({ action: "add",
                                       type: "bookmark",
                                       uri: makeURI(bookmarkData.spec),
                                       title,
                                       description: bookmarkData.description,
                                       keyword: "",
                                       postData: bookmarkData.postData,
                                       charSet: bookmarkData.charset,
                                       hiddenRows: [ "location",
                                                     "description",
                                                     "tags",
                                                     "loadInSidebar" ]
                                     }, window);
  };
  mm.addMessageListener("ContextMenu:SearchFieldBookmarkData:Result", onMessage);

  mm.sendAsyncMessage("ContextMenu:SearchFieldBookmarkData", {}, { target: gContextMenu.target });
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
    // There are two special-cases involving about:blank. One is where
    // the user has manually loaded it and it got created with a null
    // principal. The other involves the case where we load
    // some other empty page in a browser and the current page is the
    // initial about:blank page (which has that as its principal, not
    // just URI in which case it could be web-based). Especially in
    // e10s, we need to tackle that case specifically to avoid race
    // conditions when updating the URL bar.
    //
    // Note that we check the documentURI here, since the currentURI on
    // the browser might have been set by SessionStore in order to
    // support switch-to-tab without having actually loaded the content
    // yet.
    let uriToCheck = browser.documentURI || uri;
    if ((uriToCheck.spec == "about:blank" && contentPrincipal.isNullPrincipal) ||
        contentPrincipal.URI.spec == "about:blank") {
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
  gSync.openSyncedTabsPanel();
}

function ReportFalseDeceptiveSite() {
  let docURI = gBrowser.selectedBrowser.documentURI;
  let isPhishingPage =
    docURI && docURI.spec.startsWith("about:blocked?e=deceptiveBlocked");

  if (isPhishingPage) {
    let mm = gBrowser.selectedBrowser.messageManager;
    let onMessage = (message) => {
      mm.removeMessageListener("DeceptiveBlockedDetails:Result", onMessage);
      let reportUrl = gSafeBrowsing.getReportURL("PhishMistake", message.data.blockedInfo);
      if (reportUrl) {
        openTrustedLinkIn(reportUrl, "tab");
      } else {
        let bundle =
          Services.strings.createBundle("chrome://browser/locale/safebrowsing/safebrowsing.properties");
        Services.prompt.alert(window,
                              bundle.GetStringFromName("errorReportFalseDeceptiveTitle"),
                              bundle.formatStringFromName("errorReportFalseDeceptiveMessage",
                                                          [message.data.blockedInfo.provider], 1));
        }
    };
    mm.addMessageListener("DeceptiveBlockedDetails:Result", onMessage);

    mm.sendAsyncMessage("DeceptiveBlockedDetails");
  }
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
  return aIsPref ? Services.urlFormatter.formatURLPref(aFormat) :
                   Services.urlFormatter.formatURL(aFormat);
}

/**
 * Fired on the "marionette-remote-control" system notification,
 * indicating if the browser session is under remote control.
 */
const gRemoteControl = {
  observe(subject, topic, data) {
    gRemoteControl.updateVisualCue(data);
  },

  updateVisualCue(enabled) {
    const mainWindow = document.documentElement;
    if (enabled) {
      mainWindow.setAttribute("remotecontrol", "true");
    } else {
      mainWindow.removeAttribute("remotecontrol");
    }
  },
};

function getNotificationBox(aWindow) {
  var foundBrowser = gBrowser.getBrowserForDocument(aWindow.document);
  if (foundBrowser)
    return gBrowser.getNotificationBox(foundBrowser);
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

const gAccessibilityServiceIndicator = {
  init() {
    // Pref to enable accessibility service indicator.
    Services.prefs.addObserver("accessibility.indicator.enabled", this);
    // Accessibility service init/shutdown event.
    Services.obs.addObserver(this, "a11y-init-or-shutdown");
    this.update(Services.appinfo.accessibilityEnabled);
  },

  update(accessibilityEnabled = false) {
    if (this.enabled && accessibilityEnabled) {
      this._active = true;
      document.documentElement.setAttribute("accessibilitymode", "true");
      [...document.querySelectorAll(".accessibility-indicator")].forEach(
        indicator => ["click", "keypress"].forEach(type =>
          indicator.addEventListener(type, this)));
      TabsInTitlebar.update();
    } else if (this._active) {
      this._active = false;
      document.documentElement.removeAttribute("accessibilitymode");
      [...document.querySelectorAll(".accessibility-indicator")].forEach(
        indicator => ["click", "keypress"].forEach(type =>
          indicator.removeEventListener(type, this)));
      TabsInTitlebar.update();
    }
  },

  observe(subject, topic, data) {
    if (topic == "nsPref:changed" && data === "accessibility.indicator.enabled") {
      this.update(Services.appinfo.accessibilityEnabled);
    } else if (topic === "a11y-init-or-shutdown") {
      // When "a11y-init-or-shutdown" event is fired, "1" indicates that
      // accessibility service is started and "0" that it is shut down.
      this.update(data === "1");
    }
  },

  get enabled() {
    return Services.prefs.getBoolPref("accessibility.indicator.enabled");
  },

  handleEvent({ key, type }) {
    if ((type === "keypress" && [" ", "Enter"].includes(key)) ||
         type === "click") {
      let a11yServicesSupportURL =
        Services.urlFormatter.formatURLPref("accessibility.support.url");
      gBrowser.selectedTab = gBrowser.addTab(a11yServicesSupportURL);
      Services.telemetry.scalarSet("a11y.indicator_acted_on", true);
    }
  },

  uninit() {
    Services.prefs.removeObserver("accessibility.indicator.enabled", this);
    Services.obs.removeObserver(this, "a11y-init-or-shutdown");
    this.update();
  }
};

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

/**
 * Switch to a tab that has a given URI, and focuses its browser window.
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
 *        will be the parameter object that gets passed to openTrustedLinkIn. Please
 *        see the documentation for openTrustedLinkIn to see what parameters can be
 *        passed via this object.
 *        This object also allows:
 *        - 'ignoreFragment' property to be set to true to exclude fragment-portion
 *        matching when comparing URIs.
 *          If set to "whenComparing", the fragment will be unmodified.
 *          If set to "whenComparingAndReplace", the fragment will be replaced.
 *        - 'ignoreQueryString' boolean property to be set to true to exclude query string
 *        matching when comparing URIs.
 *        - 'replaceQueryString' boolean property to be set to true to exclude query string
 *        matching when comparing URIs and overwrite the initial query string with
 *        the one from the new URI.
 *        - 'adoptIntoActiveWindow' boolean property to be set to true to adopt the tab
 *        into the current window.
 * @return True if an existing tab was found, false otherwise
 */
function switchToTabHavingURI(aURI, aOpenNew, aOpenParams = {}) {
  // Certain URLs can be switched to irrespective of the source or destination
  // window being in private browsing mode:
  const kPrivateBrowsingWhitelist = new Set([
    "about:addons",
  ]);

  let ignoreFragment = aOpenParams.ignoreFragment;
  let ignoreQueryString = aOpenParams.ignoreQueryString;
  let replaceQueryString = aOpenParams.replaceQueryString;
  let adoptIntoActiveWindow = aOpenParams.adoptIntoActiveWindow;

  // These properties are only used by switchToTabHavingURI and should
  // not be used as a parameter for the new load.
  delete aOpenParams.ignoreFragment;
  delete aOpenParams.ignoreQueryString;
  delete aOpenParams.replaceQueryString;
  delete aOpenParams.adoptIntoActiveWindow;

  let isBrowserWindow = !!window.gBrowser;

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

    // Remove the query string, fragment, both, or neither from a given url.
    function cleanURL(url, removeQuery, removeFragment) {
      let ret = url;
      if (removeFragment) {
        ret = ret.split("#")[0];
        if (removeQuery) {
          // This removes a query, if present before the fragment.
          ret = ret.split("?")[0];
        }
      } else if (removeQuery) {
        // This is needed in case there is a fragment after the query.
        let fragment = ret.split("#")[1];
        ret = ret.split("?")[0].concat(
          (fragment != undefined) ? "#".concat(fragment) : "");
      }
      return ret;
    }

    // Need to handle nsSimpleURIs here too (e.g. about:...), which don't
    // work correctly with URL objects - so treat them as strings
    let ignoreFragmentWhenComparing = typeof ignoreFragment == "string" &&
                                      ignoreFragment.startsWith("whenComparing");
    let requestedCompare = cleanURL(
          aURI.displaySpec, ignoreQueryString || replaceQueryString, ignoreFragmentWhenComparing);
    let browsers = aWindow.gBrowser.browsers;
    for (let i = 0; i < browsers.length; i++) {
      let browser = browsers[i];
      let browserCompare = cleanURL(
          browser.currentURI.displaySpec, ignoreQueryString || replaceQueryString, ignoreFragmentWhenComparing);
      if (requestedCompare == browserCompare) {
        // If adoptIntoActiveWindow is set, and this is a cross-window switch,
        // adopt the tab into the current window, after the active tab.
        let doAdopt = adoptIntoActiveWindow && isBrowserWindow && aWindow != window;

        if (doAdopt) {
          window.gBrowser.adoptTab(
            aWindow.gBrowser.getTabForBrowser(browser),
            window.gBrowser.tabContainer.selectedIndex + 1,
            /* aSelectTab = */ true
          );
        } else {
          aWindow.focus();
        }

        if (ignoreFragment == "whenComparingAndReplace" || replaceQueryString) {
          browser.loadURI(aURI.spec);
        }

        if (!doAdopt) {
          aWindow.gBrowser.tabContainer.selectedIndex = i;
        }

        return true;
      }
    }
    return false;
  }

  // This can be passed either nsIURI or a string.
  if (!(aURI instanceof Ci.nsIURI))
    aURI = Services.io.newURI(aURI);

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
      openTrustedLinkIn(aURI.spec, "current", aOpenParams);
    else
      openTrustedLinkIn(aURI.spec, "tab", aOpenParams);
  }

  return false;
}

var RestoreLastSessionObserver = {
  init() {
    if (SessionStore.canRestoreLastSession &&
        !PrivateBrowsingUtils.isWindowPrivate(window)) {
      Services.obs.addObserver(this, "sessionstore-last-session-cleared", true);
      goSetCommandEnabled("Browser:RestoreLastSession", true);
    } else if (SessionStartup.isAutomaticRestoreEnabled()) {
      document.getElementById("Browser:RestoreLastSession").setAttribute("hidden", true);
    }
  },

  observe() {
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

/* Observes menus and adjusts their size for better
 * usability when opened via a touch screen. */
var MenuTouchModeObserver = {
  init() {
    window.addEventListener("popupshowing", this, true);
  },

  handleEvent(event) {
    let target = event.originalTarget;
    if (event.mozInputSource == MouseEvent.MOZ_SOURCE_TOUCH) {
      target.setAttribute("touchmode", "true");
    } else {
      target.removeAttribute("touchmode");
    }
  },

  uninit() {
    window.removeEventListener("popupshowing", this, true);
  },
};

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

    if (this.contextTab.hasAttribute("customizemode"))
      document.getElementById("context_openTabInWindow").disabled = true;

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
    // following it.
    document.getElementById("context_closeTabsToTheEnd").disabled =
      gBrowser.getTabsToTheEndFrom(this.contextTab).length == 0;

    // Disable "Close other Tabs" if there are no unpinned tabs.
    let unpinnedTabsToClose = gBrowser.visibleTabs.length - gBrowser._numPinnedTabs;
    if (!this.contextTab.pinned) {
      unpinnedTabsToClose--;
    }
    document.getElementById("context_closeOtherTabs").disabled = unpinnedTabsToClose < 1;

    // Hide "Bookmark All Tabs" for a pinned tab.  Update its state if visible.
    let bookmarkAllTabs = document.getElementById("context_bookmarkAllTabs");
    bookmarkAllTabs.hidden = this.contextTab.pinned;
    if (!bookmarkAllTabs.hidden)
      PlacesCommandHook.updateBookmarkAllTabsCommand();

    // Adjust the state of the toggle mute menu item.
    let toggleMute = document.getElementById("context_toggleMuteTab");
    if (this.contextTab.hasAttribute("activemedia-blocked")) {
      toggleMute.label = gNavigatorBundle.getString("playTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("playTab.accesskey");
    } else if (this.contextTab.hasAttribute("muted")) {
      toggleMute.label = gNavigatorBundle.getString("unmuteTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("unmuteTab.accesskey");
    } else {
      toggleMute.label = gNavigatorBundle.getString("muteTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("muteTab.accesskey");
    }

    this.contextTab.toggleMuteMenuItem = toggleMute;
    this._updateToggleMuteMenuItem(this.contextTab);

    this.contextTab.addEventListener("TabAttrModified", this);
    aPopupMenu.addEventListener("popuphiding", this);

    gSync.updateTabContextMenu(aPopupMenu, this.contextTab);
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
          attr => aEvent.detail.changed.includes(attr));
        break;
    }
  }
};

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

  Services.obs.notifyObservers(null, "restart-in-safe-mode");
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
                               "browser-delayed-startup-finished");
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

var MousePosTracker = {
  _listeners: new Set(),
  _x: 0,
  _y: 0,
  get _windowUtils() {
    delete this._windowUtils;
    return this._windowUtils = window.getInterface(Ci.nsIDOMWindowUtils);
  },

  addListener(listener) {
    if (this._listeners.has(listener))
      return;

    listener._hover = false;
    this._listeners.add(listener);

    this._callListener(listener);
  },

  removeListener(listener) {
    this._listeners.delete(listener);
  },

  handleEvent(event) {
    var fullZoom = this._windowUtils.fullZoom;
    this._x = event.screenX / fullZoom - window.mozInnerScreenX;
    this._y = event.screenY / fullZoom - window.mozInnerScreenY;

    this._listeners.forEach(function(listener) {
      try {
        this._callListener(listener);
      } catch (e) {
        Cu.reportError(e);
      }
    }, this);
  },

  _callListener(listener) {
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
    } else if (listener.onMouseLeave) {
      listener.onMouseLeave();
    }
  }
};

var ToolbarIconColor = {
  _windowState: {
    "active": false,
    "fullscreen": false,
    "tabsintitlebar": false
  },
  init() {
    this._initialized = true;

    window.addEventListener("activate", this);
    window.addEventListener("deactivate", this);
    window.addEventListener("toolbarvisibilitychange", this);
    Services.obs.addObserver(this, "lightweight-theme-styling-update");

    // If the window isn't active now, we assume that it has never been active
    // before and will soon become active such that inferFromText will be
    // called from the initial activate event.
    if (Services.focus.activeWindow == window) {
      this.inferFromText("activate");
    }
  },

  uninit() {
    this._initialized = false;

    window.removeEventListener("activate", this);
    window.removeEventListener("deactivate", this);
    window.removeEventListener("toolbarvisibilitychange", this);
    Services.obs.removeObserver(this, "lightweight-theme-styling-update");
  },

  handleEvent(event) {
    switch (event.type) {
      case "activate": // falls through
      case "deactivate":
        this.inferFromText(event.type);
        break;
      case "toolbarvisibilitychange":
        this.inferFromText(event.type, event.visible);
        break;
    }
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "lightweight-theme-styling-update":
        // inferFromText needs to run after LightweightThemeConsumer.jsm's
        // lightweight-theme-styling-update observer.
        setTimeout(() => {
          this.inferFromText(aTopic);
        }, 0);
        break;
    }
  },

  // a cache of luminance values for each toolbar
  // to avoid unnecessary calls to getComputedStyle
  _toolbarLuminanceCache: new Map(),

  inferFromText(reason, reasonValue) {
    if (!this._initialized)
      return;
    function parseRGB(aColorString) {
      let rgb = aColorString.match(/^rgba?\((\d+), (\d+), (\d+)/);
      rgb.shift();
      return rgb.map(x => parseInt(x));
    }

    switch (reason) {
      case "activate": // falls through
      case "deactivate":
        this._windowState.active = (reason === "activate");
        break;
      case "fullscreen":
        this._windowState.fullscreen = reasonValue;
        break;
      case "lightweight-theme-styling-update":
        // theme change, we'll need to recalculate all color values
        this._toolbarLuminanceCache.clear();
        break;
      case "toolbarvisibilitychange":
        // toolbar changes dont require reset of the cached color values
        break;
      case "tabsintitlebar":
        this._windowState.tabsintitlebar = reasonValue;
        break;
    }

    let toolbarSelector = "#navigator-toolbox > toolbar:not([collapsed=true])";
    if (AppConstants.platform == "macosx")
      toolbarSelector += ":not([type=menubar])";

    // The getComputedStyle calls and setting the brighttext are separated in
    // two loops to avoid flushing layout and making it dirty repeatedly.
    let cachedLuminances = this._toolbarLuminanceCache;
    let luminances = new Map();
    for (let toolbar of document.querySelectorAll(toolbarSelector)) {
      // toolbars *should* all have ids, but guard anyway to avoid blowing up
      let cacheKey = toolbar.id && toolbar.id + JSON.stringify(this._windowState);
      // lookup cached luminance value for this toolbar in this window state
      let luminance = cacheKey && cachedLuminances.get(cacheKey);
      if (isNaN(luminance)) {
        let [r, g, b] = parseRGB(getComputedStyle(toolbar).color);
        luminance = 0.2125 * r + 0.7154 * g + 0.0721 * b;
        if (cacheKey) {
          cachedLuminances.set(cacheKey, luminance);
        }
      }
      luminances.set(toolbar, luminance);
    }

    for (let [toolbar, luminance] of luminances) {
      if (luminance <= 110)
        toolbar.removeAttribute("brighttext");
      else
        toolbar.setAttribute("brighttext", "true");
    }
  }
};

var PanicButtonNotifier = {
  init() {
    this._initialized = true;
    if (window.PanicButtonNotifierShouldNotify) {
      delete window.PanicButtonNotifierShouldNotify;
      this.notify();
    }
  },
  notify() {
    if (!this._initialized) {
      window.PanicButtonNotifierShouldNotify = true;
      return;
    }
    // Display notification panel here...
    try {
      let popup = document.getElementById("panic-button-success-notification");
      popup.hidden = false;
      // To close the popup in 3 seconds after the popup is shown but left uninteracted.
      let onTimeout = () => {
        PanicButtonNotifier.close();
        removeListeners();
      };
      popup.addEventListener("popupshown", function() {
        PanicButtonNotifier.timer = setTimeout(onTimeout, 3000);
      });
      // To prevent the popup from closing when user tries to interact with the
      // popup using mouse or keyboard.
      let onUserInteractsWithPopup = () => {
        clearTimeout(PanicButtonNotifier.timer);
        removeListeners();
       };
      popup.addEventListener("mouseover", onUserInteractsWithPopup);
      window.addEventListener("keydown", onUserInteractsWithPopup);
      let removeListeners = () => {
        popup.removeEventListener("mouseover", onUserInteractsWithPopup);
        window.removeEventListener("keydown", onUserInteractsWithPopup);
        popup.removeEventListener("popuphidden", removeListeners);
      };
      popup.addEventListener("popuphidden", removeListeners);

      let widget = CustomizableUI.getWidget("panic-button").forWindow(window);
      let anchor = widget.anchor;
      anchor = document.getAnonymousElementByAttribute(anchor, "class", "toolbarbutton-icon");
      popup.openPopup(anchor, popup.getAttribute("position"));
    } catch (ex) {
      Cu.reportError(ex);
    }
  },
  close() {
    let popup = document.getElementById("panic-button-success-notification");
    popup.hidePopup();
  },
};

var AboutCapabilitiesListener = {
  _topics: [
    "AboutCapabilities:OpenPrivateWindow",
    "AboutCapabilities:DontShowIntroPanelAgain",
  ],

  init() {
    let mm = window.messageManager;
    for (let topic of this._topics) {
      mm.addMessageListener(topic, this);
    }
  },

  uninit() {
    let mm = window.messageManager;
    for (let topic of this._topics) {
      mm.removeMessageListener(topic, this);
    }
  },

  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "AboutCapabilities:OpenPrivateWindow":
        OpenBrowserWindow({private: true});
        break;

      case "AboutCapabilities:DontShowIntroPanelAgain":
        TrackingProtection.dontShowIntroPanelAgain();
        break;
    }
  },
};

const SafeBrowsingNotificationBox = {
  _currentURIBaseDomain: null,
  show(title, buttons) {
    let uri = gBrowser.currentURI;

    // start tracking host so that we know when we leave the domain
    this._currentURIBaseDomain = Services.eTLD.getBaseDomain(uri);

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
  onLocationChange(aLocationURI) {
    // take this to represent that you haven't visited a bad place
    if (!this._currentURIBaseDomain) {
      return;
    }

    let newURIBaseDomain = Services.eTLD.getBaseDomain(aLocationURI);

    if (newURIBaseDomain !== this._currentURIBaseDomain) {
      let notificationBox = gBrowser.getNotificationBox();
      let notification = notificationBox.getNotificationWithValue("blocked-badware-page");
      if (notification) {
        notificationBox.removeNotification(notification, false);
      }

      this._currentURIBaseDomain = null;
    }
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
    browser.parentNode.insertBefore(newPrompt, browser.nextSibling);
    browser.setAttribute("tabmodalPromptShowing", true);

    newPrompt.clientTop; // style flush to assure binding is attached

    let prompts = this.listPrompts();
    if (prompts.length > 1) {
      // Let's hide ourself behind the current prompt.
      newPrompt.hidden = true;
    }

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
      let label = gTabBrowserBundle.formatStringFromName("tabs.allowTabFocusByPromptForSite",
                                                      [hostForAllowFocusCheckbox], 1);
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
      prompt.hidden = false;
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
