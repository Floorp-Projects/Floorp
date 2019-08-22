/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.import("resource://gre/modules/NotificationDB.jsm");

// lazy module getters

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AMTelemetry: "resource://gre/modules/AddonManager.jsm",
  NewTabPagePreloading: "resource:///modules/NewTabPagePreloading.jsm",
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  CFRPageActions: "resource://activity-stream/lib/CFRPageActions.jsm",
  CharsetMenu: "resource://gre/modules/CharsetMenu.jsm",
  Color: "resource://gre/modules/Color.jsm",
  ContentSearch: "resource:///modules/ContentSearch.jsm",
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.jsm",
  CustomizableUI: "resource:///modules/CustomizableUI.jsm",
  Deprecated: "resource://gre/modules/Deprecated.jsm",
  DownloadsCommon: "resource:///modules/DownloadsCommon.jsm",
  DownloadUtils: "resource://gre/modules/DownloadUtils.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ExtensionsUI: "resource:///modules/ExtensionsUI.jsm",
  HomePage: "resource:///modules/HomePage.jsm",
  LightweightThemeConsumer:
    "resource://gre/modules/LightweightThemeConsumer.jsm",
  Log: "resource://gre/modules/Log.jsm",
  LoginManagerParent: "resource://gre/modules/LoginManagerParent.jsm",
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  OpenInTabsUtils: "resource:///modules/OpenInTabsUtils.jsm",
  PageActions: "resource:///modules/PageActions.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PanelMultiView: "resource:///modules/PanelMultiView.jsm",
  PanelView: "resource:///modules/PanelMultiView.jsm",
  PermitUnloader: "resource://gre/actors/BrowserElementParent.jsm",
  PictureInPicture: "resource://gre/modules/PictureInPicture.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  Pocket: "chrome://pocket/content/Pocket.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ProcessHangMonitor: "resource:///modules/ProcessHangMonitor.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  // TODO (Bug 1529552): Remove once old urlbar code goes away.
  ReaderMode: "resource://gre/modules/ReaderMode.jsm",
  ReaderParent: "resource:///modules/ReaderParent.jsm",
  RFPHelper: "resource://gre/modules/RFPHelper.jsm",
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  SessionStartup: "resource:///modules/sessionstore/SessionStartup.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.jsm",
  SimpleServiceDiscovery: "resource://gre/modules/SimpleServiceDiscovery.jsm",
  SiteDataManager: "resource:///modules/SiteDataManager.jsm",
  SitePermissions: "resource:///modules/SitePermissions.jsm",
  SubframeCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  TabModalPrompt: "chrome://global/content/tabprompts.jsm",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  Translation: "resource:///modules/translation/Translation.jsm",
  UITour: "resource:///modules/UITour.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
  UrlbarInput: "resource:///modules/UrlbarInput.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
  UrlbarValueFormatter: "resource:///modules/UrlbarValueFormatter.jsm",
  Weave: "resource://services-sync/main.js",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  fxAccounts: "resource://gre/modules/FxAccounts.jsm",
  webrtcUI: "resource:///modules/webrtcUI.jsm",
  ZoomUI: "resource:///modules/ZoomUI.jsm",
});

if (AppConstants.MOZ_CRASHREPORTER) {
  ChromeUtils.defineModuleGetter(
    this,
    "PluginCrashReporter",
    "resource:///modules/ContentCrashHandlers.jsm"
  );
}

XPCOMUtils.defineLazyScriptGetter(
  this,
  "PlacesTreeView",
  "chrome://browser/content/places/treeView.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["PlacesInsertionPoint", "PlacesController", "PlacesControllerDragHelper"],
  "chrome://browser/content/places/controller.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "PrintUtils",
  "chrome://global/content/printUtils.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "ZoomManager",
  "chrome://global/content/viewZoomOverlay.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "FullZoom",
  "chrome://browser/content/browser-fullZoom.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "PanelUI",
  "chrome://browser/content/customizableui/panelUI.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gViewSourceUtils",
  "chrome://global/content/viewSourceUtils.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gTabsPanel",
  "chrome://browser/content/browser-allTabsMenu.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["gExtensionsNotifications", "gXPInstallObserver"],
  "chrome://browser/content/browser-addons.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "ctrlTab",
  "chrome://browser/content/browser-ctrlTab.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["CustomizationHandler", "AutoHideMenubar"],
  "chrome://browser/content/browser-customization.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["PointerLock", "FullScreen"],
  "chrome://browser/content/browser-fullScreenAndPointerLock.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gIdentityHandler",
  "chrome://browser/content/browser-siteIdentity.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gProtectionsHandler",
  "chrome://browser/content/browser-siteProtections.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["gGestureSupport", "gHistorySwipeAnimation"],
  "chrome://browser/content/browser-gestureSupport.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gSafeBrowsing",
  "chrome://browser/content/browser-safebrowsing.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gSync",
  "chrome://browser/content/browser-sync.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gBrowserThumbnails",
  "chrome://browser/content/browser-thumbnails.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["openContextMenu", "nsContextMenu"],
  "chrome://browser/content/nsContextMenu.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  [
    "DownloadsPanel",
    "DownloadsOverlayLoader",
    "DownloadsSubview",
    "DownloadsView",
    "DownloadsViewUI",
    "DownloadsViewController",
    "DownloadsSummary",
    "DownloadsFooter",
    "DownloadsBlockedSubview",
  ],
  "chrome://browser/content/downloads/downloads.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["DownloadsButton", "DownloadsIndicatorView"],
  "chrome://browser/content/downloads/indicator.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "gEditItemOverlay",
  "chrome://browser/content/places/editBookmark.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "SearchOneOffs",
  "chrome://browser/content/search/search-one-offs.js"
);
if (AppConstants.NIGHTLY_BUILD) {
  XPCOMUtils.defineLazyScriptGetter(
    this,
    "gGfxUtils",
    "chrome://browser/content/browser-graphics-utils.js"
  );
}

XPCOMUtils.defineLazyScriptGetter(
  this,
  "pktUI",
  "chrome://pocket/content/main.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  "ToolbarKeyboardNavigator",
  "chrome://browser/content/browser-toolbarKeyNav.js"
);

// lazy service getters

XPCOMUtils.defineLazyServiceGetters(this, {
  ContentPrefService2: [
    "@mozilla.org/content-pref/service;1",
    "nsIContentPrefService2",
  ],
  classifierService: [
    "@mozilla.org/url-classifier/dbservice;1",
    "nsIURIClassifier",
  ],
  Favicons: ["@mozilla.org/browser/favicon-service;1", "nsIFaviconService"],
  gAboutNewTabService: [
    "@mozilla.org/browser/aboutnewtab-service;1",
    "nsIAboutNewTabService",
  ],
  gDNSService: ["@mozilla.org/network/dns-service;1", "nsIDNSService"],
  gSerializationHelper: [
    "@mozilla.org/network/serialization-helper;1",
    "nsISerializationHelper",
  ],
  Marionette: ["@mozilla.org/remote/marionette;1", "nsIMarionette"],
  WindowsUIUtils: ["@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils"],
});

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyServiceGetter(
    this,
    "gCrashReporter",
    "@mozilla.org/xre/app-info;1",
    "nsICrashReporter"
  );
}

XPCOMUtils.defineLazyGetter(this, "RTL_UI", () => {
  return Services.locale.isAppLocaleRTL;
});

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", () => {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", () => {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gTabBrowserBundle", () => {
  return Services.strings.createBundle(
    "chrome://browser/locale/tabbrowser.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gCustomizeMode", () => {
  let { CustomizeMode } = ChromeUtils.import(
    "resource:///modules/CustomizeMode.jsm"
  );
  return new CustomizeMode(window);
});

XPCOMUtils.defineLazyGetter(this, "gNavToolbox", () => {
  return document.getElementById("navigator-toolbox");
});

XPCOMUtils.defineLazyGetter(this, "gURLBar", () => gURLBarHandler.urlbar);

/**
 * Tracks the urlbar object, allowing to reinitiate it when necessary, e.g. on
 * customization.
 */
var gURLBarHandler = {
  /**
   * The urlbar binding or object.
   */
  get urlbar() {
    if (!this._urlbar) {
      let textbox = document.getElementById("urlbar");
      this._urlbar = new UrlbarInput({
        textbox,
        eventTelemetryCategory: "urlbar",
      });
      if (this._lastValue) {
        this._urlbar.value = this._lastValue;
        delete this._lastValue;
      }
      gBrowser.tabContainer.addEventListener("TabSelect", this._urlbar);
    }
    return this._urlbar;
  },

  /**
   * Invoked by CustomizationHandler when a customization starts.
   */
  customizeStart() {
    if (this._urlbar) {
      this._urlbar.removeCopyCutController();
    }
  },

  /**
   * Invoked by CustomizationHandler when a customization ends.
   */
  customizeEnd() {
    this._reset();
  },

  /**
   *  Used to reset the gURLBar value.
   */
  _reset() {
    if (this._urlbar) {
      gBrowser.tabContainer.removeEventListener("TabSelect", this._urlbar);
      this._lastValue = this._urlbar.value;
      this._urlbar.uninit();
      delete this._urlbar;
      gURLBar = this.urlbar;
    }
  },
};

XPCOMUtils.defineLazyGetter(this, "ReferrerInfo", () =>
  Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  )
);

// High priority notification bars shown at the top of the window.
XPCOMUtils.defineLazyGetter(this, "gHighPriorityNotificationBox", () => {
  return new MozElements.NotificationBox(element => {
    element.classList.add("global-notificationbox");
    element.setAttribute("notificationside", "top");
    document.getElementById("appcontent").prepend(element);
  });
});

// Regular notification bars shown at the bottom of the window.
XPCOMUtils.defineLazyGetter(this, "gNotificationBox", () => {
  return new MozElements.NotificationBox(element => {
    element.classList.add("global-notificationbox");
    element.setAttribute("notificationside", "bottom");
    document.getElementById("browser-bottombox").appendChild(element);
  });
});

XPCOMUtils.defineLazyGetter(this, "InlineSpellCheckerUI", () => {
  let { InlineSpellChecker } = ChromeUtils.import(
    "resource://gre/modules/InlineSpellChecker.jsm"
  );
  return new InlineSpellChecker();
});

XPCOMUtils.defineLazyGetter(this, "PageMenuParent", () => {
  // eslint-disable-next-line no-shadow
  let { PageMenuParent } = ChromeUtils.import(
    "resource://gre/modules/PageMenu.jsm"
  );
  return new PageMenuParent();
});

XPCOMUtils.defineLazyGetter(this, "PopupNotifications", () => {
  // eslint-disable-next-line no-shadow
  let { PopupNotifications } = ChromeUtils.import(
    "resource://gre/modules/PopupNotifications.jsm"
  );
  try {
    // Hide all notifications while the URL is being edited and the address bar
    // has focus, including the virtual focus in the results popup.
    // We also have to hide notifications explicitly when the window is
    // minimized because of the effects of the "noautohide" attribute on Linux.
    // This can be removed once bug 545265 and bug 1320361 are fixed.
    let shouldSuppress = () => {
      return (
        window.windowState == window.STATE_MINIMIZED ||
        (gURLBar.getAttribute("pageproxystate") != "valid" && gURLBar.focused)
      );
    };
    return new PopupNotifications(
      gBrowser,
      document.getElementById("notification-popup"),
      document.getElementById("notification-popup-box"),
      { shouldSuppress }
    );
  } catch (ex) {
    Cu.reportError(ex);
    return null;
  }
});

XPCOMUtils.defineLazyGetter(this, "Win7Features", () => {
  if (AppConstants.platform != "win") {
    return null;
  }

  const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
  if (
    WINTASKBAR_CONTRACTID in Cc &&
    Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available
  ) {
    let { AeroPeek } = ChromeUtils.import(
      "resource:///modules/WindowsPreviewPerTab.jsm"
    );
    return {
      onOpenWindow() {
        AeroPeek.onOpenWindow(window);
        this.handledOpening = true;
      },
      onCloseWindow() {
        if (this.handledOpening) {
          AeroPeek.onCloseWindow(window);
        }
      },
      handledOpening: false,
    };
  }
  return null;
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gToolbarKeyNavEnabled",
  "browser.toolbars.keyboard_navigation",
  false,
  (aPref, aOldVal, aNewVal) => {
    if (aNewVal) {
      ToolbarKeyboardNavigator.init();
    } else {
      ToolbarKeyboardNavigator.uninit();
    }
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gFxaToolbarEnabled",
  "identity.fxaccounts.toolbar.enabled",
  false,
  (aPref, aOldVal, aNewVal) => {
    showFxaToolbarMenu(aNewVal);
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gFxaToolbarAccessed",
  "identity.fxaccounts.toolbar.accessed",
  false,
  (aPref, aOldVal, aNewVal) => {
    showFxaToolbarMenu(gFxaToolbarEnabled);
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gFxaSendLoginUrl",
  "identity.fxaccounts.service.sendLoginUrl",
  false,
  (aPref, aOldVal, aNewVal) => {
    showFxaToolbarMenu(gFxaToolbarEnabled);
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gFxaMonitorLoginUrl",
  "identity.fxaccounts.service.monitorLoginUrl",
  false,
  (aPref, aOldVal, aNewVal) => {
    showFxaToolbarMenu(gFxaToolbarEnabled);
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gMsgingSystemFxABadge",
  "browser.messaging-system.fxatoolbarbadge.enabled",
  true,
  (aPref, aOldVal, aNewVal) => {
    showFxaToolbarMenu(gFxaToolbarEnabled);
  }
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gHtmlAboutAddonsEnabled",
  "extensions.htmlaboutaddons.enabled",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gAddonAbuseReportEnabled",
  "extensions.abuseReport.enabled",
  false
);

customElements.setElementCreationCallback("translation-notification", () => {
  Services.scriptloader.loadSubScript(
    "chrome://browser/content/translation-notification.js",
    window
  );
});

var gBrowser;
var gLastValidURLStr = "";
var gInPrintPreviewMode = false;
var gContextMenu = null; // nsContextMenu instance
var gMultiProcessBrowser = window.docShell.QueryInterface(Ci.nsILoadContext)
  .useRemoteTabs;
var gFissionBrowser = window.docShell.QueryInterface(Ci.nsILoadContext)
  .useRemoteSubframes;

var gBrowserAllowScriptsToCloseInitialTabs = false;

if (AppConstants.platform != "macosx") {
  var gEditUIVisible = true;
}

// Smart getter for the findbar.  If you don't wish to force the creation of
// the findbar, check gFindBarInitialized first.

Object.defineProperty(this, "gFindBar", {
  enumerable: true,
  get() {
    return gBrowser.getCachedFindBar();
  },
});

Object.defineProperty(this, "gFindBarInitialized", {
  enumerable: true,
  get() {
    return gBrowser.isFindBarInitialized();
  },
});

Object.defineProperty(this, "gFindBarPromise", {
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

var gPageIcons = {
  "about:home": "chrome://branding/content/icon32.png",
  "about:newtab": "chrome://branding/content/icon32.png",
  "about:welcome": "chrome://branding/content/icon32.png",
  "about:newinstall": "chrome://branding/content/icon32.png",
  "about:privatebrowsing": "chrome://browser/skin/privatebrowsing/favicon.svg",
};

var gInitialPages = [
  "about:blank",
  "about:newtab",
  "about:home",
  "about:privatebrowsing",
  "about:welcomeback",
  "about:sessionrestore",
  "about:welcome",
  "about:newinstall",
];

function isInitialPage(url) {
  if (!(url instanceof Ci.nsIURI)) {
    try {
      url = Services.io.newURI(url);
    } catch (ex) {
      return false;
    }
  }

  let nonQuery = url.prePath + url.filePath;
  return gInitialPages.includes(nonQuery) || nonQuery == BROWSER_NEW_TAB_URL;
}

function browserWindows() {
  return Services.wm.getEnumerator("navigator:browser");
}

// This is a stringbundle-like interface to gBrowserBundle, formerly a getter for
// the "bundle_browser" element.
var gNavigatorBundle = {
  getString(key) {
    return gBrowserBundle.GetStringFromName(key);
  },
  getFormattedString(key, array) {
    return gBrowserBundle.formatStringFromName(key, array);
  },
};

function showFxaToolbarMenu(enable) {
  // We only show the Firefox Account toolbar menu if the feature is enabled and
  // if sync is enabled.
  const syncEnabled = Services.prefs.getBoolPref(
    "identity.fxaccounts.enabled",
    false
  );
  const mainWindowEl = document.documentElement;
  const fxaPanelEl = document.getElementById("PanelUI-fxa");

  mainWindowEl.setAttribute("fxastatus", "not_configured");
  fxaPanelEl.addEventListener("ViewShowing", gSync.updateSendToDeviceTitle);

  Services.telemetry.setEventRecordingEnabled("fxa_app_menu", true);

  if (enable && syncEnabled) {
    mainWindowEl.setAttribute("fxatoolbarmenu", "visible");

    // We have to manually update the sync state UI when toggling the FxA toolbar
    // because it could show an invalid icon if the user is logged in and no sync
    // event was performed yet.
    gSync.maybeUpdateUIState();

    Services.telemetry.setEventRecordingEnabled("fxa_avatar_menu", true);

    // We set an attribute here so that we can toggle the custom
    // badge depending on whether the FxA menu was ever accessed.
    // If badging is handled by Messaging System we shouldn't set
    // the attribute.
    if (!gFxaToolbarAccessed && !gMsgingSystemFxABadge) {
      mainWindowEl.setAttribute("fxa_avatar_badged", "badged");
    } else {
      mainWindowEl.removeAttribute("fxa_avatar_badged");
    }

    // When the pref for a FxA service is removed, we remove it from
    // the FxA toolbar menu as well. This is useful when the service
    // might not be available that browser.
    document.getElementById(
      "PanelUI-fxa-menu-send-button"
    ).hidden = !gFxaSendLoginUrl;
    document.getElementById(
      "PanelUI-fxa-menu-monitor-button"
    ).hidden = !gFxaMonitorLoginUrl;
    document.getElementById("fxa-menu-service-separator").hidden =
      !gFxaSendLoginUrl && !gFxaMonitorLoginUrl;
  } else {
    mainWindowEl.removeAttribute("fxatoolbarmenu");
  }
}

function UpdateBackForwardCommands(aWebNavigation) {
  var backCommand = document.getElementById("Browser:Back");
  var forwardCommand = document.getElementById("Browser:Forward");

  // Avoid setting attributes on commands if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = backCommand.hasAttribute("disabled");
  var forwardDisabled = forwardCommand.hasAttribute("disabled");
  if (backDisabled == aWebNavigation.canGoBack) {
    if (backDisabled) {
      backCommand.removeAttribute("disabled");
    } else {
      backCommand.setAttribute("disabled", true);
    }
  }

  if (forwardDisabled == aWebNavigation.canGoForward) {
    if (forwardDisabled) {
      forwardCommand.removeAttribute("disabled");
    } else {
      forwardCommand.setAttribute("disabled", true);
    }
  }
}

/**
 * Click-and-Hold implementation for the Back and Forward buttons
 * XXXmano: should this live in toolbarbutton.js?
 */
function SetClickAndHoldHandlers() {
  // Bug 414797: Clone the back/forward buttons' context menu into both buttons.
  let popup = document.getElementById("backForwardMenu").cloneNode(true);
  popup.removeAttribute("id");
  // Prevent the back/forward buttons' context attributes from being inherited.
  popup.setAttribute("context", "");

  let backButton = document.getElementById("back-button");
  backButton.setAttribute("type", "menu");
  backButton.prepend(popup);
  gClickAndHoldListenersOnElement.add(backButton);

  let forwardButton = document.getElementById("forward-button");
  popup = popup.cloneNode(true);
  forwardButton.setAttribute("type", "menu");
  forwardButton.prepend(popup);
  gClickAndHoldListenersOnElement.add(forwardButton);
}

const gClickAndHoldListenersOnElement = {
  _timers: new Map(),

  _mousedownHandler(aEvent) {
    if (
      aEvent.button != 0 ||
      aEvent.currentTarget.open ||
      aEvent.currentTarget.disabled
    ) {
      return;
    }

    // Prevent the menupopup from opening immediately
    aEvent.currentTarget.menupopup.hidden = true;

    aEvent.currentTarget.addEventListener("mouseout", this);
    aEvent.currentTarget.addEventListener("mouseup", this);
    this._timers.set(
      aEvent.currentTarget,
      setTimeout(b => this._openMenu(b), 500, aEvent.currentTarget)
    );
  },

  _clickHandler(aEvent) {
    if (
      aEvent.button == 0 &&
      aEvent.target == aEvent.currentTarget &&
      !aEvent.currentTarget.open &&
      !aEvent.currentTarget.disabled
    ) {
      let cmdEvent = document.createEvent("xulcommandevent");
      cmdEvent.initCommandEvent(
        "command",
        true,
        true,
        window,
        0,
        aEvent.ctrlKey,
        aEvent.altKey,
        aEvent.shiftKey,
        aEvent.metaKey,
        null,
        aEvent.mozInputSource
      );
      aEvent.currentTarget.dispatchEvent(cmdEvent);

      // This is here to cancel the XUL default event
      // dom.click() triggers a command even if there is a click handler
      // however this can now be prevented with preventDefault().
      aEvent.preventDefault();
    }
  },

  _openMenu(aButton) {
    this._cancelHold(aButton);
    aButton.firstElementChild.hidden = false;
    aButton.open = true;
  },

  _mouseoutHandler(aEvent) {
    let buttonRect = aEvent.currentTarget.getBoundingClientRect();
    if (
      aEvent.clientX >= buttonRect.left &&
      aEvent.clientX <= buttonRect.right &&
      aEvent.clientY >= buttonRect.bottom
    ) {
      this._openMenu(aEvent.currentTarget);
    } else {
      this._cancelHold(aEvent.currentTarget);
    }
  },

  _mouseupHandler(aEvent) {
    this._cancelHold(aEvent.currentTarget);
  },

  _cancelHold(aButton) {
    clearTimeout(this._timers.get(aButton));
    aButton.removeEventListener("mouseout", this);
    aButton.removeEventListener("mouseup", this);
  },

  _keypressHandler(aEvent) {
    if (aEvent.key == " " || aEvent.key == "Enter") {
      // Normally, command events get fired for keyboard activation. However,
      // we've set type="menu", so that doesn't happen. Handle this the same
      // way we handle clicks.
      aEvent.target.click();
    }
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
      case "keypress":
        this._keypressHandler(e);
        break;
    }
  },

  remove(aButton) {
    aButton.removeEventListener("mousedown", this, true);
    aButton.removeEventListener("click", this, true);
    aButton.removeEventListener("keypress", this, true);
  },

  add(aElm) {
    this._timers.delete(aElm);

    aElm.addEventListener("mousedown", this, true);
    aElm.addEventListener("click", this, true);
    aElm.addEventListener("keypress", this, true);
  },
};

const gSessionHistoryObserver = {
  observe(subject, topic, data) {
    if (topic != "browser:purge-session-history") {
      return;
    }

    var backCommand = document.getElementById("Browser:Back");
    backCommand.setAttribute("disabled", "true");
    var fwdCommand = document.getElementById("Browser:Forward");
    fwdCommand.setAttribute("disabled", "true");

    // Clear undo history of the URL bar
    gURLBar.editor.transactionManager.clear();
  },
};

const gStoragePressureObserver = {
  _lastNotificationTime: -1,

  async observe(subject, topic, data) {
    if (topic != "QuotaManager::StoragePressure") {
      return;
    }

    const NOTIFICATION_VALUE = "storage-pressure-notification";
    if (
      gHighPriorityNotificationBox.getNotificationWithValue(NOTIFICATION_VALUE)
    ) {
      // Do not display the 2nd notification when there is already one
      return;
    }

    // Don't display notification twice within the given interval.
    // This is because
    //   - not to annoy user
    //   - give user some time to clean space.
    //     Even user sees notification and starts acting, it still takes some time.
    const MIN_NOTIFICATION_INTERVAL_MS = Services.prefs.getIntPref(
      "browser.storageManager.pressureNotification.minIntervalMS"
    );
    let duration = Date.now() - this._lastNotificationTime;
    if (duration <= MIN_NOTIFICATION_INTERVAL_MS) {
      return;
    }
    this._lastNotificationTime = Date.now();

    MozXULElement.insertFTLIfNeeded("branding/brand.ftl");
    MozXULElement.insertFTLIfNeeded("browser/preferences/preferences.ftl");

    const BYTES_IN_GIGABYTE = 1073741824;
    const USAGE_THRESHOLD_BYTES =
      BYTES_IN_GIGABYTE *
      Services.prefs.getIntPref(
        "browser.storageManager.pressureNotification.usageThresholdGB"
      );
    let msg = "";
    let buttons = [];
    let usage = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    buttons.push({
      "l10n-id": "space-alert-learn-more-button",
      callback(notificationBar, button) {
        let learnMoreURL =
          Services.urlFormatter.formatURLPref("app.support.baseURL") +
          "storage-permissions";
        // This is a content URL, loaded from trusted UX.
        openTrustedLinkIn(learnMoreURL, "tab");
      },
    });
    if (usage < USAGE_THRESHOLD_BYTES) {
      // The firefox-used space < 5GB, then warn user to free some disk space.
      // This is because this usage is small and not the main cause for space issue.
      // In order to avoid the bad and wrong impression among users that
      // firefox eats disk space a lot, indicate users to clean up other disk space.
      [msg] = await document.l10n.formatValues([
        { id: "space-alert-under-5gb-message" },
      ]);
      buttons.push({
        "l10n-id": "space-alert-under-5gb-ok-button",
        callback() {},
      });
    } else {
      // The firefox-used space >= 5GB, then guide users to about:preferences
      // to clear some data stored on firefox by websites.
      [msg] = await document.l10n.formatValues([
        { id: "space-alert-over-5gb-message" },
      ]);
      buttons.push({
        "l10n-id": "space-alert-over-5gb-pref-button",
        callback(notificationBar, button) {
          // The advanced subpanes are only supported in the old organization, which will
          // be removed by bug 1349689.
          openPreferences("privacy-sitedata");
        },
      });
    }

    gHighPriorityNotificationBox.appendNotification(
      msg,
      NOTIFICATION_VALUE,
      null,
      gHighPriorityNotificationBox.PRIORITY_WARNING_HIGH,
      buttons,
      null
    );

    // This seems to be necessary to get the buttons to display correctly
    // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1504216
    document.l10n.translateFragment(
      gHighPriorityNotificationBox.currentNotification
    );
  },
};

var gPopupBlockerObserver = {
  handleEvent(aEvent) {
    if (aEvent.originalTarget != gBrowser.selectedBrowser) {
      return;
    }

    gIdentityHandler.refreshIdentityBlock();

    if (
      !gBrowser.selectedBrowser.blockedPopups ||
      !gBrowser.selectedBrowser.blockedPopups.length
    ) {
      // Hide the notification box (if it's visible).
      let notificationBox = gBrowser.getNotificationBox();
      let notification = notificationBox.getNotificationWithValue(
        "popup-blocked"
      );
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

        var stringKey =
          AppConstants.platform == "win"
            ? "popupWarningButton"
            : "popupWarningButtonUnix";

        var popupButtonText = gNavigatorBundle.getString(stringKey);
        var popupButtonAccesskey = gNavigatorBundle.getString(
          stringKey + ".accesskey"
        );

        let messageBase;
        if (popupCount < this.maxReportedPopups) {
          messageBase = gNavigatorBundle.getString("popupWarning.message");
        } else {
          messageBase = gNavigatorBundle.getString(
            "popupWarning.exceeded.message"
          );
        }

        var message = PluralForm.get(popupCount, messageBase)
          .replace("#1", brandShortName)
          .replace("#2", popupCount);

        let notificationBox = gBrowser.getNotificationBox();
        let notification = notificationBox.getNotificationWithValue(
          "popup-blocked"
        );
        if (notification) {
          notification.label = message;
        } else {
          var buttons = [
            {
              label: popupButtonText,
              accessKey: popupButtonAccesskey,
              popup: "blockedPopupOptions",
              callback: null,
            },
          ];

          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          notificationBox.appendNotification(
            message,
            "popup-blocked",
            "chrome://browser/skin/notification-icons/popup.svg",
            priority,
            buttons
          );
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

    if (!shouldBlock) {
      this.showAllBlockedPopups(gBrowser.selectedBrowser);
    }

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
    var blockedPopupAllowSite = document.getElementById(
      "blockedPopupAllowSite"
    );
    try {
      blockedPopupAllowSite.removeAttribute("hidden");
      let uriHost = uri.asciiHost ? uri.host : uri.spec;
      var pm = Services.perms;
      if (pm.testPermission(uri, "popup") == pm.ALLOW_ACTION) {
        // Offer an item to block popups for this site, if a whitelist entry exists
        // already for it.
        let blockString = gNavigatorBundle.getFormattedString("popupBlock", [
          uriHost,
        ]);
        blockedPopupAllowSite.setAttribute("label", blockString);
        blockedPopupAllowSite.setAttribute("block", "true");
      } else {
        // Offer an item to allow popups for this site
        let allowString = gNavigatorBundle.getFormattedString("popupAllow", [
          uriHost,
        ]);
        blockedPopupAllowSite.setAttribute("label", allowString);
        blockedPopupAllowSite.removeAttribute("block");
      }
    } catch (e) {
      blockedPopupAllowSite.setAttribute("hidden", "true");
    }

    if (PrivateBrowsingUtils.isWindowPrivate(window)) {
      blockedPopupAllowSite.setAttribute("disabled", "true");
    } else {
      blockedPopupAllowSite.removeAttribute("disabled");
    }

    let blockedPopupDontShowMessage = document.getElementById(
      "blockedPopupDontShowMessage"
    );
    let showMessage = Services.prefs.getBoolPref(
      "privacy.popups.showBrowserMessage"
    );
    blockedPopupDontShowMessage.setAttribute("checked", !showMessage);
    blockedPopupDontShowMessage.setAttribute(
      "label",
      gNavigatorBundle.getString("popupWarningDontShowFromMessage")
    );

    let blockedPopupsSeparator = document.getElementById(
      "blockedPopupsSeparator"
    );
    blockedPopupsSeparator.setAttribute("hidden", true);

    gBrowser.selectedBrowser
      .retrieveListOfBlockedPopups()
      .then(blockedPopups => {
        let foundUsablePopupURI = false;
        if (blockedPopups) {
          for (let i = 0; i < blockedPopups.length; i++) {
            let blockedPopup = blockedPopups[i];

            // popupWindowURI will be null if the file picker popup is blocked.
            // xxxdz this should make the option say "Show file picker" and do it (Bug 590306)
            if (!blockedPopup.popupWindowURIspec) {
              continue;
            }

            var popupURIspec = blockedPopup.popupWindowURIspec;

            // Sometimes the popup URI that we get back from the blockedPopup
            // isn't useful (for instance, netscape.com's popup URI ends up
            // being "http://www.netscape.com", which isn't really the URI of
            // the popup they're trying to show).  This isn't going to be
            // useful to the user, so we won't create a menu item for it.
            if (
              popupURIspec == "" ||
              popupURIspec == "about:blank" ||
              popupURIspec == "<self>" ||
              popupURIspec == uri.spec
            ) {
              continue;
            }

            // Because of the short-circuit above, we may end up in a situation
            // in which we don't have any usable popup addresses to show in
            // the menu, and therefore we shouldn't show the separator.  However,
            // since we got past the short-circuit, we must've found at least
            // one usable popup URI and thus we'll turn on the separator later.
            foundUsablePopupURI = true;

            var menuitem = document.createXULElement("menuitem");
            var label = gNavigatorBundle.getFormattedString(
              "popupShowPopupPrefix",
              [popupURIspec]
            );
            menuitem.setAttribute("label", label);
            menuitem.setAttribute(
              "oncommand",
              "gPopupBlockerObserver.showBlockedPopup(event);"
            );
            menuitem.setAttribute("popupReportIndex", i);
            menuitem.popupReportBrowser = browser;
            aEvent.target.appendChild(menuitem);
          }
        }

        // Show the separator if we added any
        // showable popup addresses to the menu.
        if (foundUsablePopupURI) {
          blockedPopupsSeparator.removeAttribute("hidden");
        }
      }, null);
  },

  onPopupHiding(aEvent) {
    let item = aEvent.target.lastElementChild;
    while (item && item.id != "blockedPopupsSeparator") {
      let next = item.previousElementSibling;
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
        if (popups[i].popupWindowURIspec) {
          aBrowser.unblockPopup(i);
        }
      }
    }, null);
  },

  editPopupSettings() {
    openPreferences("privacy-permissions-block-popups");
  },

  dontShowMessage() {
    var showMessage = Services.prefs.getBoolPref(
      "privacy.popups.showBrowserMessage"
    );
    Services.prefs.setBoolPref(
      "privacy.popups.showBrowserMessage",
      !showMessage
    );
    gBrowser.getNotificationBox().removeCurrentNotification();
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  gPopupBlockerObserver,
  "maxReportedPopups",
  "privacy.popups.maxReported"
);

function gKeywordURIFixup({ target: browser, data: fixupInfo }) {
  let deserializeURI = spec => (spec ? makeURI(spec) : null);

  // We get called irrespective of whether we did a keyword search, or
  // whether the original input would be vaguely interpretable as a URL,
  // so figure that out first.
  let alternativeURI = deserializeURI(fixupInfo.fixedURI);
  if (
    !fixupInfo.keywordProviderName ||
    !alternativeURI ||
    !alternativeURI.host
  ) {
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
  if (isIPv4Address(asciiHost) || /^(?:\d+|0x[a-f0-9]+)$/i.test(asciiHost)) {
    return;
  }

  let onLookupCompleteListener = {
    onLookupComplete(request, record, status) {
      let browserRef = weakBrowser.get();
      if (!Components.isSuccessCode(status) || !browserRef) {
        return;
      }

      let currentURI = browserRef.currentURI;
      // If we're in case (3) (see above), don't show an info bar.
      if (!currentURI.equals(previousURI) && !currentURI.equals(preferredURI)) {
        return;
      }

      // show infobar offering to visit the host
      let notificationBox = gBrowser.getNotificationBox(browserRef);
      if (notificationBox.getNotificationWithValue("keyword-uri-fixup")) {
        return;
      }

      let message = gNavigatorBundle.getFormattedString(
        "keywordURIFixup.message",
        [hostName]
      );
      let yesMessage = gNavigatorBundle.getFormattedString(
        "keywordURIFixup.goTo",
        [hostName]
      );

      let buttons = [
        {
          label: yesMessage,
          accessKey: gNavigatorBundle.getString(
            "keywordURIFixup.goTo.accesskey"
          ),
          callback() {
            // Do not set this preference while in private browsing.
            if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
              let pref = "browser.fixup.domainwhitelist." + asciiHost;
              Services.prefs.setBoolPref(pref, true);
            }
            openTrustedLinkIn(alternativeURI.spec, "current");
          },
        },
        {
          label: gNavigatorBundle.getString("keywordURIFixup.dismiss"),
          accessKey: gNavigatorBundle.getString(
            "keywordURIFixup.dismiss.accesskey"
          ),
          callback() {
            let notification = notificationBox.getNotificationWithValue(
              "keyword-uri-fixup"
            );
            notificationBox.removeNotification(notification, true);
          },
        },
      ];
      let notification = notificationBox.appendNotification(
        message,
        "keyword-uri-fixup",
        null,
        notificationBox.PRIORITY_INFO_HIGH,
        buttons
      );
      notification.persistence = 1;
    },
  };

  try {
    gDNSService.asyncResolve(
      hostName,
      0,
      onLookupCompleteListener,
      Services.tm.mainThread,
      contentPrincipal.originAttributes
    );
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
      let mimeType = Cc["@mozilla.org/mime;1"]
        .getService(Ci.nsIMIMEService)
        .getTypeFromURI(aUri);
      if (mimeType == "application/x-xpinstall") {
        let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
        AddonManager.getInstallForURL(aUri.spec, {
          telemetryInfo: { source: "file-url" },
        }).then(install => {
          AddonManager.installAddonFromWebpage(
            mimeType,
            aBrowser,
            systemPrincipal,
            install
          );
        });
        return true;
      }
    } catch (e) {
      return false;
    }
  }

  return false;
}

/* Creates a null principal using the userContextId
   from the current selected tab or a passed in tab argument */
function _createNullPrincipalFromTabUserContextId(tab = gBrowser.selectedTab) {
  let userContextId;
  if (tab.hasAttribute("usercontextid")) {
    userContextId = tab.getAttribute("usercontextid");
  }
  return Services.scriptSecurityManager.createNullPrincipal({
    userContextId,
  });
}

// A shared function used by both remote and non-remote browser XBL bindings to
// load a URI or redirect it to the correct process.
function _loadURI(browser, uri, params = {}) {
  if (!uri) {
    uri = "about:blank";
  }

  let { triggeringPrincipal, referrerInfo, postData, userContextId, csp } =
    params || {};
  let loadFlags =
    params.loadFlags || params.flags || Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

  if (!triggeringPrincipal) {
    throw new Error("Must load with a triggering Principal");
  }

  let {
    uriObject,
    requiredRemoteType,
    mustChangeProcess,
    newFrameloader,
  } = E10SUtils.shouldLoadURIInBrowser(
    browser,
    uri,
    gMultiProcessBrowser,
    gFissionBrowser,
    loadFlags
  );
  if (uriObject && handleUriInChrome(browser, uriObject)) {
    // If we've handled the URI in Chrome then just return here.
    return;
  }
  if (newFrameloader) {
    // If a new frameloader is needed for process reselection because this used
    // to be a preloaded browser, clear the preloaded state now.
    browser.removeAttribute("preloadedState");
  }

  // !requiredRemoteType means we're loading in the parent/this process.
  if (!requiredRemoteType) {
    browser.isNavigating = true;
  }
  let loadURIOptions = {
    triggeringPrincipal,
    csp,
    loadFlags,
    referrerInfo,
    postData,
  };
  try {
    if (!mustChangeProcess) {
      if (userContextId) {
        browser.webNavigation.setOriginAttributesBeforeLoading({
          userContextId,
          privateBrowsingId: PrivateBrowsingUtils.isBrowserPrivate(browser)
            ? 1
            : 0,
        });
      }
      browser.webNavigation.loadURI(uri, loadURIOptions);
    } else {
      // Check if the current browser is allowed to unload.
      let { permitUnload, timedOut } = browser.permitUnload();
      if (!timedOut && !permitUnload) {
        return;
      }

      if (postData) {
        postData = serializeInputStream(postData);
      }

      let loadParams = {
        uri,
        triggeringPrincipal: triggeringPrincipal
          ? E10SUtils.serializePrincipal(triggeringPrincipal)
          : null,
        flags: loadFlags,
        referrerInfo: E10SUtils.serializeReferrerInfo(referrerInfo),
        remoteType: requiredRemoteType,
        postData,
        newFrameloader,
        csp: csp ? gSerializationHelper.serializeToString(csp) : null,
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
        browser.webNavigation.setOriginAttributesBeforeLoading({
          userContextId,
          privateBrowsingId: PrivateBrowsingUtils.isBrowserPrivate(browser)
            ? 1
            : 0,
        });
      }
      browser.webNavigation.loadURI(uri, loadURIOptions);
    } else {
      throw e;
    }
  } finally {
    if (!requiredRemoteType) {
      browser.isNavigating = false;
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
    data.loadOptions.remoteType = E10SUtils.getRemoteTypeForURI(
      data.loadOptions.uri,
      gMultiProcessBrowser,
      gFissionBrowser
    );
  }

  // We should only start the redirection if the browser window has finished
  // starting up. Otherwise, we should wait until the startup is done.
  if (gBrowserInit.delayedStartupFinished) {
    LoadInOtherProcess(browser, data.loadOptions, data.historyIndex);
  } else {
    let delayedStartupFinished = (subject, topic) => {
      if (topic == "browser-delayed-startup-finished" && subject == window) {
        Services.obs.removeObserver(delayedStartupFinished, topic);
        LoadInOtherProcess(browser, data.loadOptions, data.historyIndex);
      }
    };
    Services.obs.addObserver(
      delayedStartupFinished,
      "browser-delayed-startup-finished"
    );
  }
}

let _resolveDelayedStartup;
var delayedStartupPromise = new Promise(resolve => {
  _resolveDelayedStartup = resolve;
});

var gBrowserInit = {
  delayedStartupFinished: false,
  idleTasksFinishedPromise: null,
  idleTaskPromiseResolve: null,

  _tabToAdopt: undefined,

  getTabToAdopt() {
    if (this._tabToAdopt !== undefined) {
      return this._tabToAdopt;
    }

    if (window.arguments && window.arguments[0] instanceof window.XULElement) {
      this._tabToAdopt = window.arguments[0];

      // Clear the reference of the tab being adopted from the arguments.
      window.arguments[0] = null;
    } else {
      // There was no tab to adopt in the arguments, set _tabToAdopt to null
      // to avoid checking it again.
      this._tabToAdopt = null;
    }

    return this._tabToAdopt;
  },

  _clearTabToAdopt() {
    this._tabToAdopt = null;
  },

  // Used to check if the new window is still adopting an existing tab as its first tab
  // (e.g. from the WebExtensions internals).
  isAdoptingTab() {
    return !!this.getTabToAdopt();
  },

  onBeforeInitialXULLayout() {
    // Set a sane starting width/height for all resolutions on new profiles.
    if (Services.prefs.getBoolPref("privacy.resistFingerprinting")) {
      // When the fingerprinting resistance is enabled, making sure that we don't
      // have a maximum window to interfere with generating rounded window dimensions.
      document.documentElement.setAttribute("sizemode", "normal");
    } else if (!document.documentElement.hasAttribute("width")) {
      const TARGET_WIDTH = 1280;
      const TARGET_HEIGHT = 1040;
      let width = Math.min(screen.availWidth * 0.9, TARGET_WIDTH);
      let height = Math.min(screen.availHeight * 0.9, TARGET_HEIGHT);

      document.documentElement.setAttribute("width", width);
      document.documentElement.setAttribute("height", height);

      if (width < TARGET_WIDTH && height < TARGET_HEIGHT) {
        document.documentElement.setAttribute("sizemode", "maximized");
      }
    }

    // Run menubar initialization first, to avoid TabsInTitlebar code picking
    // up mutations from it and causing a reflow.
    AutoHideMenubar.init();
    // Update the chromemargin attribute so the window can be sized correctly.
    window.TabBarVisibility.update();
    TabsInTitlebar.init();

    new LightweightThemeConsumer(document);

    if (AppConstants.platform == "win") {
      if (
        window.matchMedia("(-moz-os-version: windows-win8)").matches &&
        window.matchMedia("(-moz-windows-default-theme)").matches
      ) {
        let windowFrameColor = new Color(
          ...ChromeUtils.import(
            "resource:///modules/Windows8WindowFrameColor.jsm",
            {}
          ).Windows8WindowFrameColor.get()
        );
        // Default to black for foreground text.
        if (!windowFrameColor.isContrastRatioAcceptable(new Color(0, 0, 0))) {
          document.documentElement.setAttribute("darkwindowframe", "true");
        }
      } else if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
        TelemetryEnvironment.onInitialized().then(() => {
          // 17763 is the build number of Windows 10 version 1809
          if (
            TelemetryEnvironment.currentEnvironment.system.os
              .windowsBuildNumber < 17763
          ) {
            document.documentElement.setAttribute(
              "always-use-accent-color-for-window-border",
              ""
            );
          }
        });
      }
    }

    if (
      Services.prefs.getBoolPref(
        "toolkit.legacyUserProfileCustomizations.windowIcon",
        false
      )
    ) {
      document.documentElement.setAttribute("icon", "main-window");
    }

    // Call this after we set attributes that might change toolbars' computed
    // text color.
    ToolbarIconColor.init();
  },

  onDOMContentLoaded() {
    // This needs setting up before we create the first remote browser.
    window.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIXULWindow).XULBrowserWindow = window.XULBrowserWindow;
    window.browserDOMWindow = new nsBrowserAccess();

    gBrowser = window._gBrowser;
    delete window._gBrowser;
    gBrowser.init();

    BrowserWindowTracker.track(window);

    gNavToolbox.palette = document.getElementById("BrowserToolbarPalette");
    gNavToolbox.palette.remove();
    let areas = CustomizableUI.areas;
    areas.splice(areas.indexOf(CustomizableUI.AREA_FIXED_OVERFLOW_PANEL), 1);
    for (let area of areas) {
      let node = document.getElementById(area);
      CustomizableUI.registerToolbarNode(node);
    }
    BrowserSearch.initPlaceHolder();

    // Hack to ensure that the various initial pages favicon is loaded
    // instantaneously, to avoid flickering and improve perceived performance.
    this._callWithURIToLoad(uriToLoad => {
      let url;
      try {
        url = Services.io.newURI(uriToLoad);
      } catch (e) {
        return;
      }
      let nonQuery = url.prePath + url.filePath;
      if (nonQuery in gPageIcons) {
        gBrowser.setIcon(gBrowser.selectedTab, gPageIcons[nonQuery]);
      }
    });

    this._setInitialFocus();

    showFxaToolbarMenu(gFxaToolbarEnabled);
  },

  onLoad() {
    gBrowser.addEventListener("DOMUpdateBlockedPopups", gPopupBlockerObserver);

    window.addEventListener("AppCommand", HandleAppCommandEvent, true);

    // These routines add message listeners. They must run before
    // loading the frame script to ensure that we don't miss any
    // message sent between when the frame script is loaded and when
    // the listener is registered.
    DOMEventHandler.init();
    gPageStyleMenu.init();
    LanguageDetectionListener.init();
    BrowserOnClick.init();
    CaptivePortalWatcher.init();
    ZoomUI.init(window);

    let mm = window.getGroupMessageManager("browsers");
    mm.loadFrameScript("chrome://browser/content/tab-content.js", true, true);
    mm.loadFrameScript("chrome://browser/content/content.js", true, true);
    mm.loadFrameScript(
      "chrome://global/content/content-HybridContentTelemetry.js",
      true
    );

    window.messageManager.addMessageListener("Browser:LoadURI", RedirectLoad);

    if (!gMultiProcessBrowser) {
      // There is a Content:Click message manually sent from content.
      Services.els.addSystemEventListener(
        gBrowser.tabpanels,
        "click",
        contentAreaClick,
        true
      );
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
      gURLBar.readOnly = true;
    }

    // Misc. inits.
    gUIDensity.init();
    TabletModeUpdater.init();
    CombinedStopReload.ensureInitialized();
    gPrivateBrowsingUI.init();
    BrowserSearch.init();
    BrowserPageActions.init();
    gAccessibilityServiceIndicator.init();
    AccessibilityRefreshBlocker.init();
    if (gToolbarKeyNavEnabled) {
      ToolbarKeyboardNavigator.init();
    }

    gRemoteControl.updateVisualCue(Marionette.running);

    // If we are given a tab to swap in, take care of it before first paint to
    // avoid an about:blank flash.
    let tabToAdopt = this.getTabToAdopt();
    if (tabToAdopt) {
      let evt = new CustomEvent("before-initial-tab-adopted", {
        bubbles: true,
      });
      gBrowser.tabpanels.dispatchEvent(evt);

      // Stop the about:blank load
      gBrowser.stop();
      // make sure it has a docshell
      gBrowser.docShell;

      // Remove the speculative focus from the urlbar to let the url be formatted.
      gURLBar.removeAttribute("focused");

      try {
        gBrowser.swapBrowsersAndCloseOther(gBrowser.selectedTab, tabToAdopt);
      } catch (e) {
        Cu.reportError(e);
      }

      // Clear the reference to the tab once its adoption has been completed.
      this._clearTabToAdopt();
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
    let reloadHistogram = Services.telemetry.getHistogramById(
      "FX_PAGE_RELOAD_KEY_COMBO"
    );
    let reloadCommand = document.getElementById("Browser:Reload");
    reloadCommand.addEventListener("command", function(event) {
      let { target } = event.sourceEvent || {};
      if (target.getAttribute("keycode") == "VK_F5") {
        reloadHistogram.add("only_f5", 1);
      } else if (target.id == "key_reload") {
        reloadHistogram.add("accel_reloadKey", 1);
      }
    });

    let reloadSkipCacheCommand = document.getElementById(
      "Browser:ReloadSkipCache"
    );
    reloadSkipCacheCommand.addEventListener("command", function(event) {
      let { target } = event.sourceEvent || {};
      if (target.getAttribute("keycode") == "VK_F5") {
        reloadHistogram.add("ctrl_f5", 1);
      } else if (target.id == "key_reload_skip_cache") {
        reloadHistogram.add("accel_shift_reload", 1);
      }
    });

    let reloadOrDuplicateCommand = document.getElementById(
      "Browser:ReloadOrDuplicate"
    );
    reloadOrDuplicateCommand.addEventListener("command", function(event) {
      let { target } = event.sourceEvent || {};
      if (target.id == "reload-button") {
        let accelKeyPressed =
          AppConstants.platform == "macosx" ? event.metaKey : event.ctrlKey;
        let auxiliaryPressed = false;
        let { sourceEvent } = event.sourceEvent || {};
        if (sourceEvent) {
          auxiliaryPressed = sourceEvent.button == 1;
        }
        if (auxiliaryPressed) {
          reloadHistogram.add("auxiliary_toolbar", 1);
        } else if (accelKeyPressed) {
          reloadHistogram.add("accel_toolbar", 1);
        } else if (event.shiftKey) {
          reloadHistogram.add("shift_toolbar", 1);
        } else {
          reloadHistogram.add("toolbar", 1);
        }
      }
    });
  },

  _cancelDelayedStartup() {
    window.removeEventListener("MozAfterPaint", this._boundDelayedStartup);
    this._boundDelayedStartup = null;
  },

  _delayedStartup() {
    let { TelemetryTimestamps } = ChromeUtils.import(
      "resource://gre/modules/TelemetryTimestamps.jsm"
    );
    TelemetryTimestamps.add("delayedStartupStarted");

    this._cancelDelayedStartup();

    // Bug 1531854 - The hidden window is force-created here
    // until all of its dependencies are handled.
    Services.appShell.hiddenDOMWindow;

    // We need to set the OfflineApps message listeners up before we
    // load homepages, which might need them.
    OfflineApps.init();

    gBrowser.addEventListener(
      "InsecureLoginFormsStateChange",
      function() {
        gIdentityHandler.refreshForInsecureLoginForms();
      },
      true
    );

    gBrowser.addEventListener(
      "PermissionStateChange",
      function() {
        gIdentityHandler.refreshIdentityBlock();
      },
      true
    );

    // Get the service so that it initializes and registers listeners for new
    // tab pages in order to be ready for any early-loading about:newtab pages,
    // e.g., start/home page, command line / startup uris to load, sessionstore
    gAboutNewTabService.QueryInterface(Ci.nsISupports);

    this._handleURIToLoad();

    Services.obs.addObserver(gIdentityHandler, "perm-changed");
    Services.obs.addObserver(gRemoteControl, "remote-active");
    Services.obs.addObserver(
      gSessionHistoryObserver,
      "browser:purge-session-history"
    );
    Services.obs.addObserver(
      gStoragePressureObserver,
      "QuotaManager::StoragePressure"
    );
    Services.obs.addObserver(gXPInstallObserver, "addon-install-disabled");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-started");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-blocked");
    Services.obs.addObserver(
      gXPInstallObserver,
      "addon-install-origin-blocked"
    );
    Services.obs.addObserver(gXPInstallObserver, "addon-install-failed");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-confirmation");
    Services.obs.addObserver(gXPInstallObserver, "addon-install-complete");
    window.messageManager.addMessageListener(
      "Browser:URIFixup",
      gKeywordURIFixup
    );

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
    BrowserSearch.delayedStartupInit();
    AutoShowBookmarksToolbar.init();
    gProtectionsHandler.init();
    HomePage.init().catch(Cu.reportError);

    let safeMode = document.getElementById("helpSafeMode");
    if (Services.appinfo.inSafeMode) {
      document.l10n.setAttributes(safeMode, "menu-help-safe-mode-with-addons");
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
    if (!Services.prefs.getBoolPref("ui.click_hold_context_menus", false)) {
      SetClickAndHoldHandlers();
    }

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

    FullScreen.init();

    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      MenuTouchModeObserver.init();
    }

    if (AppConstants.MOZ_DATA_REPORTING) {
      gDataNotificationInfoBar.init();
    }

    if (!AppConstants.MOZILLA_OFFICIAL) {
      DevelopmentHelpers.init();
    }

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

    gBrowser.tabContainer.addEventListener("TabSelect", function() {
      for (let panel of document.querySelectorAll(
        "panel[tabspecific='true']"
      )) {
        if (panel.state == "open") {
          panel.hidePopup();
        }
      }
    });

    CaptivePortalWatcher.delayedStartup();

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

    this._firstBrowserPaintDeferred = {};
    this._firstBrowserPaintDeferred.promise = new Promise(resolve => {
      this._firstBrowserPaintDeferred.resolve = resolve;
    });

    let mm = window.messageManager;
    let initialBrowser = gBrowser.selectedBrowser;
    mm.addMessageListener(
      "Browser:FirstNonBlankPaint",
      function onFirstNonBlankPaint() {
        mm.removeMessageListener(
          "Browser:FirstNonBlankPaint",
          onFirstNonBlankPaint
        );
        initialBrowser.removeAttribute("blank");
      }
    );

    // To prevent flickering of the urlbar-history-dropmarker in the general
    // case, the urlbar has the 'focused' attribute set by default.
    // If we are not fully sure the urlbar will be focused in this window,
    // we should remove the attribute before first paint.
    let shouldRemoveFocusedAttribute = true;
    this._callWithURIToLoad(uriToLoad => {
      if (isBlankPageURL(uriToLoad) || uriToLoad == "about:privatebrowsing") {
        focusAndSelectUrlBar();
        shouldRemoveFocusedAttribute = false;
        return;
      }

      if (gBrowser.selectedBrowser.isRemoteBrowser) {
        // If the initial browser is remote, in order to optimize for first paint,
        // we'll defer switching focus to that browser until it has painted.
        this._firstBrowserPaintDeferred.promise.then(() => {
          // If focus didn't move while we were waiting for first paint, we're okay
          // to move to the browser.
          if (
            document.commandDispatcher.focusedElement == initiallyFocusedElement
          ) {
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
    // invalidating styles multiple times in a row if uriToLoadPromise
    // resolves before first paint.
    if (shouldRemoveFocusedAttribute) {
      window.requestAnimationFrame(() => {
        if (shouldRemoveFocusedAttribute) {
          gURLBar.removeAttribute("focused");
        }
      });
    }
  },

  _handleURIToLoad() {
    this._callWithURIToLoad(uriToLoad => {
      if (!uriToLoad) {
        // We don't check whether window.arguments[5] (userContextId) is set
        // because tabbrowser.js takes care of that for the initial tab.
        return;
      }

      // We don't check if uriToLoad is a XULElement because this case has
      // already been handled before first paint, and the argument cleared.
      if (Array.isArray(uriToLoad)) {
        // This function throws for certain malformed URIs, so use exception handling
        // so that we don't disrupt startup
        try {
          gBrowser.loadTabs(uriToLoad, {
            inBackground: false,
            replace: true,
            // See below for the semantics of window.arguments. Only the minimum is supported.
            userContextId: window.arguments[5],
            triggeringPrincipal:
              window.arguments[8] ||
              Services.scriptSecurityManager.getSystemPrincipal(),
            allowInheritPrincipal: window.arguments[9],
            csp: window.arguments[10],
            fromExternal: true,
          });
        } catch (e) {}
      } else if (window.arguments.length >= 3) {
        // window.arguments[1]: unused (bug 871161)
        //                 [2]: referrerInfo (nsIReferrerInfo)
        //                 [3]: postData (nsIInputStream)
        //                 [4]: allowThirdPartyFixup (bool)
        //                 [5]: userContextId (int)
        //                 [6]: originPrincipal (nsIPrincipal)
        //                 [7]: originStoragePrincipal (nsIPrincipal)
        //                 [8]: triggeringPrincipal (nsIPrincipal)
        //                 [9]: allowInheritPrincipal (bool)
        //                 [10]: csp (nsIContentSecurityPolicy)
        let userContextId =
          window.arguments[5] != undefined
            ? window.arguments[5]
            : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
        loadURI(
          uriToLoad,
          window.arguments[2] || null,
          window.arguments[3] || null,
          window.arguments[4] || false,
          userContextId,
          // pass the origin principal (if any) and force its use to create
          // an initial about:blank viewer if present:
          window.arguments[6],
          window.arguments[7],
          !!window.arguments[6],
          window.arguments[8],
          // TODO fix allowInheritPrincipal to default to false.
          // Default to true unless explicitly set to false because of bug 1475201.
          window.arguments[9] !== false,
          window.arguments[10]
        );
        window.focus();
      } else {
        // Note: loadOneOrMoreURIs *must not* be called if window.arguments.length >= 3.
        // Such callers expect that window.arguments[0] is handled as a single URI.
        loadOneOrMoreURIs(
          uriToLoad,
          Services.scriptSecurityManager.getSystemPrincipal(),
          null
        );
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
   * have run (from _scheduleStartupIdleTasks in BrowserGlue.jsm).
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
      // Initialize the all tabs menu
      gTabsPanel.init();
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

    // Show the addons private browsing panel the first time a private window.
    scheduleIdleTask(() => {
      ExtensionsUI.showPrivateBrowsingNotification(window);
    });

    scheduleIdleTask(
      () => {
        // Initialize the download manager some time after the app starts so that
        // auto-resume downloads begin (such as after crashing or quitting with
        // active downloads) and speeds up the first-load of the download manager UI.
        // If the user manually opens the download manager before the timeout, the
        // downloads will start right away, and initializing again won't hurt.
        try {
          DownloadsCommon.initializeAllDataLinks();
          ChromeUtils.import(
            "resource:///modules/DownloadsTaskbar.jsm",
            {}
          ).DownloadsTaskbar.registerIndicator(window);
          if (AppConstants.platform == "macosx") {
            ChromeUtils.import(
              "resource:///modules/DownloadsMacFinderProgress.jsm"
            ).DownloadsMacFinderProgress.register();
          }
        } catch (ex) {
          Cu.reportError(ex);
        }
      },
      { timeout: 10000 }
    );

    if (Win7Features) {
      scheduleIdleTask(() => Win7Features.onOpenWindow());
    }

    scheduleIdleTask(async () => {
      NewTabPagePreloading.maybeCreatePreloadedBrowser(window);
    });

    // This should always go last, since the idle tasks (except for the ones with
    // timeouts) should execute in order. Note that this observer notification is
    // not guaranteed to fire, since the window could close before we get here.
    scheduleIdleTask(() => {
      this.idleTaskPromiseResolve();
      Services.obs.notifyObservers(
        window,
        "browser-idle-startup-tasks-finished"
      );
    });
  },

  // Returns the URI(s) to load at startup if it is immediately known, or a
  // promise resolving to the URI to load.
  get uriToLoadPromise() {
    delete this.uriToLoadPromise;
    return (this.uriToLoadPromise = (function() {
      // window.arguments[0]: URI to load (string), or an nsIArray of
      //                      nsISupportsStrings to load, or a xul:tab of
      //                      a tabbrowser, which will be replaced by this
      //                      window (for this case, all other arguments are
      //                      ignored).
      if (!window.arguments || !window.arguments[0]) {
        return null;
      }

      let uri = window.arguments[0];
      let defaultArgs = Cc["@mozilla.org/browser/clh;1"].getService(
        Ci.nsIBrowserHandler
      ).defaultArgs;

      // If the given URI is different from the homepage, we want to load it.
      if (uri != defaultArgs) {
        if (uri instanceof Ci.nsIArray) {
          // Transform the nsIArray of nsISupportsString's into a JS Array of
          // JS strings.
          return Array.from(
            uri.enumerate(Ci.nsISupportsString),
            supportStr => supportStr.data
          );
        } else if (uri instanceof Ci.nsISupportsString) {
          return uri.data;
        }
        return uri;
      }

      // The URI appears to be the the homepage. We want to load it only if
      // session restore isn't about to override the homepage.
      let willOverride = SessionStartup.willOverrideHomepage;
      if (typeof willOverride == "boolean") {
        return willOverride ? null : uri;
      }
      return willOverride.then(willOverrideHomepage =>
        willOverrideHomepage ? null : uri
      );
    })());
  },

  // Calls the given callback with the URI to load at startup.
  // Synchronously if possible, or after uriToLoadPromise resolves otherwise.
  _callWithURIToLoad(callback) {
    let uriToLoad = this.uriToLoadPromise;
    if (uriToLoad && uriToLoad.then) {
      uriToLoad.then(callback);
    } else {
      callback(uriToLoad);
    }
  },

  onUnload() {
    gUIDensity.uninit();

    TabsInTitlebar.uninit();

    ToolbarIconColor.uninit();

    // In certain scenarios it's possible for unload to be fired before onload,
    // (e.g. if the window is being closed after browser.js loads but before the
    // load completes). In that case, there's nothing to do here.
    if (!this._loadHandled) {
      return;
    }

    // First clean up services initialized in gBrowserInit.onLoad (or those whose
    // uninit methods don't depend on the services having been initialized).

    CombinedStopReload.uninit();

    gGestureSupport.init(false);

    gHistorySwipeAnimation.uninit();

    FullScreen.uninit();

    gSync.uninit();

    gExtensionsNotifications.uninit();

    try {
      gBrowser.removeProgressListener(window.XULBrowserWindow);
      gBrowser.removeTabsProgressListener(window.TabsProgressListener);
    } catch (ex) {}

    PlacesToolbarHelper.uninit();

    BookmarkingUI.uninit();

    TabletModeUpdater.uninit();

    gTabletModePageCounter.finish();

    BrowserOnClick.uninit();

    CaptivePortalWatcher.uninit();

    SidebarUI.uninit();

    DownloadsButton.uninit();

    gAccessibilityServiceIndicator.uninit();

    AccessibilityRefreshBlocker.uninit();

    if (gToolbarKeyNavEnabled) {
      ToolbarKeyboardNavigator.uninit();
    }

    BrowserSearch.uninit();

    NewTabPagePreloading.removePreloadedBrowser(window);

    // Now either cancel delayedStartup, or clean up the services initialized from
    // it.
    if (this._boundDelayedStartup) {
      this._cancelDelayedStartup();
    } else {
      if (Win7Features) {
        Win7Features.onCloseWindow();
      }
      Services.prefs.removeObserver(ctrlTab.prefName, ctrlTab);
      ctrlTab.uninit();
      gBrowserThumbnails.uninit();
      gProtectionsHandler.uninit();
      FullZoom.destroy();

      Services.obs.removeObserver(gIdentityHandler, "perm-changed");
      Services.obs.removeObserver(gRemoteControl, "remote-active");
      Services.obs.removeObserver(
        gSessionHistoryObserver,
        "browser:purge-session-history"
      );
      Services.obs.removeObserver(
        gStoragePressureObserver,
        "QuotaManager::StoragePressure"
      );
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-disabled");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-started");
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-blocked");
      Services.obs.removeObserver(
        gXPInstallObserver,
        "addon-install-origin-blocked"
      );
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-failed");
      Services.obs.removeObserver(
        gXPInstallObserver,
        "addon-install-confirmation"
      );
      Services.obs.removeObserver(gXPInstallObserver, "addon-install-complete");
      window.messageManager.removeMessageListener(
        "Browser:URIFixup",
        gKeywordURIFixup
      );
      window.messageManager.removeMessageListener(
        "Browser:LoadURI",
        RedirectLoad
      );

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
    window.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIXULWindow).XULBrowserWindow = null;
    window.browserDOMWindow = null;
  },
};

gBrowserInit.idleTasksFinishedPromise = new Promise(resolve => {
  gBrowserInit.idleTaskPromiseResolve = resolve;
});

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
      if (XULBrowserWindow.stopCommand.getAttribute("disabled") != "true") {
        BrowserStop();
      }
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
      PrintUtils.printWindow(
        gBrowser.selectedBrowser.outerWindowID,
        gBrowser.selectedBrowser
      );
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
  aEvent = getRootEvent(aEvent);

  let index = aEvent.target.getAttribute("index");
  if (!index) {
    return false;
  }

  let where = whereToOpenLink(aEvent);

  if (where == "current") {
    // Normal click. Go there in the current tab and update session history.

    try {
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
      gBrowser.goForward();
    } catch (ex) {}
  } else {
    duplicateTabIn(gBrowser.selectedTab, where, 1);
  }
}

function BrowserBack(aEvent) {
  let where = whereToOpenLink(aEvent, false, true);

  if (where == "current") {
    try {
      gBrowser.goBack();
    } catch (ex) {}
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
  gBrowser.webNavigation.stop(Ci.nsIWebNavigation.STOP_ALL);
}

function BrowserReloadOrDuplicate(aEvent) {
  aEvent = getRootEvent(aEvent);
  let accelKeyPressed =
    AppConstants.platform == "macosx" ? aEvent.metaKey : aEvent.ctrlKey;
  var backgroundTabModifier = aEvent.button == 1 || accelKeyPressed;

  if (aEvent.shiftKey && !backgroundTabModifier) {
    BrowserReloadSkipCache();
    return;
  }

  let where = whereToOpenLink(aEvent, false, true);
  if (where == "current") {
    BrowserReload();
  } else {
    duplicateTabIn(gBrowser.selectedTab, where);
  }
}

function BrowserReload() {
  if (gBrowser.currentURI.schemeIs("view-source")) {
    // Bug 1167797: For view source, we always skip the cache
    return BrowserReloadSkipCache();
  }
  const reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
  BrowserReloadWithFlags(reloadFlags);
}

const kSkipCacheFlags =
  Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
  Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
function BrowserReloadSkipCache() {
  // Bypass proxy and cache.
  BrowserReloadWithFlags(kSkipCacheFlags);
}

function BrowserHome(aEvent) {
  if (aEvent && "button" in aEvent && aEvent.button == 2) {
    // right-click: do nothing
    return;
  }

  var homePage = HomePage.get(window);
  var where = whereToOpenLink(aEvent, false, true);
  var urls;
  var notifyObservers;

  // Home page should open in a new tab when current tab is an app tab
  if (where == "current" && gBrowser && gBrowser.selectedTab.pinned) {
    where = "tab";
  }

  // openTrustedLinkIn in utilityOverlay.js doesn't handle loading multiple pages
  switch (where) {
    case "current":
      // If we're going to load an initial page in the current tab as the
      // home page, we set initialPageLoadedFromURLBar so that the URL
      // bar is cleared properly (even during a remoteness flip).
      if (isInitialPage(homePage)) {
        gBrowser.selectedBrowser.initialPageLoadedFromUserAction = homePage;
      }
      loadOneOrMoreURIs(
        homePage,
        Services.scriptSecurityManager.getSystemPrincipal(),
        null
      );
      if (isBlankPageURL(homePage)) {
        focusAndSelectUrlBar();
      } else {
        gBrowser.selectedBrowser.focus();
      }
      notifyObservers = true;
      break;
    case "tabshifted":
    case "tab":
      urls = homePage.split("|");
      var loadInBackground = Services.prefs.getBoolPref(
        "browser.tabs.loadBookmarksInBackground",
        false
      );
      // The homepage observer event should only be triggered when the homepage opens
      // in the foreground. This is mostly to support the homepage changed by extension
      // doorhanger which doesn't currently support background pages. This may change in
      // bug 1438396.
      notifyObservers = !loadInBackground;
      gBrowser.loadTabs(urls, {
        inBackground: loadInBackground,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
        csp: null,
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

function loadOneOrMoreURIs(aURIString, aTriggeringPrincipal, aCsp) {
  // we're not a browser window, pass the URI string to a new browser window
  if (window.location.href != AppConstants.BROWSER_CHROME_URL) {
    window.openDialog(
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      "all,dialog=no",
      aURIString
    );
    return;
  }

  // This function throws for certain malformed URIs, so use exception handling
  // so that we don't disrupt startup
  try {
    gBrowser.loadTabs(aURIString.split("|"), {
      inBackground: false,
      replace: true,
      triggeringPrincipal: aTriggeringPrincipal,
      csp: aCsp,
    });
  } catch (e) {}
}

/**
 * Focuses the location bar input field and selects its contents.
 */
function focusAndSelectUrlBar() {
  // In customize mode, the url bar is disabled. If a new tab is opened or the
  // user switches to a different tab, this function gets called before we've
  // finished leaving customize mode, and the url bar will still be disabled.
  // We can't focus it when it's disabled, so we need to re-run ourselves when
  // we've finished leaving customize mode.
  if (CustomizationHandler.isCustomizing()) {
    gNavToolbox.addEventListener("aftercustomization", focusAndSelectUrlBar, {
      once: true,
    });
    return;
  }

  if (window.fullScreen) {
    FullScreen.showNavToolbox();
  }

  gURLBar.select();
}

function openLocation(event) {
  if (window.location.href == AppConstants.BROWSER_CHROME_URL) {
    focusAndSelectUrlBar();
    if (gURLBar.openViewOnFocus && !gURLBar.view.isOpen) {
      gURLBar.startQuery({ event });
    }
    return;
  }

  // If there's an open browser window, redirect the command there.
  let win = getTopWin();
  if (win) {
    win.focus();
    win.openLocation();
    return;
  }

  // There are no open browser windows; open a new one.
  window.openDialog(
    AppConstants.BROWSER_CHROME_URL,
    "_blank",
    "chrome,all,dialog=no",
    BROWSER_NEW_TAB_URL
  );
}

function BrowserOpenTab(event) {
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

  // A notification intended to be useful for modular peformance tracking
  // starting as close as is reasonably possible to the time when the user
  // expressed the intent to open a new tab.  Since there are a lot of
  // entry points, this won't catch every single tab created, but most
  // initiated by the user should go through here.
  //
  // Note 1: This notification gets notified with a promise that resolves
  //         with the linked browser when the tab gets created
  // Note 2: This is also used to notify a user that an extension has changed
  //         the New Tab page.
  Services.obs.notifyObservers(
    {
      wrappedJSObject: new Promise(resolve => {
        openTrustedLinkIn(BROWSER_NEW_TAB_URL, where, {
          relatedToCurrent,
          resolveOnNewTabCreated: resolve,
        });
      }),
    },
    "browser-open-newtab-start"
  );
}

var gLastOpenDirectory = {
  _lastDir: null,
  get path() {
    if (!this._lastDir || !this._lastDir.exists()) {
      try {
        this._lastDir = Services.prefs.getComplexValue(
          "browser.open.lastDir",
          Ci.nsIFile
        );
        if (!this._lastDir.exists()) {
          this._lastDir = null;
        }
      } catch (e) {}
    }
    return this._lastDir;
  },
  set path(val) {
    try {
      if (!val || !val.isDirectory()) {
        return;
      }
    } catch (e) {
      return;
    }
    this._lastDir = val.clone();

    // Don't save the last open directory pref inside the Private Browsing mode
    if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
      Services.prefs.setComplexValue(
        "browser.open.lastDir",
        Ci.nsIFile,
        this._lastDir
      );
    }
  },
  reset() {
    this._lastDir = null;
  },
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
            gLastOpenDirectory.path = fp.file.parent.QueryInterface(Ci.nsIFile);
          }
        } catch (ex) {}
        openTrustedLinkIn(fp.fileURL.spec, "current");
      }
    };

    fp.init(
      window,
      gNavigatorBundle.getString("openFile"),
      nsIFilePicker.modeOpen
    );
    fp.appendFilters(
      nsIFilePicker.filterAll |
        nsIFilePicker.filterText |
        nsIFilePicker.filterImages |
        nsIFilePicker.filterXML |
        nsIFilePicker.filterHTML
    );
    fp.displayDirectory = gLastOpenDirectory.path;
    fp.open(fpCallback);
  } catch (ex) {}
}

function BrowserCloseTabOrWindow(event) {
  // If we're not a browser window, just close the window.
  if (window.location.href != AppConstants.BROWSER_CHROME_URL) {
    closeWindow(true);
    return;
  }

  // In a multi-select context, close all selected tabs
  if (gBrowser.multiSelectedTabsCount) {
    gBrowser.removeMultiSelectedTabs();
    return;
  }

  // Keyboard shortcuts that would close a tab that is pinned select the first
  // unpinned tab instead.
  if (
    event &&
    (event.ctrlKey || event.metaKey || event.altKey) &&
    gBrowser.selectedTab.pinned
  ) {
    if (gBrowser.visibleTabs.length > gBrowser._numPinnedTabs) {
      gBrowser.tabContainer.selectedIndex = gBrowser._numPinnedTabs;
    }
    return;
  }

  // If the current tab is the last one, this will close the window.
  gBrowser.removeCurrentTab({ animate: true });
}

function BrowserTryToCloseWindow() {
  if (WindowIsClosing()) {
    window.close();
  } // WindowIsClosing does all the necessary checks
}

function loadURI(
  uri,
  referrerInfo,
  postData,
  allowThirdPartyFixup,
  userContextId,
  originPrincipal,
  originStoragePrincipal,
  forceAboutBlankViewerInCurrent,
  triggeringPrincipal,
  allowInheritPrincipal = false,
  csp = null
) {
  if (!triggeringPrincipal) {
    throw new Error("Must load with a triggering Principal");
  }

  try {
    openLinkIn(uri, "current", {
      referrerInfo,
      postData,
      allowThirdPartyFixup,
      userContextId,
      originPrincipal,
      originStoragePrincipal,
      triggeringPrincipal,
      csp,
      forceAboutBlankViewerInCurrent,
      allowInheritPrincipal,
    });
  } catch (e) {
    Cu.reportError(e);
  }
}

function getLoadContext() {
  return window.docShell.QueryInterface(Ci.nsILoadContext);
}

function readFromClipboard() {
  var url;

  try {
    // Create transferable that will transfer the text.
    var trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    trans.init(getLoadContext());

    trans.addDataFlavor("text/unicode");

    // If available, use selection clipboard, otherwise global one
    if (Services.clipboard.supportsSelectionClipboard()) {
      Services.clipboard.getData(trans, Services.clipboard.kSelectionClipboard);
    } else {
      Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);
    }

    var data = {};
    trans.getTransferData("text/unicode", data);

    if (data) {
      data = data.value.QueryInterface(Ci.nsISupportsString);
      url = data.data;
    }
  } catch (ex) {}

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
      throw new Error(
        "BrowserViewSourceOfDocument cannot accept a CPOW as a document."
      );
    }

    let win = doc.defaultView;
    let browser = win.docShell.chromeEventHandler;
    let outerWindowID = win.windowUtils.outerWindowID;
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
      throw new Error(
        "BrowserViewSourceOfDocument should be passed the " +
          "subject browser if called from a window without " +
          "gBrowser defined."
      );
    }
    // Some internal URLs (such as specific chrome: and about: URLs that are
    // not yet remote ready) cannot be loaded in a remote browser.  View
    // source in tab expects the new view source browser's remoteness to match
    // that of the original URL, so disable remoteness if necessary for this
    // URL.
    preferredRemoteType = E10SUtils.getRemoteTypeForURI(
      args.URL,
      gMultiProcessBrowser,
      gFissionBrowser
    );
  }

  // In the case of popups, we need to find a non-popup browser window.
  if (!tabBrowser || !window.toolbar.visible) {
    // This returns only non-popup browser windows by default.
    let browserWindow = BrowserWindowTracker.getTopWindow();
    tabBrowser = browserWindow.gBrowser;
  }

  const inNewWindow = !Services.prefs.getBoolPref("view_source.tab");

  // `viewSourceInBrowser` will load the source content from the page
  // descriptor for the tab (when possible) or fallback to the network if
  // that fails.  Either way, the view source module will manage the tab's
  // location, so use "about:blank" here to avoid unnecessary redundant
  // requests.
  let tab = tabBrowser.loadOneTab("about:blank", {
    relatedToCurrent: true,
    inBackground: inNewWindow,
    skipAnimation: inNewWindow,
    preferredRemoteType,
    sameProcessAsFrameLoader: args.browser ? args.browser.frameLoader : null,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  args.viewSourceBrowser = tabBrowser.getBrowserForTab(tab);
  top.gViewSourceUtils.viewSourceInBrowser(args);

  if (inNewWindow) {
    tabBrowser.hideTab(tab);
    tabBrowser.replaceTabWithWindow(tab);
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
function BrowserPageInfo(
  documentURL,
  initialTab,
  imageElement,
  frameOuterWindowID,
  browser
) {
  if (documentURL instanceof HTMLDocument) {
    Deprecated.warning(
      "Please pass the location URL instead of the document " +
        "to BrowserPageInfo() as the first argument.",
      "https://bugzilla.mozilla.org/show_bug.cgi?id=1238180"
    );
    documentURL = documentURL.location;
  }

  let args = { initialTab, imageElement, frameOuterWindowID, browser };

  documentURL = documentURL || window.gBrowser.selectedBrowser.currentURI.spec;

  // Check for windows matching the url
  for (let currentWindow of Services.wm.getEnumerator("Browser:page-info")) {
    if (currentWindow.closed) {
      continue;
    }
    if (
      currentWindow.document.documentElement.getAttribute("relatedUrl") ==
      documentURL
    ) {
      currentWindow.focus();
      currentWindow.resetPageInfo(args);
      return currentWindow;
    }
  }

  // We didn't find a matching window, so open a new one.
  return openDialog(
    "chrome://browser/content/pageinfo/pageInfo.xul",
    "",
    "chrome,toolbar,dialog=no,resizable",
    args
  );
}

/**
 * Sets the URI to display in the location bar.
 *
 * @param aURI [optional]
 *        nsIURI to set. If this is unspecified, the current URI will be used.
 * @param updatePopupNotifications [optional]
 *        Passed though to SetPageProxyState, indicates whether the
 *        PopupNotifications need updated.
 */
function URLBarSetURI(aURI, updatePopupNotifications) {
  var value = gBrowser.userTypedValue;
  var valid = false;

  // Explicitly check for nulled out value. We don't want to reset the URL
  // bar if the user has deleted the URL and we'd just put the same URL
  // back. See bug 304198.
  if (value === null) {
    let uri = aURI || gBrowser.currentURI;
    // Strip off usernames and passwords for the location bar
    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}

    // Replace initial page URIs with an empty string
    // only if there's no opener (bug 370555).
    if (
      isInitialPage(uri) &&
      checkEmptyPageOrigin(gBrowser.selectedBrowser, uri)
    ) {
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
  } else if (
    isInitialPage(value) &&
    checkEmptyPageOrigin(gBrowser.selectedBrowser)
  ) {
    value = "";
    valid = true;
  }

  let isDifferentValidValue = valid && value != gURLBar.value;
  gURLBar.value = value;
  gURLBar.valueIsTyped = !valid;
  gURLBar.removeAttribute("usertyping");
  if (isDifferentValidValue) {
    // The selection is enforced only for new values, to avoid overriding the
    // cursor position when the user switches windows while typing.
    gURLBar.selectionStart = gURLBar.selectionEnd = 0;
  }

  SetPageProxyState(valid ? "valid" : "invalid", updatePopupNotifications);
}

function losslessDecodeURI(aURI) {
  let scheme = aURI.scheme;
  if (scheme == "moz-action") {
    throw new Error("losslessDecodeURI should never get a moz-action URI");
  }

  var value = aURI.displaySpec;

  let decodeASCIIOnly = !["https", "http", "file", "ftp"].includes(scheme);
  // Try to decode as UTF-8 if there's no encoding sequence that we would break.
  if (!/%25(?:3B|2F|3F|3A|40|26|3D|2B|24|2C|23)/i.test(value)) {
    if (decodeASCIIOnly) {
      // This only decodes ascii characters (hex) 20-7e, except 25 (%).
      // This avoids both cases stipulated below (%-related issues, and \r, \n
      // and \t, which would be %0d, %0a and %09, respectively) as well as any
      // non-US-ascii characters.
      value = value.replace(
        /%(2[0-4]|2[6-9a-f]|[3-6][0-9a-f]|7[0-9a-e])/g,
        decodeURI
      );
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
          .replace(
            /%(?!3B|2F|3F|3A|40|26|3D|2B|24|2C|23)|[\r\n\t]|\s(?=\s)|\s$/gi,
            encodeURIComponent
          );
      } catch (e) {}
    }
  }

  // Encode invisible characters (C0/C1 control characters, U+007F [DEL],
  // U+00A0 [no-break space], line and paragraph separator,
  // object replacement character) (bug 452979, bug 909264)
  value = value.replace(
    // eslint-disable-next-line no-control-regex
    /[\u0000-\u001f\u007f-\u00a0\u2028\u2029\ufffc]/g,
    encodeURIComponent
  );

  // Encode default ignorable characters (bug 546013)
  // except ZWNJ (U+200C) and ZWJ (U+200D) (bug 582186).
  // This includes all bidirectional formatting characters.
  // (RFC 3987 sections 3.2 and 4.1 paragraph 6)
  value = value.replace(
    // eslint-disable-next-line no-misleading-character-class
    /[\u00ad\u034f\u061c\u115f-\u1160\u17b4-\u17b5\u180b-\u180d\u200b\u200e-\u200f\u202a-\u202e\u2060-\u206f\u3164\ufe00-\ufe0f\ufeff\uffa0\ufff0-\ufff8]|\ud834[\udd73-\udd7a]|[\udb40-\udb43][\udc00-\udfff]/g,
    encodeURIComponent
  );
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
  if (
    splitter &&
    ((splitter.nextElementSibling == searchbar &&
      splitter.previousElementSibling == urlbar) ||
      (splitter.nextElementSibling == urlbar &&
        splitter.previousElementSibling == searchbar))
  ) {
    return;
  }

  var ibefore = null;
  if (urlbar && searchbar) {
    if (urlbar.nextElementSibling == searchbar) {
      ibefore = searchbar;
    } else if (searchbar.nextElementSibling == urlbar) {
      ibefore = urlbar;
    }
  }

  if (ibefore) {
    if (!splitter) {
      splitter = document.createXULElement("splitter");
      splitter.id = "urlbar-search-splitter";
      splitter.setAttribute("resizebefore", "flex");
      splitter.setAttribute("resizeafter", "flex");
      splitter.setAttribute("skipintoolbarset", "true");
      splitter.setAttribute("overflows", "false");
      splitter.className = "chromeclass-toolbar-additional";
    }
    urlbar.parentNode.insertBefore(splitter, ibefore);
  } else if (splitter) {
    splitter.remove();
  }
}

function UpdatePageProxyState() {
  if (gURLBar && gURLBar.value != gLastValidURLStr) {
    SetPageProxyState("invalid", true);
  }
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
 * @param updatePopupNotifications
 *        Boolean that indicates whether we should update the PopupNotifications
 *        visibility due to this change, otherwise avoid doing so as it is being
 *        handled somewhere else.
 */
function SetPageProxyState(aState, updatePopupNotifications) {
  if (!gURLBar) {
    return;
  }

  let oldPageProxyState = gURLBar.getAttribute("pageproxystate");
  gURLBar.setPageProxyState(aState);

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
  if (oldPageProxyState == aState || !updatePopupNotifications) {
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
  if (aEvent.button == 1 && Services.prefs.getBoolPref("middlemouse.paste")) {
    middleMousePaste(aEvent);
  }
}

// Values for telemtery bins: see TLS_ERROR_REPORT_UI in Histograms.json
const TLS_ERROR_REPORT_TELEMETRY_AUTO_CHECKED = 2;
const TLS_ERROR_REPORT_TELEMETRY_AUTO_UNCHECKED = 3;

const SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const SEC_ERROR_UNKNOWN_ISSUER = SEC_ERROR_BASE + 13;

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
    mm.addMessageListener("Browser:SiteBlockedError", this);
    mm.addMessageListener("Browser:EnableOnlineMode", this);
    mm.addMessageListener("Browser:SetSSLErrorReportAuto", this);
    mm.addMessageListener("Browser:ResetSSLPreferences", this);
    mm.addMessageListener("Browser:SSLErrorReportTelemetry", this);
    mm.addMessageListener("Browser:SSLErrorGoBack", this);
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
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Browser:CertExceptionError":
        this.onCertError(
          msg.target,
          msg.data.elementId,
          msg.data.isTopFrame,
          msg.data.location,
          msg.data.securityInfoAsString,
          msg.data.frameId
        );
        break;
      case "Browser:SiteBlockedError":
        this.onAboutBlocked(
          msg.data.elementId,
          msg.data.reason,
          msg.data.isTopFrame,
          msg.data.location,
          msg.data.blockedInfo
        );
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
        Services.prefs.setBoolPref(
          "security.ssl.errorReporting.automatic",
          msg.json.automatic
        );
        let bin = TLS_ERROR_REPORT_TELEMETRY_AUTO_UNCHECKED;
        if (msg.json.automatic) {
          bin = TLS_ERROR_REPORT_TELEMETRY_AUTO_CHECKED;
        }
        Services.telemetry.getHistogramById("TLS_ERROR_REPORT_UI").add(bin);
        break;
      case "Browser:SSLErrorReportTelemetry":
        let reportStatus = msg.data.reportStatus;
        Services.telemetry
          .getHistogramById("TLS_ERROR_REPORT_UI")
          .add(reportStatus);
        break;
      case "Browser:SSLErrorGoBack":
        goBackFromErrorPage();
        break;
    }
  },

  onCertError(
    browser,
    elementId,
    isTopFrame,
    location,
    securityInfoAsString,
    frameId
  ) {
    let securityInfo;
    let cert;

    switch (elementId) {
      case "viewCertificate":
        securityInfo = getSecurityInfo(securityInfoAsString);
        cert = securityInfo.serverCert;
        if (Services.prefs.getBoolPref("security.aboutcertificate.enabled")) {
          let certChain = getCertificateChain(securityInfo.failedCertChain);
          let certs = certChain.map(elem =>
            encodeURIComponent(elem.getBase64DERString())
          );
          let certsStringURL = certs.map(elem => `cert=${elem}`);
          certsStringURL = certsStringURL.join("&");
          let url = `about:certificate?${certsStringURL}`;
          openTrustedLinkIn(url, "tab", {
            triggeringPrincipal: browser.contentPrincipal,
          });
        } else {
          Services.ww.openWindow(
            window,
            "chrome://pippki/content/certViewer.xul",
            "_blank",
            "centerscreen,chrome",
            cert
          );
        }
        break;
      case "exceptionDialogButton":
        securityInfo = getSecurityInfo(securityInfoAsString);
        let overrideService = Cc[
          "@mozilla.org/security/certoverride;1"
        ].getService(Ci.nsICertOverrideService);
        let flags = 0;
        if (securityInfo.isUntrusted) {
          flags |= overrideService.ERROR_UNTRUSTED;
        }
        if (securityInfo.isDomainMismatch) {
          flags |= overrideService.ERROR_MISMATCH;
        }
        if (securityInfo.isNotValidAtThisTime) {
          flags |= overrideService.ERROR_TIME;
        }
        let uri = Services.uriFixup.createFixupURI(location, 0);
        let permanentOverride =
          !PrivateBrowsingUtils.isBrowserPrivate(browser) &&
          Services.prefs.getBoolPref("security.certerrors.permanentOverride");
        cert = securityInfo.serverCert;
        overrideService.rememberValidityOverride(
          uri.asciiHost,
          uri.port,
          cert,
          flags,
          !permanentOverride
        );
        browser.reload();
        break;

      case "returnButton":
        goBackFromErrorPage();
        break;

      case "advancedPanelReturnButton":
        goBackFromErrorPage();
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
    let secHistogram = Services.telemetry.getHistogramById(
      "URLCLASSIFIER_UI_EVENTS"
    );
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
    let triggeringPrincipal = E10SUtils.deserializePrincipal(
      blockedInfo.triggeringPrincipal,
      _createNullPrincipalFromTabUserContextId()
    );
    // Allow users to override and continue through to the site,
    // but add a notify bar as a reminder, so that they don't lose
    // track after, e.g., tab switching.
    gBrowser.loadURI(gBrowser.currentURI.spec, {
      triggeringPrincipal,
      flags: Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CLASSIFIER,
    });

    Services.perms.add(
      gBrowser.currentURI,
      "safe-browsing",
      Ci.nsIPermissionManager.ALLOW_ACTION,
      Ci.nsIPermissionManager.EXPIRE_SESSION
    );

    let buttons = [
      {
        label: gNavigatorBundle.getString(
          "safebrowsing.getMeOutOfHereButton.label"
        ),
        accessKey: gNavigatorBundle.getString(
          "safebrowsing.getMeOutOfHereButton.accessKey"
        ),
        callback() {
          getMeOutOfHere();
        },
      },
    ];

    let title;
    if (reason === "malware") {
      let reportUrl = gSafeBrowsing.getReportURL("MalwareMistake", blockedInfo);
      title = gNavigatorBundle.getString("safebrowsing.reportedAttackSite");
      // There's no button if we can not get report url, for example if the provider
      // of blockedInfo is not Google
      if (reportUrl) {
        buttons[1] = {
          label: gNavigatorBundle.getString(
            "safebrowsing.notAnAttackButton.label"
          ),
          accessKey: gNavigatorBundle.getString(
            "safebrowsing.notAnAttackButton.accessKey"
          ),
          callback() {
            openTrustedLinkIn(reportUrl, "tab");
          },
        };
      }
    } else if (reason === "phishing") {
      let reportUrl = gSafeBrowsing.getReportURL("PhishMistake", blockedInfo);
      title = gNavigatorBundle.getString("safebrowsing.deceptiveSite");
      // There's no button if we can not get report url, for example if the provider
      // of blockedInfo is not Google
      if (reportUrl) {
        buttons[1] = {
          label: gNavigatorBundle.getString(
            "safebrowsing.notADeceptiveSiteButton.label"
          ),
          accessKey: gNavigatorBundle.getString(
            "safebrowsing.notADeceptiveSiteButton.accessKey"
          ),
          callback() {
            openTrustedLinkIn(reportUrl, "tab");
          },
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
  gBrowser.loadURI(getDefaultHomePage(), {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(), // Also needs to load homepage
  });
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
    gBrowser.loadURI(getDefaultHomePage(), {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  } else {
    BrowserBack();
  }
}

/**
 * Return the default start page for the cases when the user's own homepage is
 * infected, so we can get them somewhere safe.
 */
function getDefaultHomePage() {
  let url = BROWSER_NEW_TAB_URL;
  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    return url;
  }
  url = HomePage.getDefault();
  // If url is a pipe-delimited set of pages, just take the first one.
  if (url.includes("|")) {
    url = url.split("|")[0];
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
  let unchangedRemoteness = [];

  for (let tab of gBrowser.selectedTabs) {
    let browser = tab.linkedBrowser;
    let url = browser.currentURI.spec;
    if (gBrowser.updateBrowserRemotenessByURL(browser, url)) {
      // If the remoteness has changed, the new browser doesn't have any
      // information of what was loaded before, so we need to load the previous
      // URL again.
      if (tab.linkedPanel) {
        loadBrowserURI(browser, url);
      } else {
        // Shift to fully loaded browser and make
        // sure load handler is instantiated.
        tab.addEventListener(
          "SSTabRestoring",
          () => loadBrowserURI(browser, url),
          { once: true }
        );
        gBrowser._insertBrowser(tab);
      }
    } else if (browser.hasAttribute("recordExecution")) {
      // Recording tabs always use new content processes when reloading, to get
      // a fresh recording.
      gBrowser.updateBrowserRemoteness(browser, {
        recordExecution: "*",
        newFrameloader: true,
        remoteType: E10SUtils.DEFAULT_REMOTE_TYPE,
      });
      loadBrowserURI(browser, url);
    } else {
      unchangedRemoteness.push(tab);
    }
  }

  if (unchangedRemoteness.length == 0) {
    return;
  }

  // Reset temporary permissions on the remaining tabs to reload.
  // This is done here because we only want to reset
  // permissions on user reload.
  for (let tab of unchangedRemoteness) {
    SitePermissions.clearTemporaryPermissions(tab.linkedBrowser);
    // Also reset DOS mitigations for the basic auth prompt on reload.
    delete tab.linkedBrowser.authPromptAbuseCounter;
  }
  PanelMultiView.hidePopup(gIdentityHandler._identityPopup);

  let handlingUserInput = window.windowUtils.isHandlingUserInput;

  for (let tab of unchangedRemoteness) {
    if (tab.linkedPanel) {
      sendReloadMessage(tab);
    } else {
      // Shift to fully loaded browser and make
      // sure load handler is instantiated.
      tab.addEventListener("SSTabRestoring", () => sendReloadMessage(tab), {
        once: true,
      });
      gBrowser._insertBrowser(tab);
    }
  }

  function loadBrowserURI(browser, url) {
    browser.loadURI(url, {
      flags: reloadFlags,
      triggeringPrincipal: browser.contentPrincipal,
    });
  }

  function sendReloadMessage(tab) {
    tab.linkedBrowser.sendMessageToActor(
      "Browser:Reload",
      { flags: reloadFlags, handlingUserInput },
      "BrowserTab"
    );
  }
}

function getSecurityInfo(securityInfoAsString) {
  if (!securityInfoAsString) {
    return null;
  }

  let securityInfo = gSerializationHelper.deserializeObject(
    securityInfoAsString
  );
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);

  return securityInfo;
}

// TODO: can we pull getPEMString in from pippki.js instead of
// duplicating them here?
function getPEMString(cert) {
  var derb64 = cert.getBase64DERString();
  // Wrap the Base64 string into lines of 64 characters,
  // with CRLF line breaks (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return (
    "-----BEGIN CERTIFICATE-----\r\n" +
    wrapped +
    "\r\n-----END CERTIFICATE-----\r\n"
  );
}

function getCertificateChain(certChain) {
  let certificates = [];
  for (let cert of certChain.getEnumerator()) {
    certificates.push(cert);
  }
  return certificates;
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
    return this._simplifyPageTab
      ? gBrowser.getBrowserForTab(this._simplifyPageTab)
      : null;
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
    let tabsToRemove = [
      "_simplifyPageTab",
      "_printPreviewTab",
      "_simplifiedPrintPreviewTab",
    ];
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

    if (gInPrintPreviewMode) {
      this._hideChrome();
    } else {
      this._showChrome();
    }

    TabsInTitlebar.allowedBy("print-preview", !gInPrintPreviewMode);
  },
  _hideChrome() {
    this._chromeState = {};

    this._chromeState.sidebarOpen = SidebarUI.isOpen;
    this._sidebarCommand = SidebarUI.currentID;
    SidebarUI.hide();

    this._chromeState.findOpen = gFindBarInitialized && !gFindBar.hidden;
    if (gFindBarInitialized) {
      gFindBar.close();
    }

    gBrowser.getNotificationBox().stack.hidden = true;
    gNotificationBox.stack.hidden = true;
  },
  _showChrome() {
    gNotificationBox.stack.hidden = false;
    gBrowser.getNotificationBox().stack.hidden = false;

    if (this._chromeState.findOpen) {
      gLazyFindCommand("open");
    }

    if (this._chromeState.sidebarOpen) {
      SidebarUI.show(this._sidebarCommand);
    }
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

  getCSP(aEvent) {
    return Services.droppedLinkHandler.getCSP(aEvent);
  },

  validateURIsForDrop(aEvent, aURIs) {
    return Services.droppedLinkHandler.validateURIsForDrop(aEvent, aURIs);
  },

  dropLinks(aEvent, aDisallowInherit) {
    return Services.droppedLinkHandler.dropLinks(aEvent, aDisallowInherit);
  },
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
        browserDragAndDrop.validateURIsForDrop(aEvent, urls);
      } catch (e) {
        return;
      }

      setTimeout(openHomeDialog, 0, urls.join("|"));
    }
  },

  onDragOver(aEvent) {
    if (HomePage.locked) {
      return;
    }
    browserDragAndDrop.dragOver(aEvent);
    aEvent.dropEffect = "link";
  },
  onDragExit(aEvent) {},
};

function openHomeDialog(aURL) {
  var promptTitle = gNavigatorBundle.getString("droponhometitle");
  var promptMsg;
  if (aURL.includes("|")) {
    promptMsg = gNavigatorBundle.getString("droponhomemsgMultiple");
  } else {
    promptMsg = gNavigatorBundle.getString("droponhomemsg");
  }

  var pressedVal = Services.prompt.confirmEx(
    window,
    promptTitle,
    promptMsg,
    Services.prompt.STD_YES_NO_BUTTONS,
    null,
    null,
    null,
    null,
    { value: 0 }
  );

  if (pressedVal == 0) {
    HomePage.set(aURL).catch(Cu.reportError);
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
    let csp = browserDragAndDrop.getCSP(aEvent);

    if (
      links.length >=
      Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")
    ) {
      // Sync dialog cannot be used inside drop event handler.
      let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(
        links.length,
        window
      );
      if (!answer) {
        return;
      }
    }

    for (let link of links) {
      if (link.url) {
        let data = await UrlbarUtils.getShortcutOrURIAndPostData(link.url);
        // Allow third-party services to fixup this URL.
        openNewTabWith(data.url, shiftKey, {
          // TODO fix allowInheritPrincipal
          // (this is required by javascript: drop to the new window) Bug 1475201
          allowInheritPrincipal: true,
          postData: data.postData,
          allowThirdPartyFixup: true,
          triggeringPrincipal,
          csp,
        });
      }
    }
  },
};

var newWindowButtonObserver = {
  onDragOver(aEvent) {
    browserDragAndDrop.dragOver(aEvent);
  },
  onDragExit(aEvent) {},
  async onDrop(aEvent) {
    let links = browserDragAndDrop.dropLinks(aEvent);
    let triggeringPrincipal = browserDragAndDrop.getTriggeringPrincipal(aEvent);
    let csp = browserDragAndDrop.getCSP(aEvent);

    if (
      links.length >=
      Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")
    ) {
      // Sync dialog cannot be used inside drop event handler.
      let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(
        links.length,
        window
      );
      if (!answer) {
        return;
      }
    }

    for (let link of links) {
      if (link.url) {
        let data = await UrlbarUtils.getShortcutOrURIAndPostData(link.url);
        // Allow third-party services to fixup this URL.
        openNewWindowWith(data.url, {
          // TODO fix allowInheritPrincipal
          // (this is required by javascript: drop to the new window) Bug 1475201
          allowInheritPrincipal: true,
          postData: data.postData,
          allowThirdPartyFixup: true,
          triggeringPrincipal,
          csp,
        });
      }
    }
  },
};
const DOMEventHandler = {
  init() {
    let mm = window.messageManager;
    mm.addMessageListener("Link:LoadingIcon", this);
    mm.addMessageListener("Link:SetIcon", this);
    mm.addMessageListener("Link:SetFailedIcon", this);
    mm.addMessageListener("Link:AddSearch", this);
    mm.addMessageListener("Meta:SetPageInfo", this);
  },

  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "Link:LoadingIcon":
        if (aMsg.data.canUseForTab) {
          this.setPendingIcon(aMsg.target);
        }
        break;

      case "Link:SetIcon":
        this.setIconFromLink(
          aMsg.target,
          aMsg.data.pageURL,
          aMsg.data.originalURL,
          aMsg.data.canUseForTab,
          aMsg.data.expiration,
          aMsg.data.iconURL
        );
        break;

      case "Link:SetFailedIcon":
        if (aMsg.data.canUseForTab) {
          this.clearPendingIcon(aMsg.target);
        }
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
    const { url, description, previewImageURL } = aData;
    gBrowser.setPageInfo(url, description, previewImageURL);
    return true;
  },

  setPendingIcon(aBrowser) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (tab.hasAttribute("busy")) {
      tab.setAttribute("pendingicon", "true");
    }
  },

  clearPendingIcon(aBrowser) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    tab.removeAttribute("pendingicon");
  },

  setIconFromLink(
    aBrowser,
    aPageURL,
    aOriginalURL,
    aCanUseForTab,
    aExpiration,
    aIconURL
  ) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab) {
      return;
    }

    if (aCanUseForTab) {
      this.clearPendingIcon(aBrowser);
    }

    let iconURI;
    try {
      iconURI = Services.io.newURI(aIconURL);
    } catch (ex) {
      Cu.reportError(ex);
      return;
    }
    if (iconURI.scheme != "data") {
      try {
        Services.scriptSecurityManager.checkLoadURIWithPrincipal(
          aBrowser.contentPrincipal,
          iconURI,
          Services.scriptSecurityManager.ALLOW_CHROME
        );
      } catch (ex) {
        return;
      }
    }
    try {
      PlacesUIUtils.loadFavicon(
        aBrowser,
        Services.scriptSecurityManager.getSystemPrincipal(),
        makeURI(aPageURL),
        makeURI(aOriginalURL),
        aExpiration,
        iconURI
      );
    } catch (ex) {
      Cu.reportError(ex);
    }

    if (aCanUseForTab) {
      gBrowser.setIcon(tab, aIconURL, aOriginalURL);
    }
  },

  addSearch(aBrowser, aEngine, aURL) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (!tab) {
      return;
    }

    BrowserSearch.addEngine(aBrowser, aEngine, makeURI(aURL));
  },
};

const BrowserSearch = {
  _searchInitComplete: false,

  init() {
    Services.obs.addObserver(this, "browser-search-engine-modified");
  },

  delayedStartupInit() {
    // Asynchronously initialize the search service if necessary, to get the
    // current engine for working out the placeholder.
    Services.search.getDefault().then(defaultEngine => {
      // Delay the update for this until so that we don't change it while
      // the user is looking at it / isn't expecting it.
      this._updateURLBarPlaceholder(defaultEngine.name, true);
      this._searchInitComplete = true;
    });
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
      case "engine-default":
        if (this._searchInitComplete) {
          this._updateURLBarPlaceholder(engineName);
        }
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

  /**
   * Initializes the urlbar placeholder to the pre-saved engine name. We do this
   * via a preference, to avoid needing to synchronously init the search service.
   *
   * This should be called around the time of DOMContentLoaded, so that it is
   * initialized quickly before the user sees anything.
   *
   * Note: If the preference doesn't exist, we don't do anything as the default
   * placeholder is a string which doesn't have the engine name.
   */
  initPlaceHolder() {
    let engineName = Services.prefs.getStringPref(
      "browser.urlbar.placeholderName",
      ""
    );
    if (engineName) {
      // We can do this directly, since we know we're at DOMContentLoaded.
      this._setURLBarPlaceholder(engineName);
    }
  },

  /**
   * Updates the URLBar placeholder for the specified engine, delaying the
   * update if required. This also saves the current engine name in preferences
   * for the next restart.
   *
   * Note: The engine name will only be displayed for built-in engines, as we
   * know they should have short names.
   *
   * @param {String}  engineName     The search engine name to use for the update.
   * @param {Boolean} delayUpdate    Set to true, to delay update until the
   *                                 placeholder is not displayed.
   */
  async _updateURLBarPlaceholder(engineName, delayUpdate = false) {
    if (!engineName) {
      throw new Error("Expected an engineName to be specified");
    }

    let defaultEngines = await Services.search.getDefaultEngines();
    if (
      defaultEngines.some(defaultEngine => defaultEngine.name == engineName)
    ) {
      Services.prefs.setStringPref(
        "browser.urlbar.placeholderName",
        engineName
      );
    } else {
      Services.prefs.clearUserPref("browser.urlbar.placeholderName");
      // Set the engine name to an empty string for non-default engines, which'll
      // make sure we display the default placeholder string.
      engineName = "";
    }

    // Only delay if requested, and we're not displaying text in the URL bar
    // currently.
    if (delayUpdate && !gURLBar.value) {
      // Delays changing the URL Bar placeholder until the user is not going to be
      // seeing it, e.g. when there is a value entered in the bar, or if there is
      // a tab switch to a tab which has a url loaded.
      let placeholderUpdateListener = () => {
        if (gURLBar.value) {
          this._setURLBarPlaceholder(engineName);
          gURLBar.removeEventListener("input", placeholderUpdateListener);
          gBrowser.tabContainer.removeEventListener(
            "TabSelect",
            placeholderUpdateListener
          );
        }
      };

      gURLBar.addEventListener("input", placeholderUpdateListener);
      gBrowser.tabContainer.addEventListener(
        "TabSelect",
        placeholderUpdateListener
      );
    } else {
      this._setURLBarPlaceholder(engineName);
    }
  },

  /**
   * Sets the URLBar placeholder to either something based on the engine name,
   * or the default placeholder.
   *
   * @param {String} name The name of the engine to use, an empty string if to
   *                      use the default placeholder.
   */
  _setURLBarPlaceholder(name) {
    let placeholder;
    if (name) {
      placeholder = gBrowserBundle.formatStringFromName("urlbar.placeholder", [
        name,
      ]);
    } else {
      placeholder = gURLBar.getAttribute("defaultPlaceholder");
    }
    gURLBar.placeholder = placeholder;
  },

  addEngine(browser, engine, uri) {
    if (!this._searchInitComplete) {
      // We haven't finished initialising search yet. This means we can't
      // call getEngineByName here. Since this is only on start-up and unlikely
      // to happen in the normal case, we'll just return early rather than
      // trying to handle it asynchronously.
      return;
    }
    // Check to see whether we've already added an engine with this title
    if (browser.engines) {
      if (browser.engines.some(e => e.title == engine.title)) {
        return;
      }
    }

    var hidden = false;
    // If this engine (identified by title) is already in the list, add it
    // to the list of hidden engines rather than to the main list.
    // XXX This will need to be changed when engines are identified by URL;
    // see bug 335102.
    if (Services.search.getEngineByName(engine.title)) {
      hidden = true;
    }

    var engines = (hidden ? browser.hiddenEngines : browser.engines) || [];

    engines.push({
      uri: engine.href,
      title: engine.title,
      get icon() {
        return browser.mIconURL;
      },
    });

    if (hidden) {
      browser.hiddenEngines = engines;
    } else {
      browser.engines = engines;
      if (browser == gBrowser.selectedBrowser) {
        this.updateOpenSearchBadge();
      }
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
    if (!searchBar) {
      return;
    }

    var engines = gBrowser.selectedBrowser.engines;
    if (engines && engines.length > 0) {
      searchBar.setAttribute("addengines", "true");
    } else {
      searchBar.removeAttribute("addengines");
    }
  },

  /**
   * Focuses the search bar if present on the toolbar, or the address bar,
   * putting it in search mode. Will do so in an existing non-popup browser
   * window or open a new one if necessary.
   */
  webSearch: function BrowserSearch_webSearch() {
    if (
      window.location.href != AppConstants.BROWSER_CHROME_URL ||
      gURLBar.readOnly
    ) {
      let win = getTopWin(true);
      if (win) {
        // If there's an open browser window, it should handle this command
        win.focus();
        win.BrowserSearch.webSearch();
      } else {
        // If there are no open browser windows, open a new one
        var observer = function(subject, topic, data) {
          if (subject == win) {
            BrowserSearch.webSearch();
            Services.obs.removeObserver(
              observer,
              "browser-delayed-startup-finished"
            );
          }
        };
        win = window.openDialog(
          AppConstants.BROWSER_CHROME_URL,
          "_blank",
          "chrome,all,dialog=no",
          "about:blank"
        );
        Services.obs.addObserver(observer, "browser-delayed-startup-finished");
      }
      return;
    }

    let focusUrlBarIfSearchFieldIsNotActive = function(aSearchBar) {
      if (
        !aSearchBar ||
        document.activeElement != aSearchBar.textbox.inputField
      ) {
        // Limit the results to search suggestions, like the search bar.
        gURLBar.search(UrlbarTokenizer.RESTRICT.SEARCH);
      }
    };

    let searchBar = this.searchBar;
    let placement = CustomizableUI.getPlacementOfWidget("search-container");
    let focusSearchBar = () => {
      searchBar = this.searchBar;
      searchBar.select();
      focusUrlBarIfSearchFieldIsNotActive(searchBar);
    };
    if (
      placement &&
      searchBar &&
      ((searchBar.parentNode.getAttribute("overflowedItem") == "true" &&
        placement.area == CustomizableUI.AREA_NAVBAR) ||
        placement.area == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL)
    ) {
      let navBar = document.getElementById(CustomizableUI.AREA_NAVBAR);
      navBar.overflowable.show().then(focusSearchBar);
      return;
    }
    if (searchBar) {
      if (window.fullScreen) {
        FullScreen.showNavToolbox();
      }
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
  _loadSearch(searchText, useNewTab, purpose, triggeringPrincipal, csp) {
    if (!triggeringPrincipal) {
      throw new Error(
        "Required argument triggeringPrincipal missing within _loadSearch"
      );
    }

    let engine = Services.search.defaultEngine;

    let submission = engine.getSubmission(searchText, null, purpose); // HTML response

    // getSubmission can return null if the engine doesn't have a URL
    // with a text/html response type.  This is unlikely (since
    // SearchService._addEngineToStore() should fail for such an engine),
    // but let's be on the safe side.
    if (!submission) {
      return null;
    }

    let inBackground = Services.prefs.getBoolPref(
      "browser.search.context.loadInBackground"
    );
    openLinkIn(submission.uri.spec, useNewTab ? "tab" : "current", {
      postData: submission.postData,
      inBackground,
      relatedToCurrent: true,
      triggeringPrincipal,
      csp,
    });

    return engine;
  },

  /**
   * Perform a search initiated from the context menu.
   *
   * This should only be called from the context menu. See
   * BrowserSearch.loadSearch for the preferred API.
   */
  loadSearchFromContext(terms, triggeringPrincipal, csp) {
    let engine = BrowserSearch._loadSearch(
      terms,
      true,
      "contextmenu",
      triggeringPrincipal,
      csp
    );
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
    var newWindowPref = Services.prefs.getIntPref(
      "browser.link.open_newwindow"
    );
    var where = newWindowPref == 3 ? "tab" : "window";
    openTrustedLinkIn(this.searchEnginesURL, where);
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
    try {
      BrowserUsageTelemetry.recordSearch(gBrowser, engine, source, details);
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
   */
  recordOneoffSearchInTelemetry(engine, source, type) {
    try {
      const details = { type, isOneOff: true };
      BrowserUsageTelemetry.recordSearch(gBrowser, engine, source, details);
    } catch (ex) {
      Cu.reportError(ex);
    }
  },
};

XPCOMUtils.defineConstant(this, "BrowserSearch", BrowserSearch);

function FillHistoryMenu(aParent) {
  // Lazily add the hover listeners on first showing and never remove them
  if (!aParent.hasStatusListener) {
    // Show history item's uri in the status bar when hovering, and clear on exit
    aParent.addEventListener("DOMMenuItemActive", function(aEvent) {
      // Only the current page should have the checked attribute, so skip it
      if (!aEvent.target.hasAttribute("checked")) {
        XULBrowserWindow.setOverLink(aEvent.target.getAttribute("uri"));
      }
    });
    aParent.addEventListener("DOMMenuItemInactive", function() {
      XULBrowserWindow.setOverLink("");
    });

    aParent.hasStatusListener = true;
  }

  // Remove old entries if any
  let children = aParent.children;
  for (var i = children.length - 1; i >= 0; --i) {
    if (children[i].hasAttribute("index")) {
      aParent.removeChild(children[i]);
    }
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
    let end = Math.min(
      start == 0 ? MAX_HISTORY_MENU_ITEMS : index + half_length + 1,
      count
    );
    if (end == count) {
      start = Math.max(count - MAX_HISTORY_MENU_ITEMS, 0);
    }

    let existingIndex = 0;

    for (let j = end - 1; j >= start; j--) {
      let entry = sessionHistory.entries[j];
      let uri = entry.url;

      let item =
        existingIndex < children.length
          ? children[existingIndex]
          : document.createXULElement("menuitem");

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
        item.className =
          "unified-nav-back menuitem-iconic menuitem-with-favicon";
        item.setAttribute("tooltiptext", tooltipBack);
      } else if (j == index) {
        item.setAttribute("type", "radio");
        item.setAttribute("checked", "true");
        item.className = "unified-nav-current";
        item.setAttribute("tooltiptext", tooltipCurrent);
      } else {
        item.className =
          "unified-nav-forward menuitem-iconic menuitem-with-favicon";
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
        aParent.removeChild(aParent.lastElementChild);
        existingIndex++;
      }
    }
  }

  let sessionHistory = SessionStore.getSessionHistory(
    gBrowser.selectedTab,
    updateSessionHistory
  );
  if (!sessionHistory) {
    return false;
  }

  // don't display the popup for a single item
  if (sessionHistory.entries.length <= 1) {
    return false;
  }

  updateSessionHistory(sessionHistory, true);
  return true;
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

  if (topWindow) {
    topWindow.focus();
  } else if (features) {
    window.open(uri, "_blank", features);
  } else {
    window.open(
      uri,
      "_blank",
      "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar"
    );
  }
}

/**
 * Open a new browser window.
 *
 * @param {Object} options
 *        {
 *          private: A boolean indicating if the window should be
 *                   private
 *          remote:  A boolean indicating if the window should run
 *                   remote browser tabs or not. If omitted, the window
 *                   will choose the profile default state.
 *          fission: A boolean indicating if the window should run
 *                   with fission enabled or not. If omitted, the window
 *                   will choose the profile default state.
 *        }
 * @return a reference to the new window.
 */
function OpenBrowserWindow(options) {
  var telemetryObj = {};
  TelemetryStopwatch.start("FX_NEW_WINDOW_MS", telemetryObj);

  var handler = Cc["@mozilla.org/browser/clh;1"].getService(
    Ci.nsIBrowserHandler
  );
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

  if (options && options.fission) {
    extraFeatures += ",fission";
  } else if (options && options.fission === false) {
    extraFeatures += ",non-fission";
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
  if (
    window &&
    wintype == "navigator:browser" &&
    window.content &&
    window.content.document
  ) {
    var DocCharset = window.content.document.characterSet;
    let charsetArg = "charset=" + DocCharset;

    // we should "inherit" the charset menu setting in a new window
    win = window.openDialog(
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      "chrome,all,dialog=no" + extraFeatures,
      defaultArgs,
      charsetArg
    );
  } else {
    // forget about the charset information.
    win = window.openDialog(
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      "chrome,all,dialog=no" + extraFeatures,
      defaultArgs
    );
  }

  win.addEventListener(
    "MozAfterPaint",
    () => {
      TelemetryStopwatch.finish("FX_NEW_WINDOW_MS", telemetryObj);
      if (
        Services.prefs.getIntPref("browser.startup.page") == 1 &&
        defaultArgs == HomePage.get()
      ) {
        // A notification for when a user has triggered their homepage. This is used
        // to display a doorhanger explaining that an extension has modified the
        // homepage, if necessary.
        Services.obs.notifyObservers(win, "browser-open-homepage-start");
      }
    },
    { once: true }
  );

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
  if (AppConstants.platform == "macosx") {
    return;
  }

  let editMenuPopupState = document.getElementById("menu_EditPopup").state;
  let contextMenuPopupState = document.getElementById("contentAreaContextMenu")
    .state;
  let placesContextMenuPopupState = document.getElementById("placesContext")
    .state;

  let oldVisible = gEditUIVisible;

  // The UI is visible if the Edit menu is opening or open, if the context menu
  // is open, or if the toolbar has been customized to include the Cut, Copy,
  // or Paste toolbar buttons.
  gEditUIVisible =
    editMenuPopupState == "showing" ||
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
    } else if (
      areaType == CustomizableUI.TYPE_TOOLBAR &&
      window.toolbar.visible
    ) {
      // The edit controls are on a toolbar, so they are visible,
      // unless they're in a panel that isn't visible...
      if (placement.area == "nav-bar") {
        let editControls = document.getElementById("edit-controls");
        gEditUIVisible =
          !editControls.hasAttribute("overflowedItem") ||
          kOpenPopupStates.includes(
            document.getElementById("widget-overflow").state
          );
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
 * Updates User Context Menu Item UI visibility depending on
 * privacy.userContext.enabled pref state.
 */
function updateFileMenuUserContextUIVisibility(id) {
  let menu = document.getElementById(id);
  menu.hidden = !Services.prefs.getBoolPref(
    "privacy.userContext.enabled",
    false
  );
  // Visibility of File menu item shouldn't change frequently.
  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    menu.setAttribute("disabled", "true");
  }
}

/**
 * Updates the User Context UI indicators if the browser is in a non-default context
 */
function updateUserContextUIIndicator() {
  function replaceContainerClass(classType, element, value) {
    let prefix = "identity-" + classType + "-";
    if (value && element.classList.contains(prefix + value)) {
      return;
    }
    for (let className of element.classList) {
      if (className.startsWith(prefix)) {
        element.classList.remove(className);
      }
    }
    if (value) {
      element.classList.add(prefix + value);
    }
  }

  let hbox = document.getElementById("userContext-icons");

  let userContextId = gBrowser.selectedBrowser.getAttribute("usercontextid");
  if (!userContextId) {
    replaceContainerClass("color", hbox, "");
    hbox.hidden = true;
    return;
  }

  let identity = ContextualIdentityService.getPublicIdentityFromId(
    userContextId
  );
  if (!identity) {
    replaceContainerClass("color", hbox, "");
    hbox.hidden = true;
    return;
  }

  replaceContainerClass("color", hbox, identity.color);

  let label = document.getElementById("userContext-label");
  label.setAttribute(
    "value",
    ContextualIdentityService.getUserContextLabel(userContextId)
  );

  let indicator = document.getElementById("userContext-indicator");
  replaceContainerClass("icon", indicator, identity.icon);

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

  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsIWebProgressListener2",
    "nsISupportsWeakReference",
    "nsIXULBrowserWindow",
  ]),

  get stopCommand() {
    delete this.stopCommand;
    return (this.stopCommand = document.getElementById("Browser:Stop"));
  },
  get reloadCommand() {
    delete this.reloadCommand;
    return (this.reloadCommand = document.getElementById("Browser:Reload"));
  },
  get _elementsForTextBasedTypes() {
    delete this._elementsForTextBasedTypes;
    return (this._elementsForTextBasedTypes = [
      document.getElementById("pageStyleMenu"),
      document.getElementById("context-viewpartialsource-selection"),
    ]);
  },
  get _elementsForFind() {
    delete this._elementsForFind;
    return (this._elementsForFind = [
      document.getElementById("cmd_find"),
      document.getElementById("cmd_findAgain"),
      document.getElementById("cmd_findPrevious"),
    ]);
  },
  get _elementsForViewSource() {
    delete this._elementsForViewSource;
    return (this._elementsForViewSource = [
      document.getElementById("context-viewsource"),
      document.getElementById("View:PageSource"),
    ]);
  },

  forceInitialBrowserNonRemote(aOpener) {
    gBrowser.updateBrowserRemoteness(gBrowser.initialBrowser, {
      opener: aOpener,
      remoteType: E10SUtils.NOT_REMOTE,
    });
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
      url = url.replace(
        /[\u200e\u200f\u202a\u202b\u202c\u202d\u202e]/g,
        encodeURIComponent
      );

      if (UrlbarPrefs.get("trimURLs")) {
        url = BrowserUtils.trimURL(url);
      }
    }

    this.overLink = url;
    LinkTargetDisplay.update();
  },

  showTooltip(x, y, tooltip, direction, browser) {
    if (
      Cc["@mozilla.org/widget/dragservice;1"]
        .getService(Ci.nsIDragService)
        .getCurrentSession()
    ) {
      return;
    }

    // The x,y coordinates are relative to the <browser> element using
    // the chrome zoom level.
    let elt = document.getElementById("remoteBrowserTooltip");
    elt.label = tooltip;
    elt.style.direction = direction;

    let screenX;
    let screenY;

    if (browser instanceof XULElement) {
      // XUL element such as <browser> has the `screenX` and `screenY` fields.
      // https://searchfox.org/mozilla-central/source/dom/webidl/XULElement.webidl
      screenX = browser.screenX;
      screenY = browser.screenY;
    } else {
      // In case of HTML element such as <iframe> which RDM uses,
      // calculate the coordinate manually since it does not have the fields.
      const componentBounds = browser.getBoundingClientRect();
      screenX = window.screenX + componentBounds.x;
      screenY = window.screenY + componentBounds.y;
    }

    elt.openPopupAtScreen(screenX + x, screenY + y, false, null);
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
    return BrowserUtils.onBeforeLinkTraversal(
      originalTarget,
      linkURI,
      linkNode,
      isAppTab
    );
  },

  // Check whether this URI should load in the current process
  shouldLoadURI(
    aDocShell,
    aURI,
    aReferrerInfo,
    aHasPostData,
    aTriggeringPrincipal,
    aCsp
  ) {
    if (!gMultiProcessBrowser) {
      return true;
    }

    let browser = aDocShell
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .sameTypeRootTreeItem.QueryInterface(Ci.nsIDocShell).chromeEventHandler;

    // Ignore loads that aren't in the main tabbrowser
    if (
      browser.localName != "browser" ||
      !browser.getTabBrowser ||
      browser.getTabBrowser() != gBrowser
    ) {
      return true;
    }

    if (!E10SUtils.shouldLoadURI(aDocShell, aURI, aHasPostData)) {
      // XXX: Do we want to complain if we have post data but are still
      // redirecting the load? Perhaps a telemetry probe? Theoretically we
      // shouldn't do this, as it throws out data. See bug 1348018.
      E10SUtils.redirectLoad(
        aDocShell,
        aURI,
        aReferrerInfo,
        aTriggeringPrincipal,
        false,
        null,
        aCsp
      );
      return false;
    }

    return true;
  },

  onProgressChange(
    aWebProgress,
    aRequest,
    aCurSelfProgress,
    aMaxSelfProgress,
    aCurTotalProgress,
    aMaxTotalProgress
  ) {
    // Do nothing.
  },

  onProgressChange64(
    aWebProgress,
    aRequest,
    aCurSelfProgress,
    aMaxSelfProgress,
    aCurTotalProgress,
    aMaxTotalProgress
  ) {
    return this.onProgressChange(
      aWebProgress,
      aRequest,
      aCurSelfProgress,
      aMaxSelfProgress,
      aCurTotalProgress,
      aMaxTotalProgress
    );
  },

  // This function fires only for the currently selected tab.
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    const nsIWebProgressListener = Ci.nsIWebProgressListener;

    let browser = gBrowser.selectedBrowser;

    if (
      aStateFlags & nsIWebProgressListener.STATE_START &&
      aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK
    ) {
      if (aRequest && aWebProgress.isTopLevel) {
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
        if (aRequest instanceof Ci.nsIChannel || "URI" in aRequest) {
          location = aRequest.URI;

          // For keyword URIs clear the user typed value since they will be changed into real URIs
          if (location.scheme == "keyword" && aWebProgress.isTopLevel) {
            gBrowser.userTypedValue = null;
          }

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

        // Disable View Source menu entries for images, enable otherwise
        let isText =
          browser.documentContentType &&
          BrowserUtils.mimeTypeIsTextBased(browser.documentContentType);
        for (let element of this._elementsForViewSource) {
          if (canViewSource && isText) {
            element.removeAttribute("disabled");
          } else {
            element.setAttribute("disabled", "true");
          }
        }

        this._updateElementsForContentType();
      }

      this.isBusy = false;

      if (this.busyUI) {
        this.busyUI = false;

        this.stopCommand.setAttribute("disabled", "true");
        CombinedStopReload.switchToReload(aRequest, aWebProgress);
      }
    }
  },

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags, aIsSimulated) {
    var location = aLocationURI ? aLocationURI.spec : "";

    this.hideOverLinkImmediately = true;
    this.setOverLink("", null);
    this.hideOverLinkImmediately = false;

    // We should probably not do this if the value has changed since the user
    // searched
    // Update urlbar only if a new page was loaded on the primary content area
    // Do not update urlbar if there was a subframe navigation

    if (aWebProgress.isTopLevel) {
      if (
        (location == "about:blank" && checkEmptyPageOrigin()) ||
        location == ""
      ) {
        // Second condition is for new tabs, otherwise
        // reload function is enabled until tab is refreshed.
        this.reloadCommand.setAttribute("disabled", "true");
      } else {
        this.reloadCommand.removeAttribute("disabled");
      }

      // We want to update the popup visibility if we received this notification
      // via simulated locationchange events such as switching between tabs, however
      // if this is a document navigation then PopupNotifications will be updated
      // via TabsProgressListener.onLocationChange and we do not want it called twice
      URLBarSetURI(aLocationURI, aIsSimulated);

      BookmarkingUI.onLocationChange();

      gIdentityHandler.onLocationChange();

      gProtectionsHandler.onLocationChange();

      BrowserPageActions.onLocationChange();

      SafeBrowsingNotificationBox.onLocationChange(aLocationURI);

      gTabletModePageCounter.inc();

      this._updateElementsForContentType();

      // Try not to instantiate gCustomizeMode as much as possible,
      // so don't use CustomizeMode.jsm to check for URI or customizing.
      if (
        location == "about:blank" &&
        gBrowser.selectedTab.hasAttribute("customizemode")
      ) {
        gCustomizeMode.enter();
      } else if (
        CustomizationHandler.isEnteringCustomizeMode ||
        CustomizationHandler.isCustomizing()
      ) {
        gCustomizeMode.exit();
      }

      CFRPageActions.updatePageActions(gBrowser.selectedBrowser);
    }
    Services.obs.notifyObservers(null, "touchbar-location-change", location);
    UpdateBackForwardCommands(gBrowser.webNavigation);
    ReaderParent.updateReaderButton(gBrowser.selectedBrowser);

    if (!gMultiProcessBrowser) {
      // Bug 1108553 - Cannot rotate images with e10s
      gGestureSupport.restoreRotationState();
    }

    // See bug 358202, when tabs are switched during a drag operation,
    // timers don't fire on windows (bug 203573)
    if (aRequest) {
      setTimeout(function() {
        XULBrowserWindow.asyncUpdateUI();
      }, 0);
    } else {
      this.asyncUpdateUI();
    }

    if (AppConstants.MOZ_CRASHREPORTER && aLocationURI) {
      let uri = aLocationURI;
      try {
        // If the current URI contains a username/password, remove it.
        uri = aLocationURI
          .mutate()
          .setUserPass("")
          .finalize();
      } catch (ex) {
        /* Ignore failures on about: URIs. */
      }

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

  _updateElementsForContentType() {
    let browser = gBrowser.selectedBrowser;

    let isText =
      browser.documentContentType &&
      BrowserUtils.mimeTypeIsTextBased(browser.documentContentType);
    for (let element of this._elementsForTextBasedTypes) {
      if (isText) {
        element.removeAttribute("disabled");
      } else {
        element.setAttribute("disabled", "true");
      }
    }

    // Always enable find commands in PDF documents, otherwise do it only for
    // text documents whose location is not in the blacklist.
    let enableFind =
      browser.documentContentType == "application/pdf" ||
      (isText && BrowserUtils.canFindInPage(gBrowser.currentURI.spec));
    for (let element of this._elementsForFind) {
      if (enableFind) {
        element.removeAttribute("disabled");
      } else {
        element.setAttribute("disabled", "true");
      }
    }
  },

  asyncUpdateUI() {
    BrowserSearch.updateOpenSearchBadge();
  },

  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    this.status = aMessage;
    StatusPanel.update();
  },

  // Properties used to cache security state used to update the UI
  _state: null,
  _lastLocation: null,
  _event: null,
  _lastLocationForEvent: null,

  // This is called in multiple ways:
  //  1. Due to the nsIWebProgressListener.onContentBlockingEvent notification.
  //  2. Called by tabbrowser.xml when updating the current browser.
  //  3. Called directly during this object's initializations.
  //  4. Due to the nsIWebProgressListener.onLocationChange notification.
  // aRequest will be null always in case 2 and 3, and sometimes in case 1 (for
  // instance, there won't be a request when STATE_BLOCKED_TRACKING_CONTENT or
  // other blocking events are observed).
  onContentBlockingEvent(aWebProgress, aRequest, aEvent, aIsSimulated) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    let uri = gBrowser.currentURI;
    let spec = uri.spec;
    if (this._event == aEvent && this._lastLocationForEvent == spec) {
      return;
    }
    this._event = aEvent;
    this._lastLocationForEvent = spec;

    if (
      typeof aIsSimulated != "boolean" &&
      typeof aIsSimulated != "undefined"
    ) {
      throw new Error(
        "onContentBlockingEvent: aIsSimulated receieved an unexpected type"
      );
    }

    gProtectionsHandler.onContentBlockingEvent(
      this._event,
      aWebProgress,
      aIsSimulated
    );
    // Because this function will only receive content blocking event updates
    // for the currently selected tab, we handle updates to background tabs in
    // TabsProgressListener.onContentBlockingEvent.
    gBrowser.selectedBrowser.updateSecurityUIForContentBlockingEvent(aEvent);
  },

  // This is called in multiple ways:
  //  1. Due to the nsIWebProgressListener.onSecurityChange notification.
  //  2. Called by tabbrowser.xml when updating the current browser.
  //  3. Called directly during this object's initializations.
  // aRequest will be null always in case 2 and 3, and sometimes in case 1.
  onSecurityChange(aWebProgress, aRequest, aState, aIsSimulated) {
    // Don't need to do anything if the data we use to update the UI hasn't
    // changed
    let uri = gBrowser.currentURI;
    let spec = uri.spec;
    if (this._state == aState && this._lastLocation == spec) {
      // Switching to a tab of the same URL doesn't change most security
      // information, but tab specific permissions may be different.
      gIdentityHandler.refreshIdentityBlock();
      return;
    }
    this._state = aState;
    this._lastLocation = spec;

    // Make sure the "https" part of the URL is striked out or not,
    // depending on the current mixed active content blocking state.
    gURLBar.formatValue();

    try {
      uri = Services.uriFixup.createExposableURI(uri);
    } catch (e) {}
    gIdentityHandler.updateIdentity(this._state, uri);
  },

  // simulate all change notifications after switching tabs
  onUpdateCurrentBrowser: function XWB_onUpdateCurrentBrowser(
    aStateFlags,
    aStatus,
    aMessage,
    aTotalProgress
  ) {
    if (FullZoom.updateBackgroundTabs) {
      FullZoom.onLocationChange(gBrowser.currentURI, true);
    }

    CombinedStopReload.onTabSwitch();

    // Docshell should normally take care of hiding the tooltip, but we need to do it
    // ourselves for tabswitches.
    this.hideTooltip();

    // Also hide tooltips for content loaded in the parent process:
    document.getElementById("aHTMLTooltip").hidePopup();

    var nsIWebProgressListener = Ci.nsIWebProgressListener;
    var loadingDone = aStateFlags & nsIWebProgressListener.STATE_STOP;
    // use a pseudo-object instead of a (potentially nonexistent) channel for getting
    // a correct error message - and make sure that the UI is always either in
    // loading (STATE_START) or done (STATE_STOP) mode
    this.onStateChange(
      gBrowser.webProgress,
      { URI: gBrowser.currentURI },
      loadingDone
        ? nsIWebProgressListener.STATE_STOP
        : nsIWebProgressListener.STATE_START,
      aStatus
    );
    // status message and progress value are undefined if we're done with loading
    if (loadingDone) {
      return;
    }
    this.onStatusChange(gBrowser.webProgress, null, 0, aMessage);
  },

  navigateAndRestoreByIndex: function XWB_navigateAndRestoreByIndex(
    aBrowser,
    aIndex
  ) {
    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (tab) {
      SessionStore.navigateAndRestore(tab, {}, aIndex);
      return;
    }

    throw new Error(
      "Trying to navigateAndRestore a browser which was " +
        "not attached to this tabbrowser is unsupported"
    );
  },
};

var LinkTargetDisplay = {
  get DELAY_SHOW() {
    delete this.DELAY_SHOW;
    return (this.DELAY_SHOW = Services.prefs.getIntPref(
      "browser.overlink-delay"
    ));
  },

  DELAY_HIDE: 250,
  _timer: 0,

  update() {
    clearTimeout(this._timer);
    window.removeEventListener("mousemove", this, true);

    if (!XULBrowserWindow.overLink) {
      if (XULBrowserWindow.hideOverLinkImmediately) {
        this._hide();
      } else {
        this._timer = setTimeout(this._hide.bind(this), this.DELAY_HIDE);
      }
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
    this._timer = setTimeout(
      function(self) {
        StatusPanel.update();
        window.removeEventListener("mousemove", self, true);
      },
      this.DELAY_SHOW,
      this
    );
  },

  _hide() {
    clearTimeout(this._timer);

    StatusPanel.update();
  },
};

var CombinedStopReload = {
  // Try to initialize. Returns whether initialization was successful, which
  // may mean we had already initialized.
  ensureInitialized() {
    if (this._initialized) {
      return true;
    }
    if (this._destroyed) {
      return false;
    }

    let reload = document.getElementById("reload-button");
    let stop = document.getElementById("stop-button");
    // It's possible the stop/reload buttons have been moved to the palette.
    // They may be reinserted later, so we will retry initialization if/when
    // we get notified of document loads.
    if (!stop || !reload) {
      return false;
    }

    this._initialized = true;
    if (XULBrowserWindow.stopCommand.getAttribute("disabled") != "true") {
      reload.setAttribute("displaystop", "true");
    }
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

    if (!this._initialized) {
      return;
    }

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
        if (event.button == 0 && !this.stop.disabled) {
          this._stopClicked = true;
        }
        break;
      case "animationend": {
        if (
          event.target.classList.contains("toolbarbutton-animatable-image") &&
          (event.animationName == "reload-to-stop" ||
            event.animationName == "stop-to-reload" ||
            event.animationName == "reload-to-stop-rtl" ||
            event.animationName == "stop-to-reload-rtl")
        ) {
          this.stopReloadContainer.removeAttribute("animate");
        }
      }
    }
  },

  observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      this.animate = Services.prefs.getBoolPref(
        "toolkit.cosmeticAnimations.enabled"
      );
    }
  },

  startAnimationPrefMonitoring() {
    // CombinedStopReload may have been uninitialized before the idleCallback is executed.
    if (this._destroyed) {
      return;
    }
    if (!this.ensureInitialized()) {
      this._shouldStartPrefMonitoring = true;
      return;
    }
    this.animate =
      Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled") &&
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
    if (
      !this.ensureInitialized() ||
      !this._shouldSwitch(aRequest, aWebProgress)
    ) {
      return;
    }

    // Store the time that we switched to the stop button only if a request
    // is active. Requests are null if the switch is related to a tabswitch.
    // This is used to determine if we should show the stop->reload animation.
    if (aRequest instanceof Ci.nsIRequest) {
      this.timeWhenSwitchedToStop = window.performance.now();
    }

    let shouldAnimate =
      aRequest instanceof Ci.nsIRequest &&
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

    let shouldAnimate =
      aRequest instanceof Ci.nsIRequest &&
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
      this.reload.disabled =
        XULBrowserWindow.reloadCommand.getAttribute("disabled") == "true";
      return;
    }

    if (this._timer) {
      return;
    }

    // Temporarily disable the reload button to prevent the user from
    // accidentally reloading the page when intending to click the stop button
    this.reload.disabled = true;
    this._timer = setTimeout(
      function(self) {
        self._timer = 0;
        self.reload.disabled =
          XULBrowserWindow.reloadCommand.getAttribute("disabled") == "true";
      },
      650,
      this
    );
  },

  _loadTimeExceedsMinimumForAnimation() {
    // If the time between switching to the stop button then switching to
    // the reload button exceeds 150ms, then we will show the animation.
    // If we don't know when we switched to stop (switchToStop is called
    // after init but before switchToReload), then we will prevent the
    // animation from occuring.
    return (
      this.timeWhenSwitchedToStop &&
      window.performance.now() - this.timeWhenSwitchedToStop > 150
    );
  },

  _shouldSwitch(aRequest, aWebProgress) {
    if (
      aRequest &&
      aRequest.originalURI &&
      (aRequest.originalURI.schemeIs("chrome") ||
        (aRequest.originalURI.schemeIs("about") &&
          aWebProgress.isTopLevel &&
          !aRequest.originalURI.spec.startsWith("about:reader")))
    ) {
      return false;
    }

    return true;
  },

  _cancelTransition() {
    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = 0;
    }
  },
};

// This helper only cares about loading the frame
// script if the pref is seen as true.
// After the frame script is loaded, it takes over
// the responsibility of watching the pref and
// enabling/disabling itself.
const AccessibilityRefreshBlocker = {
  PREF: "accessibility.blockautorefresh",

  init() {
    if (Services.prefs.getBoolPref(this.PREF)) {
      this.loadFrameScript();
    } else {
      Services.prefs.addObserver(this.PREF, this);
    }
  },

  uninit() {
    Services.prefs.removeObserver(this.PREF, this);
  },

  observe(aSubject, aTopic, aPrefName) {
    if (
      aTopic == "nsPref:changed" &&
      aPrefName == this.PREF &&
      Services.prefs.getBoolPref(this.PREF)
    ) {
      this.loadFrameScript();
      Services.prefs.removeObserver(this.PREF, this);
    }
  },

  loadFrameScript() {
    if (!this._loaded) {
      this._loaded = true;
      let mm = window.getGroupMessageManager("browsers");
      mm.loadFrameScript(
        "chrome://browser/content/content-refreshblocker.js",
        true,
        true
      );
    }
  },
};

var TabsProgressListener = {
  onContentBlockingEvent(aBrowser, aWebProgress, aRequest, aEvent) {
    // Handle content blocking events for background (=non-selected) tabs.
    // This event is processed for the selected tab in XULBrowserWindow.onContentBlockingEvent.
    if (aBrowser != gBrowser.selectedBrowser) {
      aBrowser.updateSecurityUIForContentBlockingEvent(aEvent);
    }
  },

  onStateChange(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    // Collect telemetry data about tab load times.
    if (
      aWebProgress.isTopLevel &&
      (!aRequest.originalURI || aRequest.originalURI.scheme != "about")
    ) {
      let histogram = "FX_PAGE_LOAD_MS_2";
      let recordLoadTelemetry = true;

      if (aWebProgress.loadType & Ci.nsIDocShell.LOAD_CMD_RELOAD) {
        // loadType is constructed by shifting loadFlags, this is why we need to
        // do the same shifting here.
        // https://searchfox.org/mozilla-central/rev/11cfa0462a6b5d8c5e2111b8cfddcf78098f0141/docshell/base/nsDocShellLoadTypes.h#22
        if (aWebProgress.loadType & (kSkipCacheFlags << 16)) {
          histogram = "FX_PAGE_RELOAD_SKIP_CACHE_MS";
        } else if (aWebProgress.loadType == Ci.nsIDocShell.LOAD_CMD_RELOAD) {
          histogram = "FX_PAGE_RELOAD_NORMAL_MS";
        } else {
          recordLoadTelemetry = false;
        }
      }

      let stopwatchRunning = TelemetryStopwatch.running(histogram, aBrowser);
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
          if (stopwatchRunning) {
            // Oops, we're seeing another start without having noticed the previous stop.
            if (recordLoadTelemetry) {
              TelemetryStopwatch.cancel(histogram, aBrowser);
            }
          }
          if (recordLoadTelemetry) {
            TelemetryStopwatch.start(histogram, aBrowser);
          }
          Services.telemetry.getHistogramById("FX_TOTAL_TOP_VISITS").add(true);
        } else if (
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          stopwatchRunning /* we won't see STATE_START events for pre-rendered tabs */
        ) {
          if (recordLoadTelemetry) {
            TelemetryStopwatch.finish(histogram, aBrowser);
          }
        }
      } else if (
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStatus == Cr.NS_BINDING_ABORTED &&
        stopwatchRunning /* we won't see STATE_START events for pre-rendered tabs */
      ) {
        if (recordLoadTelemetry) {
          TelemetryStopwatch.cancel(histogram, aBrowser);
        }
      }
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
    if (!aWebProgress.isTopLevel) {
      return;
    }

    // Only need to call locationChange if the PopupNotifications object
    // for this window has already been initialized (i.e. its getter no
    // longer exists)
    if (!Object.getOwnPropertyDescriptor(window, "PopupNotifications").get) {
      PopupNotifications.locationChange(aBrowser);
    }

    let tab = gBrowser.getTabForBrowser(aBrowser);
    if (tab && tab._sharingState) {
      gBrowser.resetBrowserSharing(aBrowser);
    }
    webrtcUI.forgetStreamsFromBrowser(aBrowser);

    gBrowser.getNotificationBox(aBrowser).removeTransientNotifications();

    FullZoom.onLocationChange(aLocationURI, false, aBrowser);
  },

  onLinkIconAvailable(browser, dataURI, iconURI) {
    if (!iconURI) {
      return;
    }
    if (browser == gBrowser.selectedBrowser) {
      // If the "Add Search Engine" page action is in the urlbar, its image
      // needs to be set to the new icon, so call updateOpenSearchBadge.
      BrowserSearch.updateOpenSearchBadge();
    }
  },
};

function nsBrowserAccess() {}

nsBrowserAccess.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIBrowserDOMWindow]),

  _openURIInNewTab(
    aURI,
    aReferrerInfo,
    aIsPrivate,
    aIsExternal,
    aForceNotRemote = false,
    aUserContextId = Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID,
    aOpenerWindow = null,
    aOpenerBrowser = null,
    aTriggeringPrincipal = null,
    aNextRemoteTabId = 0,
    aName = "",
    aCsp = null
  ) {
    let win, needToFocusWin;

    // try the current window.  if we're in a popup, fall back on the most recent browser window
    if (window.toolbar.visible) {
      win = window;
    } else {
      win = BrowserWindowTracker.getTopWindow({ private: aIsPrivate });
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

    let loadInBackground = Services.prefs.getBoolPref(
      "browser.tabs.loadDivertedInBackground"
    );

    let tab = win.gBrowser.loadOneTab(aURI ? aURI.spec : "about:blank", {
      triggeringPrincipal: aTriggeringPrincipal,
      referrerInfo: aReferrerInfo,
      userContextId: aUserContextId,
      fromExternal: aIsExternal,
      inBackground: loadInBackground,
      forceNotRemote: aForceNotRemote,
      opener: aOpenerWindow,
      openerBrowser: aOpenerBrowser,
      nextRemoteTabId: aNextRemoteTabId,
      name: aName,
      csp: aCsp,
    });
    let browser = win.gBrowser.getBrowserForTab(tab);

    if (needToFocusWin || (!loadInBackground && aIsExternal)) {
      win.focus();
    }

    return browser;
  },

  createContentWindow(
    aURI,
    aOpener,
    aWhere,
    aFlags,
    aTriggeringPrincipal,
    aCsp
  ) {
    return this.getContentWindowOrOpenURI(
      null,
      aOpener,
      aWhere,
      aFlags,
      aTriggeringPrincipal,
      aCsp
    );
  },

  openURI(aURI, aOpener, aWhere, aFlags, aTriggeringPrincipal, aCsp) {
    if (!aURI) {
      Cu.reportError("openURI should only be called with a valid URI");
      throw Cr.NS_ERROR_FAILURE;
    }
    return this.getContentWindowOrOpenURI(
      aURI,
      aOpener,
      aWhere,
      aFlags,
      aTriggeringPrincipal,
      aCsp
    );
  },

  getContentWindowOrOpenURI(
    aURI,
    aOpener,
    aWhere,
    aFlags,
    aTriggeringPrincipal,
    aCsp
  ) {
    // This function should only ever be called if we're opening a URI
    // from a non-remote browser window (via nsContentTreeOwner).
    if (aOpener && Cu.isCrossProcessWrapper(aOpener)) {
      Cu.reportError(
        "nsBrowserAccess.openURI was passed a CPOW for aOpener. " +
          "openURI should only ever be called from non-remote browsers."
      );
      throw Cr.NS_ERROR_FAILURE;
    }

    var browsingContext = null;
    var isExternal = !!(aFlags & Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    if (aOpener && isExternal) {
      Cu.reportError(
        "nsBrowserAccess.openURI did not expect an opener to be " +
          "passed if the context is OPEN_EXTERNAL."
      );
      throw Cr.NS_ERROR_FAILURE;
    }

    if (isExternal && aURI && aURI.schemeIs("chrome")) {
      dump("use --chrome command-line option to load external chrome urls\n");
      return null;
    }

    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      if (
        isExternal &&
        Services.prefs.prefHasUserValue(
          "browser.link.open_newwindow.override.external"
        )
      ) {
        aWhere = Services.prefs.getIntPref(
          "browser.link.open_newwindow.override.external"
        );
      } else {
        aWhere = Services.prefs.getIntPref("browser.link.open_newwindow");
      }
    }

    let referrerInfo;
    if (aFlags & Ci.nsIBrowserDOMWindow.OPEN_NO_REFERRER) {
      referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, false, null);
    } else {
      referrerInfo = new ReferrerInfo(
        aOpener && aOpener.document
          ? aOpener.document.referrerInfo.referrerPolicy
          : Ci.nsIReferrerInfo.EMPTY,
        true,
        aOpener ? makeURI(aOpener.location.href) : null
      );
    }

    let isPrivate = aOpener
      ? PrivateBrowsingUtils.isContentWindowPrivate(aOpener)
      : PrivateBrowsingUtils.isWindowPrivate(window);

    switch (aWhere) {
      case Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW:
        // FIXME: Bug 408379. So how come this doesn't send the
        // referrer like the other loads do?
        var url = aURI ? aURI.spec : "about:blank";
        let features = "all,dialog=no";
        if (isPrivate) {
          features += ",private";
        }
        // Pass all params to openDialog to ensure that "url" isn't passed through
        // loadOneOrMoreURIs, which splits based on "|"
        try {
          openDialog(
            AppConstants.BROWSER_CHROME_URL,
            "_blank",
            features,
            // window.arguments
            url,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            aTriggeringPrincipal,
            null,
            aCsp
          );
          // At this point, the new browser window is just starting to load, and
          // hasn't created the content <browser> that we should return. So we
          // can't actually return a valid BrowsingContext for this load without
          // spinning the event loop.
          //
          // Fortunately, no current callers of this API who pass OPEN_NEWWINDOW
          // actually use the return value, so we're safe returning null for
          // now.
          //
          // Ideally this should be fixed.
          browsingContext = null;
        } catch (ex) {
          Cu.reportError(ex);
        }
        break;
      case Ci.nsIBrowserDOMWindow.OPEN_NEWTAB:
        // If we have an opener, that means that the caller is expecting access
        // to the nsIDOMWindow of the opened tab right away. For e10s windows,
        // this means forcing the newly opened browser to be non-remote so that
        // we can hand back the nsIDOMWindow. The XULBrowserWindow.shouldLoadURI
        // will do the job of shuttling off the newly opened browser to run in
        // the right process once it starts loading a URI.
        let forceNotRemote = !!aOpener;
        let userContextId =
          aOpener && aOpener.document
            ? aOpener.document.nodePrincipal.originAttributes.userContextId
            : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
        let openerWindow =
          aFlags & Ci.nsIBrowserDOMWindow.OPEN_NO_OPENER ? null : aOpener;
        let browser = this._openURIInNewTab(
          aURI,
          referrerInfo,
          isPrivate,
          isExternal,
          forceNotRemote,
          userContextId,
          openerWindow,
          null,
          aTriggeringPrincipal,
          0,
          "",
          aCsp
        );
        if (browser) {
          browsingContext = browser.browsingContext;
        }
        break;
      default:
        // OPEN_CURRENTWINDOW or an illegal value
        browsingContext =
          window.content && BrowsingContext.getFromWindow(window.content);
        if (aURI) {
          let loadflags = isExternal
            ? Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL
            : Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          gBrowser.loadURI(aURI.spec, {
            triggeringPrincipal: aTriggeringPrincipal,
            csp: aCsp,
            flags: loadflags,
            referrerInfo,
          });
        }
        if (
          !Services.prefs.getBoolPref("browser.tabs.loadDivertedInBackground")
        ) {
          window.focus();
        }
    }
    return browsingContext;
  },

  createContentWindowInFrame: function browser_createContentWindowInFrame(
    aURI,
    aParams,
    aWhere,
    aFlags,
    aNextRemoteTabId,
    aName
  ) {
    // Passing a null-URI to only create the content window.
    return this.getContentWindowOrOpenURIInFrame(
      null,
      aParams,
      aWhere,
      aFlags,
      aNextRemoteTabId,
      aName
    );
  },

  openURIInFrame: function browser_openURIInFrame(
    aURI,
    aParams,
    aWhere,
    aFlags,
    aNextRemoteTabId,
    aName
  ) {
    return this.getContentWindowOrOpenURIInFrame(
      aURI,
      aParams,
      aWhere,
      aFlags,
      aNextRemoteTabId,
      aName
    );
  },

  getContentWindowOrOpenURIInFrame: function browser_getContentWindowOrOpenURIInFrame(
    aURI,
    aParams,
    aWhere,
    aFlags,
    aNextRemoteTabId,
    aName
  ) {
    if (aWhere != Ci.nsIBrowserDOMWindow.OPEN_NEWTAB) {
      dump("Error: openURIInFrame can only open in new tabs");
      return null;
    }

    var isExternal = !!(aFlags & Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);

    var userContextId =
      aParams.openerOriginAttributes &&
      "userContextId" in aParams.openerOriginAttributes
        ? aParams.openerOriginAttributes.userContextId
        : Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;

    return this._openURIInNewTab(
      aURI,
      aParams.referrerInfo,
      aParams.isPrivate,
      isExternal,
      false,
      userContextId,
      null,
      aParams.openerBrowser,
      aParams.triggeringPrincipal,
      aNextRemoteTabId,
      aName,
      aParams.csp
    );
  },

  isTabContentWindow(aWindow) {
    return gBrowser.browsers.some(browser => browser.contentWindow == aWindow);
  },

  canClose() {
    return CanCloseWindow();
  },

  get tabCount() {
    return gBrowser.tabs.length;
  },
};

function onViewToolbarsPopupShowing(aEvent, aInsertPoint) {
  var popup = aEvent.target;
  if (popup != aEvent.currentTarget) {
    return;
  }

  // Empty the menu
  for (var i = popup.children.length - 1; i >= 0; --i) {
    var deadItem = popup.children[i];
    if (deadItem.hasAttribute("toolbarId")) {
      popup.removeChild(deadItem);
    }
  }

  var firstMenuItem = aInsertPoint || popup.firstElementChild;

  let toolbarNodes = gNavToolbox.querySelectorAll("toolbar");

  for (let toolbar of toolbarNodes) {
    if (!toolbar.hasAttribute("toolbarname")) {
      continue;
    }

    let menuItem = document.createXULElement("menuitem");
    let hidingAttribute =
      toolbar.getAttribute("type") == "menubar" ? "autohide" : "collapsed";
    menuItem.setAttribute("id", "toggle_" + toolbar.id);
    menuItem.setAttribute("toolbarId", toolbar.id);
    menuItem.setAttribute("type", "checkbox");
    menuItem.setAttribute("label", toolbar.getAttribute("toolbarname"));
    menuItem.setAttribute(
      "checked",
      toolbar.getAttribute(hidingAttribute) != "true"
    );
    menuItem.setAttribute("accesskey", toolbar.getAttribute("accesskey"));
    if (popup.id != "toolbar-context-menu") {
      menuItem.setAttribute("key", toolbar.getAttribute("key"));
    }

    popup.insertBefore(menuItem, firstMenuItem);

    menuItem.addEventListener("command", onViewToolbarCommand);
  }

  let moveToPanel = popup.querySelector(".customize-context-moveToPanel");
  let removeFromToolbar = popup.querySelector(
    ".customize-context-removeFromToolbar"
  );
  // View -> Toolbars menu doesn't have the moveToPanel or removeFromToolbar items.
  if (!moveToPanel || !removeFromToolbar) {
    return;
  }

  // triggerNode can be a nested child element of a toolbaritem.
  let toolbarItem = popup.triggerNode;

  if (toolbarItem && toolbarItem.localName == "toolbarpaletteitem") {
    toolbarItem = toolbarItem.firstElementChild;
  } else if (toolbarItem && toolbarItem.localName != "toolbar") {
    while (toolbarItem && toolbarItem.parentElement) {
      let parent = toolbarItem.parentElement;
      if (
        (parent.classList &&
          parent.classList.contains("customization-target")) ||
        parent.getAttribute("overflowfortoolbar") || // Needs to work in the overflow list as well.
        parent.localName == "toolbarpaletteitem" ||
        parent.localName == "toolbar"
      ) {
        break;
      }
      toolbarItem = parent;
    }
  } else {
    toolbarItem = null;
  }

  let showTabStripItems = toolbarItem && toolbarItem.id == "tabbrowser-tabs";
  for (let node of popup.querySelectorAll(
    'menuitem[contexttype="toolbaritem"]'
  )) {
    node.hidden = showTabStripItems;
  }

  for (let node of popup.querySelectorAll('menuitem[contexttype="tabbar"]')) {
    node.hidden = !showTabStripItems;
  }

  if (showTabStripItems) {
    let multipleTabsSelected = !!gBrowser.multiSelectedTabsCount;
    document.getElementById(
      "toolbar-context-bookmarkSelectedTabs"
    ).hidden = !multipleTabsSelected;
    document.getElementById(
      "toolbar-context-bookmarkSelectedTab"
    ).hidden = multipleTabsSelected;
    document.getElementById(
      "toolbar-context-reloadSelectedTabs"
    ).hidden = !multipleTabsSelected;
    document.getElementById(
      "toolbar-context-reloadSelectedTab"
    ).hidden = multipleTabsSelected;
    document.getElementById(
      "toolbar-context-selectAllTabs"
    ).disabled = gBrowser.allTabsSelected();
    document.getElementById("toolbar-context-undoCloseTab").disabled =
      SessionStore.getClosedTabCount(window) == 0;

    MozXULElement.insertFTLIfNeeded("browser/toolbarContextMenu.ftl");
    document
      .getElementById("toolbar-context-menu")
      .querySelectorAll("[data-lazy-l10n-id]")
      .forEach(el => {
        el.setAttribute("data-l10n-id", el.getAttribute("data-lazy-l10n-id"));
        el.removeAttribute("data-lazy-l10n-id");
      });
    return;
  }

  let movable =
    toolbarItem &&
    toolbarItem.id &&
    CustomizableUI.isWidgetRemovable(toolbarItem);
  if (movable) {
    if (CustomizableUI.isSpecialWidget(toolbarItem.id)) {
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
    Services.xulStore.persist(toolbar, hidingAttribute);
  }

  let eventParams = {
    detail: {
      visible: isVisible,
    },
    bubbles: true,
  };
  let event = new CustomEvent("toolbarvisibilitychange", eventParams);
  toolbar.dispatchEvent(event);

  if (
    toolbar.getAttribute("type") == "menubar" &&
    CustomizationHandler.isCustomizing()
  ) {
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
  let prefix = control.getAttribute("checked") == "true" ? "" : "un";
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
      let histogram = Services.telemetry.getKeyedHistogramById(
        "FX_TABLETMODE_PAGE_LOAD"
      );
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
    if (
      aTopic != "nsPref:changed" ||
      (aPrefName != this.uiDensityPref && aPrefName != this.autoTouchModePref)
    ) {
      return;
    }

    this.update();
  },

  getCurrentDensity() {
    // Automatically override the uidensity to touch in Windows tablet mode.
    if (
      AppConstants.isPlatformAndVersionAtLeast("win", "10") &&
      WindowsUIUtils.inTabletMode &&
      Services.prefs.getBoolPref(this.autoTouchModePref)
    ) {
      return { mode: this.MODE_TOUCH, overridden: true };
    }
    return {
      mode: Services.prefs.getIntPref(this.uiDensityPref),
      overridden: false,
    };
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
      let tree = SidebarUI.browser.contentDocument.querySelector(
        ".sidebar-placesTree"
      );
      if (tree) {
        // Tree items don't update their styles without changing some property on the
        // parent tree element, like background-color or border. See bug 1407399.
        tree.style.border = "1px";
        tree.style.border = "";
      }
    }

    gBrowser.tabContainer.uiDensityChanged();
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
function GetDynamicShortcutTooltipText(nodeId) {
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
    gDynamicTooltipCache.set(
      nodeId,
      gNavigatorBundle.getFormattedString(strId, args)
    );
  }
  return gDynamicTooltipCache.get(nodeId);
}

function UpdateDynamicShortcutTooltipText(aTooltip) {
  let nodeId =
    aTooltip.triggerNode.id || aTooltip.triggerNode.getAttribute("anonid");
  aTooltip.setAttribute("label", GetDynamicShortcutTooltipText(nodeId));
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
    return (
      (aNode instanceof HTMLAnchorElement && aNode.href) ||
      (aNode instanceof HTMLAreaElement && aNode.href) ||
      aNode instanceof HTMLLinkElement
    );
  }

  let node = event.composedTarget;
  while (node && !isHTMLLink(node)) {
    node = node.flattenedTreeParentNode;
  }

  if (node) {
    return [node.href, node];
  }

  // If there is no linkNode, try simple XLink.
  let href, baseURI;
  node = event.composedTarget;
  while (node && !href) {
    if (
      node.nodeType == Node.ELEMENT_NODE &&
      (node.localName == "a" ||
        node.namespaceURI == "http://www.w3.org/1998/Math/MathML")
    ) {
      href =
        node.getAttribute("href") ||
        node.getAttributeNS("http://www.w3.org/1999/xlink", "href");

      if (href) {
        baseURI = node.baseURI;
        break;
      }
    }
    node = node.flattenedTreeParentNode;
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
 *        Whether the event comes from an extension panel.
 * @note default event is prevented if the click is handled.
 */
function contentAreaClick(event, isPanelClick) {
  if (!event.isTrusted || event.defaultPrevented || event.button != 0) {
    return;
  }

  let [href, linkNode] = hrefAndLinkNodeForClickEvent(event);
  if (!href) {
    // Not a link, handle middle mouse navigation.
    if (
      event.button == 1 &&
      Services.prefs.getBoolPref("middlemouse.contentLoadURL") &&
      !Services.prefs.getBoolPref("general.autoScroll")
    ) {
      middleMousePaste(event);
      event.preventDefault();
    }
    return;
  }

  // This code only applies if we have a linkNode (i.e. clicks on real anchor
  // elements, as opposed to XLink).
  if (
    linkNode &&
    event.button == 0 &&
    !event.ctrlKey &&
    !event.shiftKey &&
    !event.altKey &&
    !event.metaKey
  ) {
    // An extension panel's links should target the main content area.  Do this
    // if no modifier keys are down and if there's no target or the target
    // equals _main (the IE convention) or _content (the Mozilla convention).
    let target = linkNode.target;
    let mainTarget = !target || target == "_content" || target == "_main";
    if (isPanelClick && mainTarget) {
      // javascript and data links should be executed in the current browser.
      if (
        linkNode.getAttribute("onclick") ||
        href.startsWith("javascript:") ||
        href.startsWith("data:")
      ) {
        return;
      }

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
  }

  handleLinkClick(event, href, linkNode);

  // Mark the page as a user followed link.  This is done so that history can
  // distinguish automatic embed visits from user activated ones.  For example
  // pages loaded in frames are embed visits and lost with the session, while
  // visits across frames should be preserved.
  try {
    if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
      PlacesUIUtils.markPageAsFollowedLink(href);
    }
  } catch (ex) {
    /* Skip invalid URIs. */
  }
}

/**
 * Handles clicks on links.
 *
 * @return true if the click event was handled, false otherwise.
 */
function handleLinkClick(event, href, linkNode) {
  if (event.button == 2) {
    // right click
    return false;
  }

  var where = whereToOpenLink(event);
  if (where == "current") {
    return false;
  }

  var doc = event.target.ownerDocument;
  let referrerInfo = Cc["@mozilla.org/referrer-info;1"].createInstance(
    Ci.nsIReferrerInfo
  );
  if (linkNode) {
    referrerInfo.initWithNode(linkNode);
  } else {
    referrerInfo.initWithDocument(doc);
  }

  if (where == "save") {
    saveURL(
      href,
      linkNode ? gatherTextUnder(linkNode) : "",
      null,
      true,
      true,
      referrerInfo,
      doc
    );
    event.preventDefault();
    return true;
  }

  // if the mixedContentChannel is present and the referring URI passes
  // a same origin check with the target URI, we can preserve the users
  // decision of disabling MCB on a page for it's child tabs.
  var persistAllowMixedContentInChildTab = false;

  if (where == "tab" && gBrowser.docShell.mixedContentChannel) {
    const sm = Services.scriptSecurityManager;
    try {
      var targetURI = makeURI(href);
      let isPrivateWin =
        doc.nodePrincipal.originAttributes.privateBrowsingId > 0;
      sm.checkSameOriginURI(
        doc.documentURIObject,
        targetURI,
        false,
        isPrivateWin
      );
      persistAllowMixedContentInChildTab = true;
    } catch (e) {}
  }

  let frameOuterWindowID = WebNavigationFrames.getFrameId(doc.defaultView);

  urlSecurityCheck(href, doc.nodePrincipal);
  let params = {
    charset: doc.characterSet,
    allowMixedContent: persistAllowMixedContentInChildTab,
    referrerInfo,
    originPrincipal: doc.nodePrincipal,
    originStoragePrincipal: doc.effectiveStoragePrincipal,
    triggeringPrincipal: doc.nodePrincipal,
    csp: doc.csp,
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
  if (!clipboard) {
    return;
  }

  // Strip embedded newlines and surrounding whitespace, to match the URL
  // bar's behavior (stripsurroundingwhitespace)
  clipboard = clipboard.replace(/\s*\n\s*/g, "");

  clipboard = UrlbarUtils.stripUnsafeProtocolOnPaste(clipboard);

  // if it's not the current tab, we don't need to do anything because the
  // browser doesn't exist.
  let where = whereToOpenLink(event, true, false);
  let lastLocationChange;
  if (where == "current") {
    lastLocationChange = gBrowser.selectedBrowser.lastLocationChange;
  }

  UrlbarUtils.getShortcutOrURIAndPostData(clipboard).then(data => {
    try {
      makeURI(data.url);
    } catch (ex) {
      // Not a valid URI.
      return;
    }

    try {
      UrlbarUtils.addToUrlbarHistory(data.url, window);
    } catch (ex) {
      // Things may go wrong when adding url to session history,
      // but don't let that interfere with the loading of the url.
      Cu.reportError(ex);
    }

    if (
      where != "current" ||
      lastLocationChange == gBrowser.selectedBrowser.lastLocationChange
    ) {
      openUILink(data.url, event, {
        ignoreButton: true,
        allowInheritPrincipal: data.mayInheritPrincipal,
        triggeringPrincipal: gBrowser.selectedBrowser.contentPrincipal,
        csp: gBrowser.selectedBrowser.csp,
      });
    }
  });

  if (event instanceof Event) {
    event.stopPropagation();
  }
}

// handleDroppedLink has the following 2 overloads:
//   handleDroppedLink(event, url, name, triggeringPrincipal)
//   handleDroppedLink(event, links, triggeringPrincipal)
function handleDroppedLink(
  event,
  urlOrLinks,
  nameOrTriggeringPrincipal,
  triggeringPrincipal
) {
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
    if (event.shiftKey) {
      inBackground = !inBackground;
    }
  }

  (async function() {
    if (
      links.length >=
      Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")
    ) {
      // Sync dialog cannot be used inside drop event handler.
      let answer = await OpenInTabsUtils.promiseConfirmOpenInTabs(
        links.length,
        window
      );
      if (!answer) {
        return;
      }
    }

    let urls = [];
    let postDatas = [];
    for (let link of links) {
      let data = await UrlbarUtils.getShortcutOrURIAndPostData(link.url);
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
    if (aCharset == "Japanese") {
      aCharset = "Shift_JIS";
    }
    gBrowser.selectedBrowser.characterSet = aCharset;
    // Save the forced character-set
    PlacesUIUtils.setCharsetForPage(
      getWebNavigation().currentURI,
      aCharset,
      window
    ).catch(Cu.reportError);
  }
  BrowserCharsetReload();
}

function BrowserCharsetReload() {
  BrowserReloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function UpdateCurrentCharset(target) {
  let selectedCharset = CharsetMenu.foldCharset(
    gBrowser.selectedBrowser.characterSet,
    gBrowser.selectedBrowser.charsetAutodetected
  );
  for (let menuItem of target.getElementsByTagName("menuitem")) {
    let isSelected = menuItem.getAttribute("charset") === selectedCharset;
    menuItem.setAttribute("checked", isSelected);
  }
}

function promptRemoveExtension(addon) {
  let { name } = addon;
  let brand = document
    .getElementById("bundle_brand")
    .getString("brandShorterName");
  let { getFormattedString, getString } = gNavigatorBundle;
  let title = getFormattedString("webext.remove.confirmation.title", [name]);
  let message = getFormattedString("webext.remove.confirmation.message", [
    name,
    brand,
  ]);
  let btnTitle = getString("webext.remove.confirmation.button");
  let {
    BUTTON_TITLE_IS_STRING: titleString,
    BUTTON_TITLE_CANCEL: titleCancel,
    BUTTON_POS_0,
    BUTTON_POS_1,
    confirmEx,
  } = Services.prompt;
  let btnFlags = BUTTON_POS_0 * titleString + BUTTON_POS_1 * titleCancel;
  let checkboxState = { value: false };
  let checkboxMessage = null;

  // Enable abuse report checkbox in the remove extension dialog,
  // if enabled by the about:config prefs and the addon type
  // is currently supported.
  if (
    gHtmlAboutAddonsEnabled &&
    gAddonAbuseReportEnabled &&
    ["extension", "theme"].includes(addon.type)
  ) {
    checkboxMessage = getFormattedString(
      "webext.remove.abuseReportCheckbox.message",
      [document.getElementById("bundle_brand").getString("vendorShortName")]
    );
  }
  const result = confirmEx(
    null,
    title,
    message,
    btnFlags,
    btnTitle,
    null,
    null,
    checkboxMessage,
    checkboxState
  );
  return { remove: result === 0, report: checkboxState.value };
}

var ToolbarContextMenu = {
  updateDownloadsAutoHide(popup) {
    let checkbox = document.getElementById(
      "toolbar-context-autohide-downloads-button"
    );
    let isDownloads =
      popup.triggerNode &&
      ["downloads-button", "wrapper-downloads-button"].includes(
        popup.triggerNode.id
      );
    checkbox.hidden = !isDownloads;
    if (DownloadsButton.autoHideDownloadsButton) {
      checkbox.setAttribute("checked", "true");
    } else {
      checkbox.removeAttribute("checked");
    }
  },

  onDownloadsAutoHideChange(event) {
    let autoHide = event.target.getAttribute("checked") == "true";
    Services.prefs.setBoolPref("browser.download.autohideButton", autoHide);
  },

  _getUnwrappedTriggerNode(popup) {
    // Toolbar buttons are wrapped in customize mode. Unwrap if necessary.
    let { triggerNode } = popup;
    if (triggerNode && gCustomizeMode.isWrappedToolbarItem(triggerNode)) {
      return triggerNode.firstElementChild;
    }
    return triggerNode;
  },

  _getExtensionId(popup) {
    let node = this._getUnwrappedTriggerNode(popup);
    return node && node.getAttribute("data-extensionid");
  },

  async updateExtension(popup) {
    let removeExtension = popup.querySelector(
      ".customize-context-removeExtension"
    );
    let manageExtension = popup.querySelector(
      ".customize-context-manageExtension"
    );
    let reportExtension = popup.querySelector(
      ".customize-context-reportExtension"
    );
    let separator = reportExtension.nextElementSibling;
    let id = this._getExtensionId(popup);
    let addon = id && (await AddonManager.getAddonByID(id));

    for (let element of [removeExtension, manageExtension, separator]) {
      element.hidden = !addon;
    }

    reportExtension.hidden =
      !addon || !gAddonAbuseReportEnabled || !gHtmlAboutAddonsEnabled;

    if (addon) {
      removeExtension.disabled = !(
        addon.permissions & AddonManager.PERM_CAN_UNINSTALL
      );
    }
  },

  async removeExtensionForContextAction(popup) {
    let id = this._getExtensionId(popup);
    let addon = id && (await AddonManager.getAddonByID(id));
    if (!addon || !(addon.permissions & AddonManager.PERM_CAN_UNINSTALL)) {
      return;
    }
    let { remove, report } = promptRemoveExtension(addon);
    AMTelemetry.recordActionEvent({
      object: "browserAction",
      action: "uninstall",
      value: remove ? "accepted" : "cancelled",
      extra: { addonId: addon.id },
    });
    if (remove) {
      // Leave the extension in pending uninstall if we are also
      // reporting the add-on.
      await addon.uninstall(report);
      if (report) {
        this.reportExtensionForContextAction(popup, "uninstall");
      }
    }
  },

  async reportExtensionForContextAction(popup, reportEntryPoint) {
    let id = this._getExtensionId(popup);
    let addon = id && (await AddonManager.getAddonByID(id));
    if (!addon) {
      return;
    }
    const win = await BrowserOpenAddonsMgr("addons://list/extension");
    win.openAbuseReport({
      addonId: addon.id,
      reportEntryPoint,
    });
  },

  openAboutAddonsForContextAction(popup) {
    let id = this._getExtensionId(popup);
    if (id) {
      let viewID = "addons://detail/" + encodeURIComponent(id);
      BrowserOpenAddonsMgr(viewID);
      AMTelemetry.recordActionEvent({
        object: "browserAction",
        action: "manage",
        extra: { addonId: id },
      });
    }
  },
};

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
    mm.addMessageListener("PageStyle:StyleSheets", msg => {
      if (msg.target.permanentKey) {
        this._pageStyleSheets.set(msg.target.permanentKey, msg.data);
      }
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
        preferredStyleSheetSet: true,
      };
    }

    return data;
  },

  fillPopup(menuPopup) {
    let styleSheetInfo = this._getStyleSheetInfo(gBrowser.selectedBrowser);
    var noStyle = menuPopup.firstElementChild;
    var persistentOnly = noStyle.nextElementSibling;
    var sep = persistentOnly.nextElementSibling;
    while (sep.nextElementSibling) {
      menuPopup.removeChild(sep.nextElementSibling);
    }

    let styleSheets = styleSheetInfo.filteredStyleSheets;
    var currentStyleSheets = {};
    var styleDisabled = styleSheetInfo.authorStyleDisabled;
    var haveAltSheets = false;
    var altStyleSelected = false;

    for (let currentStyleSheet of styleSheets) {
      if (!currentStyleSheet.disabled) {
        altStyleSelected = true;
      }

      haveAltSheets = true;

      let lastWithSameTitle = null;
      if (currentStyleSheet.title in currentStyleSheets) {
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];
      }

      if (!lastWithSameTitle) {
        let menuItem = document.createXULElement("menuitem");
        menuItem.setAttribute("type", "radio");
        menuItem.setAttribute("label", currentStyleSheet.title);
        menuItem.setAttribute("data", currentStyleSheet.title);
        menuItem.setAttribute(
          "checked",
          !currentStyleSheet.disabled && !styleDisabled
        );
        menuItem.setAttribute(
          "oncommand",
          "gPageStyleMenu.switchStyleSheet(this.getAttribute('data'));"
        );
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else if (currentStyleSheet.disabled) {
        lastWithSameTitle.removeAttribute("checked");
      }
    }

    noStyle.setAttribute("checked", styleDisabled);
    persistentOnly.setAttribute("checked", !altStyleSelected && !styleDisabled);
    persistentOnly.hidden = styleSheetInfo.preferredStyleSheetSet
      ? haveAltSheets
      : false;
    sep.hidden = (noStyle.hidden && persistentOnly.hidden) || !haveAltSheets;
  },

  switchStyleSheet(title) {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.sendAsyncMessage("PageStyle:Switch", { title });
  },

  disableStyle() {
    let mm = gBrowser.selectedBrowser.messageManager;
    mm.sendAsyncMessage("PageStyle:Disable");
  },
};

var LanguageDetectionListener = {
  init() {
    window.messageManager.addMessageListener(
      "Translation:DocumentState",
      msg => {
        Translation.documentStateReceived(msg.target, msg.data);
      }
    );
  },
};

// Note that this is also called from non-browser windows on OSX, which do
// share menu items but not much else. See nonbrowser-mac.js.
var BrowserOffline = {
  _inited: false,

  // BrowserOffline Public Methods
  init() {
    if (!this._uiElement) {
      this._uiElement = document.getElementById("cmd_toggleOfflineStatus");
    }

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
    if (aTopic != "network:offline-status-changed") {
      return;
    }

    // This notification is also received because of a loss in connectivity,
    // which we ignore by updating the UI to the current value of io.offline
    this._updateOfflineUI(Services.io.offline);
  },

  // BrowserOffline Implementation Methods
  _canGoOffline() {
    try {
      var cancelGoOffline = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
        Ci.nsISupportsPRBool
      );
      Services.obs.notifyObservers(cancelGoOffline, "offline-requested");

      // Something aborted the quit process.
      if (cancelGoOffline.data) {
        return false;
      }
    } catch (ex) {}

    return true;
  },

  _uiElement: null,
  _updateOfflineUI(aOffline) {
    var offlineLocked = Services.prefs.prefIsLocked("network.online");
    if (offlineLocked) {
      this._uiElement.setAttribute("disabled", "true");
    }

    this._uiElement.setAttribute("checked", aOffline);
  },
};

var OfflineApps = {
  warnUsage(browser, uri) {
    if (!browser) {
      return;
    }

    let mainAction = {
      label: gNavigatorBundle.getString("offlineApps.manageUsage"),
      accessKey: gNavigatorBundle.getString("offlineApps.manageUsageAccessKey"),
      callback: this.manage,
    };

    let warnQuotaKB = Services.prefs.getIntPref("offline-apps.quota.warn");
    // This message shows the quota in MB, and so we divide the quota (in kb) by 1024.
    let message = gNavigatorBundle.getFormattedString("offlineApps.usage", [
      uri.host,
      warnQuotaKB / 1024,
    ]);

    let anchorID = "indexedDB-notification-icon";
    let options = {
      persistent: true,
      hideClose: true,
    };
    PopupNotifications.show(
      browser,
      "offline-app-usage",
      message,
      anchorID,
      mainAction,
      null,
      options
    );

    // Now that we've warned once, prevent the warning from showing up
    // again.
    Services.perms.add(
      uri,
      "offline-app",
      Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN
    );
  },

  // XXX: duplicated in preferences/advanced.js
  _getOfflineAppUsage(host, groups) {
    let cacheService = Cc[
      "@mozilla.org/network/application-cache-service;1"
    ].getService(Ci.nsIApplicationCacheService);
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
    if (
      Services.perms.testExactPermission(uri, "offline-app") !=
      Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN
    ) {
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
    let notification = PopupNotifications.getNotification(
      notificationID,
      browser
    );

    if (notification) {
      notification.options.controlledItems.push([
        Cu.getWeakReference(browser),
        docId,
        uri,
      ]);
    } else {
      let mainAction = {
        label: gNavigatorBundle.getString("offlineApps.allowStoring.label"),
        accessKey: gNavigatorBundle.getString(
          "offlineApps.allowStoring.accesskey"
        ),
        callback() {
          for (let [ciBrowser, ciDocId, ciUri] of notification.options
            .controlledItems) {
            OfflineApps.allowSite(ciBrowser, ciDocId, ciUri);
          }
        },
      };
      let secondaryActions = [
        {
          label: gNavigatorBundle.getString("offlineApps.dontAllow.label"),
          accessKey: gNavigatorBundle.getString(
            "offlineApps.dontAllow.accesskey"
          ),
          callback() {
            for (let [, , ciUri] of notification.options.controlledItems) {
              OfflineApps.disallowSite(ciUri);
            }
          },
        },
      ];
      let message = gNavigatorBundle.getFormattedString(
        "offlineApps.available2",
        [host]
      );
      let anchorID = "indexedDB-notification-icon";
      let options = {
        persistent: true,
        hideClose: true,
        controlledItems: [[Cu.getWeakReference(browser), docId, uri]],
      };
      notification = PopupNotifications.show(
        browser,
        notificationID,
        message,
        anchorID,
        mainAction,
        secondaryActions,
        options
      );
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
    openPreferences("panePrivacy");
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
        this.requestPermission(
          msg.target,
          msg.data.docId,
          makeURI(msg.data.uri)
        );
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

  init: function IndexedDBPromptHelper_init() {
    Services.obs.addObserver(this, this._permissionsPrompt);
  },

  uninit: function IndexedDBPromptHelper_uninit() {
    Services.obs.removeObserver(this, this._permissionsPrompt);
  },

  observe: function IndexedDBPromptHelper_observe(subject, topic, data) {
    if (topic != this._permissionsPrompt) {
      throw new Error("Unexpected topic!");
    }

    var request = subject.QueryInterface(Ci.nsIIDBPermissionsRequest);

    var browser = request.browserElement;
    if (browser.ownerGlobal != window) {
      // Only listen for notifications for browsers in our chrome window.
      return;
    }

    // Get the host name if available or the file path otherwise.
    var host = browser.currentURI.asciiHost || browser.currentURI.pathQueryRef;

    var message;
    var responseTopic;
    if (topic == this._permissionsPrompt) {
      message = gNavigatorBundle.getFormattedString("offlineApps.available2", [
        host,
      ]);
      responseTopic = this._permissionsResponse;
    }

    var observer = request.responseObserver;

    var mainAction = {
      label: gNavigatorBundle.getString("offlineApps.allowStoring.label"),
      accessKey: gNavigatorBundle.getString(
        "offlineApps.allowStoring.accesskey"
      ),
      callback() {
        observer.observe(
          null,
          responseTopic,
          Ci.nsIPermissionManager.ALLOW_ACTION
        );
      },
    };

    var secondaryActions = [
      {
        label: gNavigatorBundle.getString("offlineApps.dontAllow.label"),
        accessKey: gNavigatorBundle.getString(
          "offlineApps.dontAllow.accesskey"
        ),
        callback() {
          observer.observe(
            null,
            responseTopic,
            Ci.nsIPermissionManager.DENY_ACTION
          );
        },
      },
    ];

    PopupNotifications.show(
      browser,
      topic,
      message,
      this._notificationIcon,
      mainAction,
      secondaryActions,
      {
        persistent: true,
        hideClose: true,
      }
    );
  },
};

var CanvasPermissionPromptHelper = {
  _permissionsPrompt: "canvas-permissions-prompt",
  _permissionsPromptHideDoorHanger: "canvas-permissions-prompt-hide-doorhanger",
  _notificationIcon: "canvas-notification-icon",

  init() {
    Services.obs.addObserver(this, this._permissionsPrompt);
    Services.obs.addObserver(this, this._permissionsPromptHideDoorHanger);
  },

  uninit() {
    Services.obs.removeObserver(this, this._permissionsPrompt);
    Services.obs.removeObserver(this, this._permissionsPromptHideDoorHanger);
  },

  // aSubject is an nsIBrowser (e10s) or an nsIDOMWindow (non-e10s).
  // aData is an Origin string.
  observe(aSubject, aTopic, aData) {
    if (
      aTopic != this._permissionsPrompt &&
      aTopic != this._permissionsPromptHideDoorHanger
    ) {
      return;
    }

    let browser;
    if (aSubject instanceof Ci.nsIDOMWindow) {
      browser = aSubject.docShell.chromeEventHandler;
    } else {
      browser = aSubject;
    }

    if (gBrowser.selectedBrowser !== browser) {
      // Must belong to some other window.
      return;
    }

    let message = gNavigatorBundle.getFormattedString(
      "canvas.siteprompt",
      ["<>"],
      1
    );

    let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      aData
    );

    function setCanvasPermission(aPerm, aPersistent) {
      Services.perms.addFromPrincipal(
        principal,
        "canvas",
        aPerm,
        aPersistent
          ? Ci.nsIPermissionManager.EXPIRE_NEVER
          : Ci.nsIPermissionManager.EXPIRE_SESSION
      );
    }

    let mainAction = {
      label: gNavigatorBundle.getString("canvas.allow"),
      accessKey: gNavigatorBundle.getString("canvas.allow.accesskey"),
      callback(state) {
        setCanvasPermission(
          Ci.nsIPermissionManager.ALLOW_ACTION,
          state && state.checkboxChecked
        );
      },
    };

    let secondaryActions = [
      {
        label: gNavigatorBundle.getString("canvas.notAllow"),
        accessKey: gNavigatorBundle.getString("canvas.notAllow.accesskey"),
        callback(state) {
          setCanvasPermission(
            Ci.nsIPermissionManager.DENY_ACTION,
            state && state.checkboxChecked
          );
        },
      },
    ];

    let checkbox = {
      // In PB mode, we don't want the "always remember" checkbox
      show: !PrivateBrowsingUtils.isWindowPrivate(window),
    };
    if (checkbox.show) {
      checkbox.checked = true;
      checkbox.label = gBrowserBundle.GetStringFromName("canvas.remember");
    }

    let options = {
      checkbox,
      name: principal.URI.host,
      learnMoreURL:
        Services.urlFormatter.formatURLPref("app.support.baseURL") +
        "fingerprint-permission",
      dismissed: aTopic == this._permissionsPromptHideDoorHanger,
    };
    PopupNotifications.show(
      browser,
      this._permissionsPrompt,
      message,
      this._notificationIcon,
      mainAction,
      secondaryActions,
      options
    );
  },
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

  register(mgr, { origin, tid }) {
    let mainAction = this.buildCancelAction(mgr, tid);
    this.show(tid, "register", "webauthn.registerPrompt2", origin, mainAction);
  },

  registerDirect(mgr, { origin, tid }) {
    let mainAction = this.buildProceedAction(mgr, tid);
    let secondaryActions = [this.buildCancelAction(mgr, tid)];

    let learnMoreURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "webauthn-direct-attestation";

    let options = {
      learnMoreURL,
      checkbox: {
        label: gNavigatorBundle.getString("webauthn.anonymize"),
      },
    };

    this.show(
      tid,
      "register-direct",
      "webauthn.registerDirectPrompt2",
      origin,
      mainAction,
      secondaryActions,
      options
    );
  },

  sign(mgr, { origin, tid }) {
    let mainAction = this.buildCancelAction(mgr, tid);
    this.show(tid, "sign", "webauthn.signPrompt2", origin, mainAction);
  },

  show(
    tid,
    id,
    stringId,
    origin,
    mainAction,
    secondaryActions = [],
    options = {}
  ) {
    this.reset();

    try {
      origin = Services.io.newURI(origin).asciiHost;
    } catch (e) {
      /* Might fail for arbitrary U2F RP IDs. */
    }

    let brandShortName = document
      .getElementById("bundle_brand")
      .getString("brandShortName");
    let message = gNavigatorBundle.getFormattedString(
      stringId,
      ["<>", brandShortName],
      1
    );

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
      gBrowser.selectedBrowser,
      `webauthn-prompt-${id}`,
      message,
      this._icon,
      mainAction,
      secondaryActions,
      options
    );
  },

  cancel({ tid }) {
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
        mgr.resumeRegister(tid, state.checkboxChecked);
      },
    };
  },

  buildCancelAction(mgr, tid) {
    return {
      label: gNavigatorBundle.getString("webauthn.cancel"),
      accessKey: gNavigatorBundle.getString("webauthn.cancel.accesskey"),
      callback() {
        mgr.cancel(tid);
      },
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

    let { permitUnload, timedOut } = browser.permitUnload();

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
  if (!closeWindow(false, warnAboutClosingWindow)) {
    return false;
  }

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
  let isPBWindow =
    PrivateBrowsingUtils.isWindowPrivate(window) &&
    !PrivateBrowsingUtils.permanentPrivateBrowsing;

  let closingTabs = gBrowser.tabs.length - gBrowser._removingTabs.length;

  if (!isPBWindow && !toolbar.visible) {
    return gBrowser.warnAboutClosingTabs(
      closingTabs,
      gBrowser.closingTabsEnum.ALL
    );
  }

  // Figure out if there's at least one other browser window around.
  let otherPBWindowExists = false;
  let otherWindowExists = false;
  for (let win of browserWindows()) {
    if (!win.closed && win != window) {
      otherWindowExists = true;
      if (isPBWindow && PrivateBrowsingUtils.isWindowPrivate(win)) {
        otherPBWindowExists = true;
      }
      // If the current window is not in private browsing mode we don't need to
      // look for other pb windows, we can leave the loop when finding the
      // first non-popup window. If however the current window is in private
      // browsing mode then we need at least one other pb and one non-popup
      // window to break out early.
      if (!isPBWindow || otherPBWindowExists) {
        break;
      }
    }
  }

  if (isPBWindow && !otherPBWindowExists) {
    let exitingCanceled = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    exitingCanceled.data = false;
    Services.obs.notifyObservers(exitingCanceled, "last-pb-context-exiting");
    if (exitingCanceled.data) {
      return false;
    }
  }

  if (otherWindowExists) {
    return (
      isPBWindow ||
      gBrowser.warnAboutClosingTabs(closingTabs, gBrowser.closingTabsEnum.ALL)
    );
  }

  let os = Services.obs;

  let closingCanceled = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );
  os.notifyObservers(closingCanceled, "browser-lastwindow-close-requested");
  if (closingCanceled.data) {
    return false;
  }

  os.notifyObservers(null, "browser-lastwindow-close-granted");

  // OS X doesn't quit the application when the last window is closed, but keeps
  // the session alive. Hence don't prompt users to save tabs, but warn about
  // closing multiple tabs.
  return (
    AppConstants.platform != "macosx" ||
    (isPBWindow ||
      gBrowser.warnAboutClosingTabs(closingTabs, gBrowser.closingTabsEnum.ALL))
  );
}

var MailIntegration = {
  sendLinkForBrowser(aBrowser) {
    this.sendMessage(
      gURLBar.makeURIReadable(aBrowser.currentURI).displaySpec,
      aBrowser.contentTitle
    );
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
    var extProtocolSvc = Cc[
      "@mozilla.org/uriloader/external-protocol-service;1"
    ].getService(Ci.nsIExternalProtocolService);
    if (extProtocolSvc) {
      extProtocolSvc.loadURI(aURL);
    }
  },
};

function BrowserOpenAddonsMgr(aView) {
  return new Promise(resolve => {
    let emWindow;
    let browserWindow;

    var receivePong = function(aSubject, aTopic, aData) {
      let browserWin = aSubject.docShell.rootTreeItem.domWindow;
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
      let tab = browserWindow.gBrowser.getTabForBrowser(
        emWindow.docShell.chromeEventHandler
      );
      browserWindow.gBrowser.selectedTab = tab;
      emWindow.focus();
      resolve(emWindow);
      return;
    }

    // This must be a new load, else the ping/pong would have
    // found the window above.
    let whereToOpen =
      window.gBrowser && gBrowser.selectedTab.isEmpty ? "current" : "tab";
    openTrustedLinkIn("about:addons", whereToOpen);

    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      if (aView) {
        aSubject.loadView(aView);
      }
      aSubject.focus();
      resolve(aSubject);
    }, "EM-loaded");
  });
}

function AddKeywordForSearchField() {
  if (!gContextMenu) {
    throw new Error("Context menu doesn't seem to be open.");
  }

  gContextMenu.addKeywordForSearchField();
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
  if (gBrowser.tabs.length == 1 && gBrowser.selectedTab.isEmpty) {
    blankTabToRemove = gBrowser.selectedTab;
  }

  var tab = null;
  if (SessionStore.getClosedTabCount(window) > (aIndex || 0)) {
    tab = SessionStore.undoCloseTab(window, aIndex || 0);

    if (blankTabToRemove) {
      gBrowser.removeTab(blankTabToRemove);
    }
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
  if (SessionStore.getClosedWindowCount() > (aIndex || 0)) {
    window = SessionStore.undoCloseWindow(aIndex || 0);
  }

  return window;
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
function checkEmptyPageOrigin(
  browser = gBrowser.selectedBrowser,
  uri = browser.currentURI
) {
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
    if (
      (uriToCheck.spec == "about:blank" && contentPrincipal.isNullPrincipal) ||
      contentPrincipal.URI.spec == "about:blank"
    ) {
      return true;
    }
    return contentPrincipal.URI.equals(uri);
  }
  // ... so for those that don't have them, enforce that the page has the
  // system principal (this matches e.g. on about:newtab).

  return contentPrincipal.isSystemPrincipal;
}

function ReportFalseDeceptiveSite() {
  let docURI = gBrowser.selectedBrowser.documentURI;
  let isPhishingPage =
    docURI && docURI.spec.startsWith("about:blocked?e=deceptiveBlocked");

  if (isPhishingPage) {
    let mm = gBrowser.selectedBrowser.messageManager;
    let onMessage = message => {
      mm.removeMessageListener("DeceptiveBlockedDetails:Result", onMessage);
      let reportUrl = gSafeBrowsing.getReportURL(
        "PhishMistake",
        message.data.blockedInfo
      );
      if (reportUrl) {
        openTrustedLinkIn(reportUrl, "tab");
      } else {
        let bundle = Services.strings.createBundle(
          "chrome://browser/locale/safebrowsing/safebrowsing.properties"
        );
        Services.prompt.alert(
          window,
          bundle.GetStringFromName("errorReportFalseDeceptiveTitle"),
          bundle.formatStringFromName("errorReportFalseDeceptiveMessage", [
            message.data.blockedInfo.provider,
          ])
        );
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
  return aIsPref
    ? Services.urlFormatter.formatURLPref(aFormat)
    : Services.urlFormatter.formatURL(aFormat);
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

const gAccessibilityServiceIndicator = {
  init() {
    // Pref to enable accessibility service indicator.
    Services.prefs.addObserver("accessibility.indicator.enabled", this);
    // Accessibility service init/shutdown event.
    Services.obs.addObserver(this, "a11y-init-or-shutdown");
    this._update(Services.appinfo.accessibilityEnabled);
  },

  _update(accessibilityEnabled = false) {
    if (this.enabled && accessibilityEnabled) {
      this._active = true;
      document.documentElement.setAttribute("accessibilitymode", "true");
      [...document.querySelectorAll(".accessibility-indicator")].forEach(
        indicator =>
          ["click", "keypress"].forEach(type =>
            indicator.addEventListener(type, this)
          )
      );
    } else if (this._active) {
      this._active = false;
      document.documentElement.removeAttribute("accessibilitymode");
      [...document.querySelectorAll(".accessibility-indicator")].forEach(
        indicator =>
          ["click", "keypress"].forEach(type =>
            indicator.removeEventListener(type, this)
          )
      );
    }
  },

  observe(subject, topic, data) {
    if (
      topic == "nsPref:changed" &&
      data === "accessibility.indicator.enabled"
    ) {
      this._update(Services.appinfo.accessibilityEnabled);
    } else if (topic === "a11y-init-or-shutdown") {
      // When "a11y-init-or-shutdown" event is fired, "1" indicates that
      // accessibility service is started and "0" that it is shut down.
      this._update(data === "1");
    }
  },

  get enabled() {
    return Services.prefs.getBoolPref("accessibility.indicator.enabled");
  },

  handleEvent({ key, type }) {
    if (
      (type === "keypress" && [" ", "Enter"].includes(key)) ||
      type === "click"
    ) {
      let a11yServicesSupportURL = Services.urlFormatter.formatURLPref(
        "accessibility.support.url"
      );
      // This is a known URL coming from trusted UI
      openTrustedLinkIn(a11yServicesSupportURL, "tab");
      Services.telemetry.scalarSet("a11y.indicator_acted_on", true);
    }
  },

  uninit() {
    Services.prefs.removeObserver("accessibility.indicator.enabled", this);
    Services.obs.removeObserver(this, "a11y-init-or-shutdown");
  },
};

// Note that this is also called from non-browser windows on OSX, which do
// share menu items but not much else. See nonbrowser-mac.js.
var gPrivateBrowsingUI = {
  init: function PBUI_init() {
    // Do nothing for normal windows
    if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
      return;
    }

    // Disable the Clear Recent History... menu item when in PB mode
    // temporary fix until bug 463607 is fixed
    document.getElementById("Tools:Sanitize").setAttribute("disabled", "true");

    if (window.location.href != AppConstants.BROWSER_CHROME_URL) {
      return;
    }

    // Adjust the window's title
    let docElement = document.documentElement;
    if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
      docElement.setAttribute(
        "title",
        docElement.getAttribute("title_privatebrowsing")
      );
      docElement.setAttribute(
        "titlemodifier",
        docElement.getAttribute("titlemodifier_privatebrowsing")
      );
    }
    docElement.setAttribute(
      "privatebrowsingmode",
      PrivateBrowsingUtils.permanentPrivateBrowsing ? "permanent" : "temporary"
    );
    gBrowser.updateTitlebar();

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      // Adjust the New Window menu entries
      let newWindow = document.getElementById("menu_newNavigator");
      let newPrivateWindow = document.getElementById("menu_newPrivateWindow");
      if (newWindow && newPrivateWindow) {
        newPrivateWindow.hidden = true;
        newWindow.label = newPrivateWindow.label;
        newWindow.accessKey = newPrivateWindow.accessKey;
        newWindow.command = newPrivateWindow.command;
      }
    }
  },
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
  const kPrivateBrowsingWhitelist = new Set(["about:addons"]);

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
    if (
      !kPrivateBrowsingWhitelist.has(aURI.spec) &&
      (PrivateBrowsingUtils.isWindowPrivate(window) ||
        PrivateBrowsingUtils.isWindowPrivate(aWindow)) &&
      !PrivateBrowsingUtils.permanentPrivateBrowsing
    ) {
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
        ret = ret
          .split("?")[0]
          .concat(fragment != undefined ? "#".concat(fragment) : "");
      }
      return ret;
    }

    // Need to handle nsSimpleURIs here too (e.g. about:...), which don't
    // work correctly with URL objects - so treat them as strings
    let ignoreFragmentWhenComparing =
      typeof ignoreFragment == "string" &&
      ignoreFragment.startsWith("whenComparing");
    let requestedCompare = cleanURL(
      aURI.displaySpec,
      ignoreQueryString || replaceQueryString,
      ignoreFragmentWhenComparing
    );
    let browsers = aWindow.gBrowser.browsers;
    for (let i = 0; i < browsers.length; i++) {
      let browser = browsers[i];
      let browserCompare = cleanURL(
        browser.currentURI.displaySpec,
        ignoreQueryString || replaceQueryString,
        ignoreFragmentWhenComparing
      );
      if (requestedCompare == browserCompare) {
        // If adoptIntoActiveWindow is set, and this is a cross-window switch,
        // adopt the tab into the current window, after the active tab.
        let doAdopt =
          adoptIntoActiveWindow && isBrowserWindow && aWindow != window;

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
          browser.loadURI(aURI.spec, {
            triggeringPrincipal:
              aOpenParams.triggeringPrincipal ||
              _createNullPrincipalFromTabUserContextId(),
          });
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
  if (!(aURI instanceof Ci.nsIURI)) {
    aURI = Services.io.newURI(aURI);
  }

  // Prioritise this window.
  if (isBrowserWindow && switchIfURIInWindow(window)) {
    return true;
  }

  for (let browserWin of browserWindows()) {
    // Skip closed (but not yet destroyed) windows,
    // and the current window (which was checked earlier).
    if (browserWin.closed || browserWin == window) {
      continue;
    }
    if (switchIfURIInWindow(browserWin)) {
      return true;
    }
  }

  // No opened tab has that url.
  if (aOpenNew) {
    if (isBrowserWindow && gBrowser.selectedTab.isEmpty) {
      openTrustedLinkIn(aURI.spec, "current", aOpenParams);
    } else {
      openTrustedLinkIn(aURI.spec, "tab", aOpenParams);
    }
  }

  return false;
}

var RestoreLastSessionObserver = {
  init() {
    if (
      SessionStore.canRestoreLastSession &&
      !PrivateBrowsingUtils.isWindowPrivate(window)
    ) {
      Services.obs.addObserver(this, "sessionstore-last-session-cleared", true);
      goSetCommandEnabled("Browser:RestoreLastSession", true);
    } else if (SessionStore.willAutoRestore) {
      document
        .getElementById("Browser:RestoreLastSession")
        .setAttribute("hidden", true);
    }
  },

  observe() {
    // The last session can only be restored once so there's
    // no way we need to re-enable our menu item.
    Services.obs.removeObserver(this, "sessionstore-last-session-cleared");
    goSetCommandEnabled("Browser:RestoreLastSession", false);
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};

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

// Prompt user to restart the browser in safe mode
function safeModeRestart() {
  if (Services.appinfo.inSafeMode) {
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart"
    );

    if (cancelQuit.data) {
      return;
    }

    Services.startup.quit(
      Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit
    );
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
        if (
          topic == "browser-delayed-startup-finished" &&
          subject == otherWin
        ) {
          Services.obs.removeObserver(delayedStartupFinished, topic);
          let otherGBrowser = otherWin.gBrowser;
          let otherTab = otherGBrowser.selectedTab;
          SessionStore.duplicateTab(otherWin, aTab, delta);
          otherGBrowser.removeTab(otherTab, { animate: false });
        }
      };

      Services.obs.addObserver(
        delayedStartupFinished,
        "browser-delayed-startup-finished"
      );
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

  /**
   * Registers a listener.
   *
   * @param listener (object)
   *        A listener is expected to expose the following properties:
   *
   *        getMouseTargetRect (function)
   *          Returns the rect that the MousePosTracker needs to alert
   *          the listener about if the mouse happens to be within it.
   *
   *        onMouseEnter (function, optional)
   *          The function to be called if the mouse enters the rect
   *          returned by getMouseTargetRect. MousePosTracker always
   *          runs this inside of a requestAnimationFrame, since it
   *          assumes that the notification is used to update the DOM.
   *
   *        onMouseLeave (function, optional)
   *          The function to be called if the mouse exits the rect
   *          returned by getMouseTargetRect. MousePosTracker always
   *          runs this inside of a requestAnimationFrame, since it
   *          assumes that the notification is used to update the DOM.
   */
  addListener(listener) {
    if (this._listeners.has(listener)) {
      return;
    }

    listener._hover = false;
    this._listeners.add(listener);

    this._callListener(listener);
  },

  removeListener(listener) {
    this._listeners.delete(listener);
  },

  handleEvent(event) {
    let fullZoom = window.windowUtils.fullZoom;
    this._x = event.screenX / fullZoom - window.mozInnerScreenX;
    this._y = event.screenY / fullZoom - window.mozInnerScreenY;

    this._listeners.forEach(listener => {
      try {
        this._callListener(listener);
      } catch (e) {
        Cu.reportError(e);
      }
    });
  },

  _callListener(listener) {
    let rect = listener.getMouseTargetRect();
    let hover =
      this._x >= rect.left &&
      this._x <= rect.right &&
      this._y >= rect.top &&
      this._y <= rect.bottom;

    if (hover == listener._hover) {
      return;
    }

    listener._hover = hover;

    if (hover) {
      if (listener.onMouseEnter) {
        listener.onMouseEnter();
      }
    } else if (listener.onMouseLeave) {
      listener.onMouseLeave();
    }
  },
};

var ToolbarIconColor = {
  _windowState: {
    active: false,
    fullscreen: false,
    tabsintitlebar: false,
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
    if (!this._initialized) {
      return;
    }
    function parseRGB(aColorString) {
      let rgb = aColorString.match(/^rgba?\((\d+), (\d+), (\d+)/);
      rgb.shift();
      return rgb.map(x => parseInt(x));
    }

    switch (reason) {
      case "activate": // falls through
      case "deactivate":
        this._windowState.active = reason === "activate";
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

    let toolbarSelector = ".browser-toolbar:not([collapsed=true])";
    if (AppConstants.platform == "macosx") {
      toolbarSelector += ":not([type=menubar])";
    }

    // The getComputedStyle calls and setting the brighttext are separated in
    // two loops to avoid flushing layout and making it dirty repeatedly.
    let cachedLuminances = this._toolbarLuminanceCache;
    let luminances = new Map();
    for (let toolbar of document.querySelectorAll(toolbarSelector)) {
      // toolbars *should* all have ids, but guard anyway to avoid blowing up
      let cacheKey =
        toolbar.id && toolbar.id + JSON.stringify(this._windowState);
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
      if (luminance <= 110) {
        toolbar.removeAttribute("brighttext");
      } else {
        toolbar.setAttribute("brighttext", "true");
      }
    }
  },
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
      let anchor = widget.anchor.icon;
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
      "chrome://global/skin/icons/blocklist_favicon.png",
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
      let notification = notificationBox.getNotificationWithValue(
        "blocked-badware-page"
      );
      if (notification) {
        notificationBox.removeNotification(notification, false);
      }

      this._currentURIBaseDomain = null;
    }
  },
};

function TabModalPromptBox(browser) {
  this._weakBrowserRef = Cu.getWeakReference(browser);
  /*
   * This WeakMap holds the TabModalPrompt instances, key to the <tabmodalprompt> prompt
   * in the DOM. We don't want to hold the instances directly to avoid leaking.
   *
   * WeakMap also prevents us from reading back its insertion order.
   * Order of the elements in the DOM should be the only order to consider.
   */
  this.prompts = new WeakMap();
}

TabModalPromptBox.prototype = {
  _promptCloseCallback(
    onCloseCallback,
    principalToAllowFocusFor,
    allowFocusCheckbox,
    ...args
  ) {
    if (
      principalToAllowFocusFor &&
      allowFocusCheckbox &&
      allowFocusCheckbox.checked
    ) {
      Services.perms.addFromPrincipal(
        principalToAllowFocusFor,
        "focus-tab-by-prompt",
        Services.perms.ALLOW_ACTION
      );
    }
    onCloseCallback.apply(this, args);
  },

  appendPrompt(args, onCloseCallback) {
    let newPrompt = new TabModalPrompt(window);
    this.prompts.set(newPrompt.element, newPrompt);

    let browser = this.browser;
    browser.parentNode.insertBefore(
      newPrompt.element,
      browser.nextElementSibling
    );
    browser.setAttribute("tabmodalPromptShowing", true);

    let prompts = this.listPrompts();
    if (prompts.length > 1) {
      // Let's hide ourself behind the current prompt.
      newPrompt.element.hidden = true;
    }

    let principalToAllowFocusFor = this._allowTabFocusByPromptPrincipal;
    delete this._allowTabFocusByPromptPrincipal;

    let allowFocusCheckbox; // Define outside the if block so we can bind it into the callback.
    let hostForAllowFocusCheckbox = "";
    try {
      hostForAllowFocusCheckbox = principalToAllowFocusFor.URI.host;
    } catch (ex) {
      /* Ignore exceptions for host-less URIs */
    }
    if (hostForAllowFocusCheckbox) {
      let allowFocusRow = document.createXULElement("row");
      allowFocusCheckbox = document.createXULElement("checkbox");
      let spacer = document.createXULElement("spacer");
      allowFocusRow.appendChild(spacer);
      let label = gTabBrowserBundle.formatStringFromName(
        "tabs.allowTabFocusByPromptForSite",
        [hostForAllowFocusCheckbox]
      );
      allowFocusCheckbox.setAttribute("label", label);
      allowFocusRow.appendChild(allowFocusCheckbox);
      newPrompt.ui.rows.append(allowFocusRow);
    }

    let tab = gBrowser.getTabForBrowser(browser);
    let closeCB = this._promptCloseCallback.bind(
      null,
      onCloseCallback,
      principalToAllowFocusFor,
      allowFocusCheckbox
    );
    newPrompt.init(args, tab, closeCB);
    return newPrompt;
  },

  removePrompt(aPrompt) {
    this.prompts.delete(aPrompt.element);
    let browser = this.browser;
    aPrompt.element.remove();

    let prompts = this.listPrompts();
    if (prompts.length) {
      let prompt = prompts[prompts.length - 1];
      prompt.element.hidden = false;
      // Because we were hidden before, this won't have been possible, so do it now:
      prompt.ensureXBLBindingAttached();
      prompt.Dialog.setDefaultFocus();
    } else {
      browser.removeAttribute("tabmodalPromptShowing");
      browser.focus();
    }
  },

  listPrompts(aPrompt) {
    // Get the nodelist, then return the TabModalPrompt instances as an array
    const XUL_NS =
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let els = this.browser.parentNode.getElementsByTagNameNS(
      XUL_NS,
      "tabmodalprompt"
    );
    return Array.from(els).map(el => this.prompts.get(el));
  },

  onNextPromptShowAllowFocusCheckboxFor(principal) {
    this._allowTabFocusByPromptPrincipal = principal;
  },

  get browser() {
    let browser = this._weakBrowserRef.get();
    if (!browser) {
      throw new Error("Stale promptbox! The associated browser is gone.");
    }
    return browser;
  },
};

var ConfirmationHint = {
  /**
   * Shows a transient, non-interactive confirmation hint anchored to an
   * element, usually used in response to a user action to reaffirm that it was
   * successful and potentially provide extra context. Examples for such hints:
   * - "Saved to Library!" after bookmarking a page
   * - "Sent!" after sending a tab to another device
   * - "Queued (offline)" when attempting to send a tab to another device
   *   while offline
   *
   * @param  anchor (DOM node, required)
   *         The anchor for the panel.
   * @param  messageId (string, required)
   *         For getting the message string from browser.properties:
   *         confirmationHint.<messageId>.label
   * @param  options (object, optional)
   *         An object with the following optional properties:
   *         - event (DOM event): The event that triggered the feedback.
   *         - hideArrow (boolean): Optionally hide the arrow.
   *         - showDescription (boolean): show description text (confirmationHint.<messageId>.description)
   *
   */
  show(anchor, messageId, options = {}) {
    this._message.textContent = gBrowserBundle.GetStringFromName(
      `confirmationHint.${messageId}.label`
    );

    if (options.showDescription) {
      this._description.textContent = gBrowserBundle.GetStringFromName(
        `confirmationHint.${messageId}.description`
      );
      this._description.hidden = false;
      this._panel.classList.add("with-description");
    } else {
      this._description.hidden = true;
      this._panel.classList.remove("with-description");
    }

    if (options.hideArrow) {
      this._panel.setAttribute("hidearrow", "true");
    }

    // The timeout value used here allows the panel to stay open for
    // 1.5s second after the text transition (duration=120ms) has finished.
    // If there is a description, we show for 4s after the text transition.
    const DURATION = options.showDescription ? 4000 : 1500;
    this._panel.addEventListener(
      "popupshown",
      () => {
        this._animationBox.setAttribute("animate", "true");

        setTimeout(() => {
          this._panel.hidePopup(true);
        }, DURATION + 120);
      },
      { once: true }
    );

    this._panel.addEventListener(
      "popuphidden",
      () => {
        this._panel.removeAttribute("hidearrow");
        this._animationBox.removeAttribute("animate");
      },
      { once: true }
    );

    this._panel.hidden = false;
    this._panel.openPopup(anchor, {
      position: "bottomcenter topleft",
      triggerEvent: options.event,
    });
  },

  get _panel() {
    delete this._panel;
    return (this._panel = document.getElementById("confirmation-hint"));
  },

  get _animationBox() {
    delete this._animationBox;
    return (this._animationBox = document.getElementById(
      "confirmation-hint-checkmark-animation-container"
    ));
  },

  get _message() {
    delete this._message;
    return (this._message = document.getElementById(
      "confirmation-hint-message"
    ));
  },

  get _description() {
    delete this._description;
    return (this._description = document.getElementById(
      "confirmation-hint-description"
    ));
  },
};
