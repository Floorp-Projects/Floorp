/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [
  "AboutHomeStartupCache",
  "BrowserGlue",
  "ContentPermissionPrompt",
  "DefaultBrowserCheck",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.jsm",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  ASRouterDefaultConfig:
    "resource://activity-stream/lib/ASRouterDefaultConfig.jsm",
  ASRouterNewTabHook: "resource://activity-stream/lib/ASRouterNewTabHook.jsm",
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  BackgroundUpdate: "resource://gre/modules/BackgroundUpdate.jsm",
  Blocklist: "resource://gre/modules/Blocklist.jsm",
  BookmarkHTMLUtils: "resource://gre/modules/BookmarkHTMLUtils.jsm",
  BookmarkJSONUtils: "resource://gre/modules/BookmarkJSONUtils.jsm",
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.jsm",
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
  BrowserUIUtils: "resource:///modules/BrowserUIUtils.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  BuiltInThemes: "resource:///modules/BuiltInThemes.jsm",
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.jsm",
  Corroborate: "resource://gre/modules/Corroborate.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  Discovery: "resource:///modules/Discovery.jsm",
  DoHController: "resource:///modules/DoHController.jsm",
  DownloadsViewableInternally:
    "resource:///modules/DownloadsViewableInternally.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ExtensionsUI: "resource:///modules/ExtensionsUI.jsm",
  FeatureGate: "resource://featuregates/FeatureGate.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  HomePage: "resource:///modules/HomePage.jsm",
  Integration: "resource://gre/modules/Integration.jsm",
  Interactions: "resource:///modules/Interactions.jsm",
  Log: "resource://gre/modules/Log.jsm",
  LoginBreaches: "resource:///modules/LoginBreaches.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  Normandy: "resource://normandy/Normandy.jsm",
  OsEnvironment: "resource://gre/modules/OsEnvironment.jsm",
  PageActions: "resource:///modules/PageActions.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PdfJs: "resource://pdf.js/PdfJs.jsm",
  PermissionUI: "resource:///modules/PermissionUI.jsm",
  PlacesBackups: "resource://gre/modules/PlacesBackups.jsm",
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ProcessHangMonitor: "resource:///modules/ProcessHangMonitor.jsm",
  PublicSuffixList: "resource://gre/modules/netwerk-dns/PublicSuffixList.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  RemoteSecuritySettings:
    "resource://gre/modules/psm/RemoteSecuritySettings.jsm",
  RFPHelper: "resource://gre/modules/RFPHelper.jsm",
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  SaveToPocket: "chrome://pocket/content/SaveToPocket.jsm",
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.jsm",
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.jsm",
  SessionStartup: "resource:///modules/sessionstore/SessionStartup.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.jsm",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  TabUnloader: "resource:///modules/TabUnloader.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
  TRRRacer: "resource:///modules/TRRPerformance.jsm",
  UIState: "resource://services-sync/UIState.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  WebChannel: "resource://gre/modules/WebChannel.jsm",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.jsm",
});

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  AboutLoginsParent: "resource:///modules/AboutLoginsParent.jsm",
  PluginManager: "resource:///actors/PluginParent.jsm",
});

// Modules requiring an initialization method call.
let initializedModules = {};
[
  [
    "ContentPrefServiceParent",
    "resource://gre/modules/ContentPrefServiceParent.jsm",
    "alwaysInit",
  ],
  ["UpdateListener", "resource://gre/modules/UpdateListener.jsm", "init"],
].forEach(([name, resource, init]) => {
  XPCOMUtils.defineLazyGetter(this, name, () => {
    ChromeUtils.import(resource, initializedModules);
    initializedModules[name][init]();
    return initializedModules[name];
  });
});

XPCOMUtils.defineLazyServiceGetters(this, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
  PushService: ["@mozilla.org/push/Service;1", "nsIPushService"],
});

const PREF_PDFJS_ISDEFAULT_CACHE_STATE = "pdfjs.enabledCache.state";
const PREF_DFPI_ENABLED_BY_DEFAULT =
  "privacy.restrict3rdpartystorage.rollout.enabledByDefault";

/**
 * Fission-compatible JSProcess implementations.
 * Each actor options object takes the form of a ProcessActorOptions dictionary.
 * Detailed documentation of these options is in dom/docs/ipc/jsactors.rst,
 * available at https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html
 */
let JSPROCESSACTORS = {
  // Miscellaneous stuff that needs to be initialized per process.
  BrowserProcess: {
    child: {
      moduleURI: "resource:///actors/BrowserProcessChild.jsm",
      observers: [
        // WebRTC related notifications. They are here to avoid loading WebRTC
        // components when not needed.
        "getUserMedia:request",
        "recording-device-stopped",
        "PeerConnection:request",
        "recording-device-events",
        "recording-window-ended",
      ],
    },
  },

  RefreshBlockerObserver: {
    child: {
      moduleURI: "resource:///actors/RefreshBlockerChild.jsm",
      observers: [
        "webnavigation-create",
        "chrome-webnavigation-create",
        "webnavigation-destroy",
        "chrome-webnavigation-destroy",
      ],
    },

    enablePreference: "accessibility.blockautorefresh",
    onPreferenceChanged: (prefName, prevValue, isEnabled) => {
      BrowserWindowTracker.orderedWindows.forEach(win => {
        for (let browser of win.gBrowser.browsers) {
          try {
            browser.sendMessageToActor(
              "PreferenceChanged",
              { isEnabled },
              "RefreshBlocker",
              "all"
            );
          } catch (ex) {}
        }
      });
    },
  },
};

/**
 * Fission-compatible JSWindowActor implementations.
 * Detailed documentation of these options is in dom/docs/ipc/jsactors.rst,
 * available at https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html
 */
let JSWINDOWACTORS = {
  AboutLogins: {
    parent: {
      moduleURI: "resource:///actors/AboutLoginsParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutLoginsChild.jsm",
      events: {
        AboutLoginsCopyLoginDetail: { wantUntrusted: true },
        AboutLoginsCreateLogin: { wantUntrusted: true },
        AboutLoginsDeleteLogin: { wantUntrusted: true },
        AboutLoginsDismissBreachAlert: { wantUntrusted: true },
        AboutLoginsImportFromBrowser: { wantUntrusted: true },
        AboutLoginsImportFromFile: { wantUntrusted: true },
        AboutLoginsImportReportInit: { wantUntrusted: true },
        AboutLoginsImportReportReady: { wantUntrusted: true },
        AboutLoginsInit: { wantUntrusted: true },
        AboutLoginsGetHelp: { wantUntrusted: true },
        AboutLoginsOpenPreferences: { wantUntrusted: true },
        AboutLoginsOpenSite: { wantUntrusted: true },
        AboutLoginsRecordTelemetryEvent: { wantUntrusted: true },
        AboutLoginsRemoveAllLogins: { wantUntrusted: true },
        AboutLoginsSortChanged: { wantUntrusted: true },
        AboutLoginsSyncEnable: { wantUntrusted: true },
        AboutLoginsSyncOptions: { wantUntrusted: true },
        AboutLoginsUpdateLogin: { wantUntrusted: true },
        AboutLoginsExportPasswords: { wantUntrusted: true },
      },
    },
    matches: ["about:logins", "about:logins?*", "about:loginsimportreport"],
  },

  AboutNewTab: {
    parent: {
      moduleURI: "resource:///actors/AboutNewTabParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutNewTabChild.jsm",
      events: {
        DOMContentLoaded: {},
        pageshow: {},
        visibilitychange: {},
      },
    },
    // The wildcard on about:newtab is for the ?endpoint query parameter
    // that is used for snippets debugging. The wildcard for about:home
    // is similar, and also allows for falling back to loading the
    // about:home document dynamically if an attempt is made to load
    // about:home?jscache from the AboutHomeStartupCache as a top-level
    // load.
    matches: ["about:home*", "about:welcome", "about:newtab*"],
    remoteTypes: ["privilegedabout"],
  },

  AboutPlugins: {
    parent: {
      moduleURI: "resource:///actors/AboutPluginsParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutPluginsChild.jsm",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:plugins"],
  },

  AboutPocket: {
    parent: {
      moduleURI: "resource:///actors/AboutPocketParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutPocketChild.jsm",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: [
      "about:pocket-saved*",
      "about:pocket-signup*",
      "about:pocket-home*",
    ],
  },

  AboutPrivateBrowsing: {
    parent: {
      moduleURI: "resource:///actors/AboutPrivateBrowsingParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutPrivateBrowsingChild.jsm",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:privatebrowsing"],
  },

  AboutProtections: {
    parent: {
      moduleURI: "resource:///actors/AboutProtectionsParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutProtectionsChild.jsm",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:protections", "about:protections?*"],
  },

  AboutReader: {
    parent: {
      moduleURI: "resource:///actors/AboutReaderParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutReaderChild.jsm",
      events: {
        DOMContentLoaded: {},
        pageshow: { mozSystemGroup: true },
        // Don't try to create the actor if only the pagehide event fires.
        // This can happen with the initial about:blank documents.
        pagehide: { mozSystemGroup: true, createActor: false },
      },
    },
    messageManagerGroups: ["browsers"],
  },

  AboutTabCrashed: {
    parent: {
      moduleURI: "resource:///actors/AboutTabCrashedParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutTabCrashedChild.jsm",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:tabcrashed*"],
  },

  AboutWelcome: {
    parent: {
      moduleURI: "resource:///actors/AboutWelcomeParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/AboutWelcomeChild.jsm",
      events: {
        // This is added so the actor instantiates immediately and makes
        // methods available to the page js on load.
        DOMDocElementInserted: {},
      },
    },
    matches: ["about:welcome"],
    remoteTypes: ["privilegedabout"],

    // See Bug 1618306
    // Remove this preference check when we turn on separate about:welcome for all users.
    enablePreference: "browser.aboutwelcome.enabled",
  },

  BlockedSite: {
    parent: {
      moduleURI: "resource:///actors/BlockedSiteParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/BlockedSiteChild.jsm",
      events: {
        AboutBlockedLoaded: { wantUntrusted: true },
        click: {},
      },
    },
    matches: ["about:blocked?*"],
    allFrames: true,
  },

  BrowserTab: {
    parent: {
      moduleURI: "resource:///actors/BrowserTabParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/BrowserTabChild.jsm",

      events: {
        DOMDocElementInserted: {},
        MozAfterPaint: {},
      },
    },

    messageManagerGroups: ["browsers"],
  },

  ClickHandler: {
    parent: {
      moduleURI: "resource:///actors/ClickHandlerParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/ClickHandlerChild.jsm",
      events: {
        click: { capture: true, mozSystemGroup: true, wantUntrusted: true },
        auxclick: { capture: true, mozSystemGroup: true, wantUntrusted: true },
      },
    },

    allFrames: true,
  },

  ContentSearch: {
    parent: {
      moduleURI: "resource:///actors/ContentSearchParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/ContentSearchChild.jsm",
      events: {
        ContentSearchClient: { capture: true, wantUntrusted: true },
      },
    },
    matches: [
      "about:home",
      "about:welcome",
      "about:newtab",
      "about:privatebrowsing",
      "about:test-about-content-search-ui",
    ],
    remoteTypes: ["privilegedabout"],
  },

  ContextMenu: {
    parent: {
      moduleURI: "resource:///actors/ContextMenuParent.jsm",
    },

    child: {
      moduleURI: "resource:///actors/ContextMenuChild.jsm",
      events: {
        contextmenu: { mozSystemGroup: true },
      },
    },

    allFrames: true,
  },

  DecoderDoctor: {
    parent: {
      moduleURI: "resource:///actors/DecoderDoctorParent.jsm",
    },

    child: {
      moduleURI: "resource:///actors/DecoderDoctorChild.jsm",
      observers: ["decoder-doctor-notification"],
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  DOMFullscreen: {
    parent: {
      moduleURI: "resource:///actors/DOMFullscreenParent.jsm",
    },

    child: {
      moduleURI: "resource:///actors/DOMFullscreenChild.jsm",
      events: {
        "MozDOMFullscreen:Request": {},
        "MozDOMFullscreen:Entered": {},
        "MozDOMFullscreen:NewOrigin": {},
        "MozDOMFullscreen:Exit": {},
        "MozDOMFullscreen:Exited": {},
      },
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  EncryptedMedia: {
    parent: {
      moduleURI: "resource:///actors/EncryptedMediaParent.jsm",
    },

    child: {
      moduleURI: "resource:///actors/EncryptedMediaChild.jsm",
      observers: ["mediakeys-request"],
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  FormValidation: {
    parent: {
      moduleURI: "resource:///actors/FormValidationParent.jsm",
    },

    child: {
      moduleURI: "resource:///actors/FormValidationChild.jsm",
      events: {
        MozInvalidForm: {},
        // Listening to ‘pageshow’ event is only relevant if an invalid form
        // popup was open, so don't create the actor when fired.
        pageshow: { createActor: false },
      },
    },

    allFrames: true,
  },

  LightweightTheme: {
    child: {
      moduleURI: "resource:///actors/LightweightThemeChild.jsm",
      events: {
        pageshow: { mozSystemGroup: true },
        DOMContentLoaded: {},
      },
    },
    includeChrome: true,
    allFrames: true,
    matches: [
      "about:home",
      "about:newtab",
      "about:welcome",
      "chrome://browser/content/syncedtabs/sidebar.xhtml",
      "chrome://browser/content/places/historySidebar.xhtml",
      "chrome://browser/content/places/bookmarksSidebar.xhtml",
    ],
  },

  LinkHandler: {
    parent: {
      moduleURI: "resource:///actors/LinkHandlerParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/LinkHandlerChild.jsm",
      events: {
        DOMHeadElementParsed: {},
        DOMLinkAdded: {},
        DOMLinkChanged: {},
        pageshow: {},
        // The `pagehide` event is only used to clean up state which will not be
        // present if the actor hasn't been created.
        pagehide: { createActor: false },
      },
    },

    messageManagerGroups: ["browsers"],
  },

  NetError: {
    parent: {
      moduleURI: "resource:///actors/NetErrorParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/NetErrorChild.jsm",
      events: {
        DOMDocElementInserted: {},
        click: {},
      },
    },

    matches: ["about:certerror?*", "about:neterror?*"],
    allFrames: true,
  },

  PageInfo: {
    child: {
      moduleURI: "resource:///actors/PageInfoChild.jsm",
    },

    allFrames: true,
  },

  PageStyle: {
    parent: {
      moduleURI: "resource:///actors/PageStyleParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/PageStyleChild.jsm",
      events: {
        pageshow: {},
      },
    },

    // Only matching web pages, as opposed to internal about:, chrome: or
    // resource: pages. See https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns
    matches: ["*://*/*", "file:///*"],
    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  Pdfjs: {
    parent: {
      moduleURI: "resource://pdf.js/PdfjsParent.jsm",
    },
    child: {
      moduleURI: "resource://pdf.js/PdfjsChild.jsm",
    },
    allFrames: true,
  },

  // GMP crash reporting
  Plugin: {
    parent: {
      moduleURI: "resource:///actors/PluginParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/PluginChild.jsm",
      events: {
        PluginCrashed: { capture: true },
      },
    },

    allFrames: true,
  },

  PointerLock: {
    parent: {
      moduleURI: "resource:///actors/PointerLockParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/PointerLockChild.jsm",
      events: {
        "MozDOMPointerLock:Entered": {},
        "MozDOMPointerLock:Exited": {},
      },
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  Prompt: {
    parent: {
      moduleURI: "resource:///actors/PromptParent.jsm",
    },
    includeChrome: true,
    allFrames: true,
  },

  RefreshBlocker: {
    parent: {
      moduleURI: "resource:///actors/RefreshBlockerParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/RefreshBlockerChild.jsm",
    },

    messageManagerGroups: ["browsers"],
    enablePreference: "accessibility.blockautorefresh",
  },

  ScreenshotsComponent: {
    child: {
      moduleURI: "resource:///actors/ScreenshotsComponentChild.jsm",
    },
    enablePreference: "screenshots.browser.component.enabled",
  },

  SearchSERPTelemetry: {
    parent: {
      moduleURI: "resource:///actors/SearchSERPTelemetryParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/SearchSERPTelemetryChild.jsm",
      events: {
        DOMContentLoaded: {},
        pageshow: { mozSystemGroup: true },
        // The 'unload' event is only used to clean up state, and should not
        // force actor creation.
        unload: { createActor: false },
      },
    },
  },

  ShieldFrame: {
    parent: {
      moduleURI: "resource://normandy-content/ShieldFrameParent.jsm",
    },
    child: {
      moduleURI: "resource://normandy-content/ShieldFrameChild.jsm",
      events: {
        pageshow: {},
        pagehide: {},
        ShieldPageEvent: { wantUntrusted: true },
      },
    },
    matches: ["about:studies*"],
  },

  ASRouter: {
    parent: {
      moduleURI: "resource:///actors/ASRouterParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/ASRouterChild.jsm",
      events: {
        // This is added so the actor instantiates immediately and makes
        // methods available to the page js on load.
        DOMDocElementInserted: {},
      },
    },
    matches: ["about:home*", "about:newtab*", "about:welcome*"],
    remoteTypes: ["privilegedabout"],
  },

  SwitchDocumentDirection: {
    child: {
      moduleURI: "resource:///actors/SwitchDocumentDirectionChild.jsm",
    },

    allFrames: true,
  },

  Translation: {
    parent: {
      moduleURI: "resource:///modules/translation/TranslationParent.jsm",
    },
    child: {
      moduleURI: "resource:///modules/translation/TranslationChild.jsm",
      events: {
        pageshow: {},
        load: { mozSystemGroup: true, capture: true },
      },
    },
    enablePreference: "browser.translation.detectLanguage",
  },

  UITour: {
    parent: {
      moduleURI: "resource:///modules/UITourParent.jsm",
    },
    child: {
      moduleURI: "resource:///modules/UITourChild.jsm",
      events: {
        mozUITour: { wantUntrusted: true },
      },
    },

    messageManagerGroups: ["browsers"],
  },

  WebRTC: {
    parent: {
      moduleURI: "resource:///actors/WebRTCParent.jsm",
    },
    child: {
      moduleURI: "resource:///actors/WebRTCChild.jsm",
    },

    allFrames: true,
  },
};

(function earlyBlankFirstPaint() {
  let startTime = Cu.now();
  if (
    AppConstants.platform == "macosx" ||
    Services.startup.wasSilentlyStarted ||
    !Services.prefs.getBoolPref("browser.startup.blankWindow", false)
  ) {
    return;
  }

  // Until bug 1450626 and bug 1488384 are fixed, skip the blank window when
  // using a non-default theme.
  if (
    !Services.startup.showedPreXULSkeletonUI &&
    Services.prefs.getCharPref(
      "extensions.activeThemeID",
      "default-theme@mozilla.org"
    ) != "default-theme@mozilla.org"
  ) {
    return;
  }

  let store = Services.xulStore;
  let getValue = attr =>
    store.getValue(AppConstants.BROWSER_CHROME_URL, "main-window", attr);
  let width = getValue("width");
  let height = getValue("height");

  // The clean profile case isn't handled yet. Return early for now.
  if (!width || !height) {
    return;
  }

  let browserWindowFeatures =
    "chrome,all,dialog=no,extrachrome,menubar,resizable,scrollbars,status," +
    "location,toolbar,personalbar";
  let win = Services.ww.openWindow(
    null,
    "about:blank",
    null,
    browserWindowFeatures,
    null
  );

  // Hide the titlebar if the actual browser window will draw in it.
  let hiddenTitlebar = Services.appinfo.drawInTitlebar;
  if (hiddenTitlebar) {
    win.windowUtils.setChromeMargin(0, 2, 2, 2);
  }

  let docElt = win.document.documentElement;
  docElt.setAttribute("screenX", getValue("screenX"));
  docElt.setAttribute("screenY", getValue("screenY"));

  // The sizemode="maximized" attribute needs to be set before first paint.
  let sizemode = getValue("sizemode");
  if (sizemode == "maximized") {
    docElt.setAttribute("sizemode", sizemode);

    // Set the size to use when the user leaves the maximized mode.
    // The persisted size is the outer size, but the height/width
    // attributes set the inner size.
    let appWin = win.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIAppWindow);
    height -= appWin.outerToInnerHeightDifferenceInCSSPixels;
    width -= appWin.outerToInnerWidthDifferenceInCSSPixels;
    docElt.setAttribute("height", height);
    docElt.setAttribute("width", width);
  } else {
    // Setting the size of the window in the features string instead of here
    // causes the window to grow by the size of the titlebar.
    win.resizeTo(width, height);
  }

  // Set this before showing the window so that graphics code can use it to
  // decide to skip some expensive code paths (eg. starting the GPU process).
  docElt.setAttribute("windowtype", "navigator:blank");

  // The window becomes visible after OnStopRequest, so make this happen now.
  win.stop();

  ChromeUtils.addProfilerMarker("earlyBlankFirstPaint", startTime);
  win.openTime = Cu.now();

  let { TelemetryTimestamps } = ChromeUtils.import(
    "resource://gre/modules/TelemetryTimestamps.jsm"
  );
  TelemetryTimestamps.add("blankWindowShown");
})();

XPCOMUtils.defineLazyGetter(
  this,
  "WeaveService",
  () => Cc["@mozilla.org/weave/service;1"].getService().wrappedJSObject
);

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyModuleGetters(this, {
    UnsubmittedCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  });
}

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gTabbrowserBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/tabbrowser.properties"
  );
});

const global = this;

const listeners = {
  observers: {
    "update-downloading": ["UpdateListener"],
    "update-staged": ["UpdateListener"],
    "update-downloaded": ["UpdateListener"],
    "update-available": ["UpdateListener"],
    "update-error": ["UpdateListener"],
    "update-swap": ["UpdateListener"],
    "gmp-plugin-crash": ["PluginManager"],
    "plugin-crashed": ["PluginManager"],
  },

  observe(subject, topic, data) {
    for (let module of this.observers[topic]) {
      try {
        global[module].observe(subject, topic, data);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  init() {
    for (let observer of Object.keys(this.observers)) {
      Services.obs.addObserver(this, observer);
    }
  },
};

// Seconds of idle before trying to create a bookmarks backup.
const BOOKMARKS_BACKUP_IDLE_TIME_SEC = 8 * 60;
// Minimum interval between backups.  We try to not create more than one backup
// per interval.
const BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS = 1;
// Seconds of idle time before the late idle tasks will be scheduled.
const LATE_TASKS_IDLE_TIME_SEC = 20;
// Time after we stop tracking startup crashes.
const STARTUP_CRASHES_END_DELAY_MS = 30 * 1000;

/*
 * OS X has the concept of zero-window sessions and therefore ignores the
 * browser-lastwindow-close-* topics.
 */
const OBSERVE_LASTWINDOW_CLOSE_TOPICS = AppConstants.platform != "macosx";

function BrowserGlue() {
  XPCOMUtils.defineLazyServiceGetter(
    this,
    "_userIdleService",
    "@mozilla.org/widget/useridleservice;1",
    "nsIUserIdleService"
  );

  XPCOMUtils.defineLazyGetter(this, "_distributionCustomizer", function() {
    const { DistributionCustomizer } = ChromeUtils.import(
      "resource:///modules/distribution.js"
    );
    return new DistributionCustomizer();
  });

  XPCOMUtils.defineLazyServiceGetter(
    this,
    "AlertsService",
    "@mozilla.org/alerts-service;1",
    "nsIAlertsService"
  );

  this._init();
}

BrowserGlue.prototype = {
  _saveSession: false,
  _migrationImportsDefaultBookmarks: false,
  _placesBrowserInitComplete: false,
  _isNewProfile: undefined,

  _setPrefToSaveSession: function BG__setPrefToSaveSession(aForce) {
    if (!this._saveSession && !aForce) {
      return;
    }

    if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
      Services.prefs.setBoolPref(
        "browser.sessionstore.resume_session_once",
        true
      );
    }

    // This method can be called via [NSApplication terminate:] on Mac, which
    // ends up causing prefs not to be flushed to disk, so we need to do that
    // explicitly here. See bug 497652.
    Services.prefs.savePrefFile(null);
  },

  _setSyncAutoconnectDelay: function BG__setSyncAutoconnectDelay() {
    // Assume that a non-zero value for services.sync.autoconnectDelay should override
    if (Services.prefs.prefHasUserValue("services.sync.autoconnectDelay")) {
      let prefDelay = Services.prefs.getIntPref(
        "services.sync.autoconnectDelay"
      );

      if (prefDelay > 0) {
        return;
      }
    }

    // delays are in seconds
    const MAX_DELAY = 300;
    let delay = 3;
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      // browser windows without a gBrowser almost certainly means we are
      // shutting down, so instead of just ignoring that window we abort.
      if (win.closed || !win.gBrowser) {
        return;
      }
      delay += win.gBrowser.tabs.length;
    }
    delay = delay <= MAX_DELAY ? delay : MAX_DELAY;

    const { Weave } = ChromeUtils.import("resource://services-sync/main.js");
    Weave.Service.scheduler.delayedAutoConnect(delay);
  },

  // nsIObserver implementation
  observe: async function BG_observe(subject, topic, data) {
    switch (topic) {
      case "notifications-open-settings":
        this._openPreferences("privacy-permissions");
        break;
      case "final-ui-startup":
        this._beforeUIStartup();
        break;
      case "browser-delayed-startup-finished":
        this._onFirstWindowLoaded(subject);
        Services.obs.removeObserver(this, "browser-delayed-startup-finished");
        break;
      case "sessionstore-windows-restored":
        this._onWindowsRestored();
        break;
      case "browser:purge-session-history":
        // reset the console service's error buffer
        Services.console.logStringMessage(null); // clear the console (in case it's open)
        Services.console.reset();
        break;
      case "restart-in-safe-mode":
        this._onSafeModeRestart(subject);
        break;
      case "quit-application-requested":
        this._onQuitRequest(subject, data);
        break;
      case "quit-application-granted":
        this._onQuitApplicationGranted();
        break;
      case "browser-lastwindow-close-requested":
        if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
          // The application is not actually quitting, but the last full browser
          // window is about to be closed.
          this._onQuitRequest(subject, "lastwindow");
        }
        break;
      case "browser-lastwindow-close-granted":
        if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
          this._setPrefToSaveSession();
        }
        break;
      case "weave:service:ready":
        this._setSyncAutoconnectDelay();
        break;
      case "fxaccounts:onverified":
        this._onThisDeviceConnected();
        break;
      case "fxaccounts:device_connected":
        this._onDeviceConnected(data);
        break;
      case "fxaccounts:verify_login":
        this._onVerifyLoginNotification(JSON.parse(data));
        break;
      case "fxaccounts:device_disconnected":
        data = JSON.parse(data);
        if (data.isLocalDevice) {
          this._onDeviceDisconnected();
        }
        break;
      case "fxaccounts:commands:open-uri":
        this._onDisplaySyncURIs(subject);
        break;
      case "session-save":
        this._setPrefToSaveSession(true);
        subject.QueryInterface(Ci.nsISupportsPRBool);
        subject.data = true;
        break;
      case "places-init-complete":
        Services.obs.removeObserver(this, "places-init-complete");
        if (!this._migrationImportsDefaultBookmarks) {
          this._initPlaces(false);
        }
        break;
      case "idle":
        this._backupBookmarks();
        break;
      case "distribution-customization-complete":
        Services.obs.removeObserver(
          this,
          "distribution-customization-complete"
        );
        // Customization has finished, we don't need the customizer anymore.
        delete this._distributionCustomizer;
        break;
      case "browser-glue-test": // used by tests
        if (data == "force-ui-migration") {
          this._migrateUI();
        } else if (data == "force-distribution-customization") {
          this._distributionCustomizer.applyCustomizations();
          // To apply distribution bookmarks use "places-init-complete".
        } else if (data == "test-force-places-init") {
          this._placesInitialized = false;
          this._initPlaces(false);
        } else if (data == "mock-alerts-service") {
          Object.defineProperty(this, "AlertsService", {
            value: subject.wrappedJSObject,
          });
        } else if (data == "places-browser-init-complete") {
          if (this._placesBrowserInitComplete) {
            Services.obs.notifyObservers(null, "places-browser-init-complete");
          }
        } else if (data == "add-breaches-sync-handler") {
          this._addBreachesSyncHandler();
        }
        break;
      case "initial-migration-will-import-default-bookmarks":
        this._migrationImportsDefaultBookmarks = true;
        break;
      case "initial-migration-did-import-default-bookmarks":
        this._initPlaces(true);
        break;
      case "handle-xul-text-link":
        let linkHandled = subject.QueryInterface(Ci.nsISupportsPRBool);
        if (!linkHandled.data) {
          let win = BrowserWindowTracker.getTopWindow();
          if (win) {
            data = JSON.parse(data);
            let where = win.whereToOpenLink(data);
            // Preserve legacy behavior of non-modifier left-clicks
            // opening in a new selected tab.
            if (where == "current") {
              where = "tab";
            }
            win.openTrustedLinkIn(data.href, where);
            linkHandled.data = true;
          }
        }
        break;
      case "profile-before-change":
        // Any component depending on Places should be finalized in
        // _onPlacesShutdown.  Any component that doesn't need to act after
        // the UI has gone should be finalized in _onQuitApplicationGranted.
        this._dispose();
        break;
      case "keyword-search":
        // This notification is broadcast by the docshell when it "fixes up" a
        // URI that it's been asked to load into a keyword search.
        let engine = null;
        try {
          engine = Services.search.getEngineByName(
            subject.QueryInterface(Ci.nsISupportsString).data
          );
        } catch (ex) {
          Cu.reportError(ex);
        }
        let win = BrowserWindowTracker.getTopWindow();
        BrowserSearchTelemetry.recordSearch(
          win.gBrowser.selectedBrowser,
          engine,
          "urlbar"
        );
        break;
      case "browser-search-engine-modified":
        // Ensure we cleanup the hiddenOneOffs pref when removing
        // an engine, and that newly added engines are visible.
        if (data == "engine-added" || data == "engine-removed") {
          let engineName = subject.QueryInterface(Ci.nsISearchEngine).name;
          let pref = Services.prefs.getStringPref(
            "browser.search.hiddenOneOffs"
          );
          let hiddenList = pref ? pref.split(",") : [];
          hiddenList = hiddenList.filter(x => x !== engineName);
          Services.prefs.setStringPref(
            "browser.search.hiddenOneOffs",
            hiddenList.join(",")
          );
        }
        break;
      case "flash-plugin-hang":
        this._handleFlashHang();
        break;
      case "xpi-signature-changed":
        let disabledAddons = JSON.parse(data).disabled;
        let addons = await AddonManager.getAddonsByIDs(disabledAddons);
        if (addons.some(addon => addon)) {
          this._notifyUnsignedAddonsDisabled();
        }
        break;
      case "sync-ui-state:update":
        this._updateFxaBadges(BrowserWindowTracker.getTopWindow());
        break;
      case "handlersvc-store-initialized":
        // Initialize PdfJs when running in-process and remote. This only
        // happens once since PdfJs registers global hooks. If the PdfJs
        // extension is installed the init method below will be overridden
        // leaving initialization to the extension.
        // parent only: configure default prefs, set up pref observers, register
        // pdf content handler, and initializes parent side message manager
        // shim for privileged api access.
        PdfJs.init(this._isNewProfile);

        // Allow certain viewable internally types to be opened from downloads.
        DownloadsViewableInternally.register();

        break;
    }
  },

  // initialization (called on application startup)
  _init: function BG__init() {
    let os = Services.obs;
    [
      "notifications-open-settings",
      "final-ui-startup",
      "browser-delayed-startup-finished",
      "sessionstore-windows-restored",
      "browser:purge-session-history",
      "quit-application-requested",
      "quit-application-granted",
      "weave:service:ready",
      "fxaccounts:onverified",
      "fxaccounts:device_connected",
      "fxaccounts:verify_login",
      "fxaccounts:device_disconnected",
      "fxaccounts:commands:open-uri",
      "session-save",
      "places-init-complete",
      "distribution-customization-complete",
      "handle-xul-text-link",
      "profile-before-change",
      "keyword-search",
      "browser-search-engine-modified",
      "restart-in-safe-mode",
      "flash-plugin-hang",
      "xpi-signature-changed",
      "sync-ui-state:update",
      "handlersvc-store-initialized",
    ].forEach(topic => os.addObserver(this, topic, true));
    if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
      os.addObserver(this, "browser-lastwindow-close-requested", true);
      os.addObserver(this, "browser-lastwindow-close-granted", true);
    }

    ActorManagerParent.addJSProcessActors(JSPROCESSACTORS);
    ActorManagerParent.addJSWindowActors(JSWINDOWACTORS);

    this._flashHangCount = 0;
    this._firstWindowReady = new Promise(
      resolve => (this._firstWindowLoaded = resolve)
    );
    if (AppConstants.platform == "win") {
      JawsScreenReaderVersionCheck.init();
    }
  },

  // cleanup (called on application shutdown)
  _dispose: function BG__dispose() {
    // AboutHomeStartupCache might write to the cache during
    // quit-application-granted, so we defer uninitialization
    // until here.
    AboutHomeStartupCache.uninit();

    if (this._bookmarksBackupIdleTime) {
      this._userIdleService.removeIdleObserver(
        this,
        this._bookmarksBackupIdleTime
      );
      this._bookmarksBackupIdleTime = null;
    }
    if (this._lateTasksIdleObserver) {
      this._userIdleService.removeIdleObserver(
        this._lateTasksIdleObserver,
        LATE_TASKS_IDLE_TIME_SEC
      );
      delete this._lateTasksIdleObserver;
    }
    if (this._gmpInstallManager) {
      this._gmpInstallManager.uninit();
      delete this._gmpInstallManager;
    }

    Services.prefs.removeObserver(
      "privacy.trackingprotection",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "network.cookie.cookieBehavior",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "network.cookie.cookieBehavior.pbmode",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "network.http.referer.disallowCrossSiteRelaxingDefault",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "privacy.partition.network_state.ocsp_cache",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      ContentBlockingCategoriesPrefs.PREF_CB_CATEGORY,
      this._updateCBCategory
    );
    Services.prefs.removeObserver(
      "privacy.trackingprotection",
      this._setPrefExpectations
    );
    Services.prefs.removeObserver(
      "browser.contentblocking.features.strict",
      this._setPrefExpectationsAndUpdate
    );
    Services.prefs.removeObserver(
      PREF_DFPI_ENABLED_BY_DEFAULT,
      this._setDefaultCookieBehavior
    );
  },

  // runs on startup, before the first command line handler is invoked
  // (i.e. before the first window is opened)
  _beforeUIStartup: function BG__beforeUIStartup() {
    SessionStartup.init();

    // check if we're in safe mode
    if (Services.appinfo.inSafeMode) {
      Services.ww.openWindow(
        null,
        "chrome://browser/content/safeMode.xhtml",
        "_blank",
        "chrome,centerscreen,modal,resizable=no",
        null
      );
    }

    // apply distribution customizations
    this._distributionCustomizer.applyCustomizations();

    // handle any UI migration
    this._migrateUI();

    if (!Services.prefs.prefHasUserValue(PREF_PDFJS_ISDEFAULT_CACHE_STATE)) {
      PdfJs.checkIsDefault(this._isNewProfile);
    }

    listeners.init();

    SessionStore.init();

    BuiltInThemes.maybeInstallActiveBuiltInTheme();

    if (AppConstants.MOZ_NORMANDY) {
      Normandy.init();
    }

    SaveToPocket.init();

    AboutHomeStartupCache.init();

    Services.obs.notifyObservers(null, "browser-ui-startup-complete");
  },

  _checkForOldBuildUpdates() {
    // check for update if our build is old
    if (
      AppConstants.MOZ_UPDATER &&
      Services.prefs.getBoolPref("app.update.checkInstallTime")
    ) {
      let buildID = Services.appinfo.appBuildID;
      let today = new Date().getTime();
      /* eslint-disable no-multi-spaces */
      let buildDate = new Date(
        buildID.slice(0, 4), // year
        buildID.slice(4, 6) - 1, // months are zero-based.
        buildID.slice(6, 8), // day
        buildID.slice(8, 10), // hour
        buildID.slice(10, 12), // min
        buildID.slice(12, 14)
      ) // ms
        .getTime();
      /* eslint-enable no-multi-spaces */

      const millisecondsIn24Hours = 86400000;
      let acceptableAge =
        Services.prefs.getIntPref("app.update.checkInstallTime.days") *
        millisecondsIn24Hours;

      if (buildDate + acceptableAge < today) {
        Cc["@mozilla.org/updates/update-service;1"]
          .getService(Ci.nsIApplicationUpdateService)
          .checkForBackgroundUpdates();
      }
    }
  },

  async _onSafeModeRestart(window) {
    // prompt the user to confirm
    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let strings = gBrowserBundle;
    let promptTitle = strings.formatStringFromName(
      "troubleshootModeRestartPromptTitle",
      [productName]
    );
    let promptMessage = strings.GetStringFromName(
      "troubleshootModeRestartPromptMessage"
    );
    let restartText = strings.GetStringFromName(
      "troubleshootModeRestartButton"
    );
    let buttonFlags =
      Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
      Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL +
      Services.prompt.BUTTON_POS_0_DEFAULT;

    let rv = await Services.prompt.asyncConfirmEx(
      window.browsingContext,
      Ci.nsIPrompt.MODAL_TYPE_INTERNAL_WINDOW,
      promptTitle,
      promptMessage,
      buttonFlags,
      restartText,
      null,
      null,
      null,
      {}
    );
    if (rv.get("buttonNumClicked") != 0) {
      return;
    }

    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart"
    );

    if (!cancelQuit.data) {
      Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
    }
  },

  /**
   * Show a notification bar offering a reset.
   *
   * @param reason
   *        String of either "unused" or "uninstall", specifying the reason
   *        why a profile reset is offered.
   */
  _resetProfileNotification(reason) {
    let win = BrowserWindowTracker.getTopWindow();
    if (!win) {
      return;
    }

    const { ResetProfile } = ChromeUtils.import(
      "resource://gre/modules/ResetProfile.jsm"
    );
    if (!ResetProfile.resetSupported()) {
      return;
    }

    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let resetBundle = Services.strings.createBundle(
      "chrome://global/locale/resetProfile.properties"
    );

    let message;
    if (reason == "unused") {
      message = resetBundle.formatStringFromName("resetUnusedProfile.message", [
        productName,
      ]);
    } else if (reason == "uninstall") {
      message = resetBundle.formatStringFromName("resetUninstalled.message", [
        productName,
      ]);
    } else {
      throw new Error(
        `Unknown reason (${reason}) given to _resetProfileNotification.`
      );
    }
    let buttons = [
      {
        label: resetBundle.formatStringFromName(
          "refreshProfile.resetButton.label",
          [productName]
        ),
        accessKey: resetBundle.GetStringFromName(
          "refreshProfile.resetButton.accesskey"
        ),
        callback() {
          ResetProfile.openConfirmationDialog(win);
        },
      },
    ];

    win.gNotificationBox.appendNotification(
      "reset-profile-notification",
      {
        label: message,
        image: "chrome://global/skin/icons/question-64.png",
        priority: win.gNotificationBox.PRIORITY_INFO_LOW,
      },
      buttons
    );
  },

  _notifyUnsignedAddonsDisabled() {
    let win = BrowserWindowTracker.getTopWindow();
    if (!win) {
      return;
    }

    let message = win.gNavigatorBundle.getString(
      "unsignedAddonsDisabled.message"
    );
    let buttons = [
      {
        label: win.gNavigatorBundle.getString(
          "unsignedAddonsDisabled.learnMore.label"
        ),
        accessKey: win.gNavigatorBundle.getString(
          "unsignedAddonsDisabled.learnMore.accesskey"
        ),
        callback() {
          win.BrowserOpenAddonsMgr("addons://list/extension?unsigned=true");
        },
      },
    ];

    win.gNotificationBox.appendNotification(
      "unsigned-addons-disabled",
      {
        label: message,
        priority: win.gNotificationBox.PRIORITY_WARNING_MEDIUM,
      },
      buttons
    );
  },

  _firstWindowTelemetry(aWindow) {
    let scaling = aWindow.devicePixelRatio * 100;
    try {
      Services.telemetry.getHistogramById("DISPLAY_SCALING").add(scaling);
    } catch (ex) {}
  },

  _collectStartupConditionsTelemetry() {
    let nowSeconds = Math.round(Date.now() / 1000);
    // Don't include cases where we don't have the pref. This rules out the first install
    // as well as the first run of a build since this was introduced. These could by some
    // definitions be referred to as "cold" startups, but probably not since we likely
    // just wrote many of the files we use to disk. This way we should approximate a lower
    // bound to the number of cold startups rather than an upper bound.
    let lastCheckSeconds = Services.prefs.getIntPref(
      "browser.startup.lastColdStartupCheck",
      nowSeconds
    );
    Services.prefs.setIntPref(
      "browser.startup.lastColdStartupCheck",
      nowSeconds
    );
    try {
      let secondsSinceLastOSRestart =
        Services.startup.secondsSinceLastOSRestart;
      let isColdStartup =
        nowSeconds - secondsSinceLastOSRestart > lastCheckSeconds;
      Services.telemetry.scalarSet("startup.is_cold", isColdStartup);
      Services.telemetry.scalarSet(
        "startup.seconds_since_last_os_restart",
        secondsSinceLastOSRestart
      );
    } catch (ex) {
      Cu.reportError(ex);
    }
  },

  // the first browser window has finished initializing
  _onFirstWindowLoaded: function BG__onFirstWindowLoaded(aWindow) {
    AboutNewTab.init();

    TabCrashHandler.init();

    ProcessHangMonitor.init();

    UrlbarPrefs.updateFirefoxSuggestScenario();

    // A channel for "remote troubleshooting" code...
    let channel = new WebChannel(
      "remote-troubleshooting",
      "remote-troubleshooting"
    );
    channel.listen((id, data, target) => {
      if (data.command == "request") {
        let { Troubleshoot } = ChromeUtils.import(
          "resource://gre/modules/Troubleshoot.jsm"
        );
        Troubleshoot.snapshot(snapshotData => {
          // for privacy we remove crash IDs and all preferences (but bug 1091944
          // exists to expose prefs once we are confident of privacy implications)
          delete snapshotData.crashes;
          delete snapshotData.modifiedPreferences;
          delete snapshotData.printingPreferences;
          channel.send(snapshotData, target);
        });
      }
    });

    // Offer to reset a user's profile if it hasn't been used for 60 days.
    const OFFER_PROFILE_RESET_INTERVAL_MS = 60 * 24 * 60 * 60 * 1000;
    let lastUse = Services.appinfo.replacedLockTime;
    let disableResetPrompt = Services.prefs.getBoolPref(
      "browser.disableResetPrompt",
      false
    );

    if (
      !disableResetPrompt &&
      lastUse &&
      Date.now() - lastUse >= OFFER_PROFILE_RESET_INTERVAL_MS
    ) {
      this._resetProfileNotification("unused");
    } else if (AppConstants.platform == "win" && !disableResetPrompt) {
      // Check if we were just re-installed and offer Firefox Reset
      let updateChannel;
      try {
        updateChannel = ChromeUtils.import(
          "resource://gre/modules/UpdateUtils.jsm",
          {}
        ).UpdateUtils.UpdateChannel;
      } catch (ex) {}
      if (updateChannel) {
        let uninstalledValue = WindowsRegistry.readRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          "Software\\Mozilla\\Firefox",
          `Uninstalled-${updateChannel}`
        );
        let removalSuccessful = WindowsRegistry.removeRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          "Software\\Mozilla\\Firefox",
          `Uninstalled-${updateChannel}`
        );
        if (removalSuccessful && uninstalledValue == "True") {
          this._resetProfileNotification("uninstall");
        }
      }
    }

    this._checkForOldBuildUpdates();

    // Check if Sync is configured
    if (Services.prefs.prefHasUserValue("services.sync.username")) {
      WeaveService.init();
    }

    PageThumbs.init();

    NewTabUtils.init();

    Services.telemetry.setEventRecordingEnabled(
      "security.ui.protections",
      true
    );

    PageActions.init();

    DoHController.init();

    this._firstWindowTelemetry(aWindow);
    this._firstWindowLoaded();

    this._collectStartupConditionsTelemetry();

    // Set the default favicon size for UI views that use the page-icon protocol.
    PlacesUtils.favicons.setDefaultIconURIPreferredSize(
      16 * aWindow.devicePixelRatio
    );
    // _setDefaultCookieBehavior needs to run before other functions that modify
    // privacy preferences such as _setPrefExpectationsAndUpdate and _matchCBCategory
    this._setDefaultCookieBehavior();
    this._setPrefExpectationsAndUpdate();
    this._matchCBCategory();

    // This observes the entire privacy.trackingprotection.* pref tree.
    Services.prefs.addObserver(
      "privacy.trackingprotection",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "network.cookie.cookieBehavior",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "network.cookie.cookieBehavior.pbmode",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "network.http.referer.disallowCrossSiteRelaxingDefault",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "privacy.partition.network_state.ocsp_cache",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      ContentBlockingCategoriesPrefs.PREF_CB_CATEGORY,
      this._updateCBCategory
    );
    Services.prefs.addObserver(
      "media.autoplay.default",
      this._updateAutoplayPref
    );
    Services.prefs.addObserver(
      "privacy.trackingprotection",
      this._setPrefExpectations
    );
    Services.prefs.addObserver(
      "browser.contentblocking.features.strict",
      this._setPrefExpectationsAndUpdate
    );
    Services.prefs.addObserver(
      PREF_DFPI_ENABLED_BY_DEFAULT,
      this._setDefaultCookieBehavior
    );
  },

  _updateAutoplayPref() {
    const blocked = Services.prefs.getIntPref("media.autoplay.default", 1);
    const telemetry = Services.telemetry.getHistogramById(
      "AUTOPLAY_DEFAULT_SETTING_CHANGE"
    );
    const labels = { 0: "allow", 1: "blockAudible", 5: "blockAll" };
    if (blocked in labels) {
      telemetry.add(labels[blocked]);
    }
  },

  // For the initial rollout of dFPI, set the default cookieBehavior based on the pref
  // set during onboarding when the user chooses to enable protections or not.
  _setDefaultCookieBehavior() {
    if (!Services.prefs.prefHasUserValue(PREF_DFPI_ENABLED_BY_DEFAULT)) {
      Services.telemetry.scalarSet("privacy.dfpi_rollout_enabledByDefault", 2);
      return;
    }
    let dFPIEnabled = Services.prefs.getBoolPref(PREF_DFPI_ENABLED_BY_DEFAULT);

    let defaultPrefs = Services.prefs.getDefaultBranch("");
    defaultPrefs.setIntPref(
      "network.cookie.cookieBehavior",
      dFPIEnabled
        ? Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
        : Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
    );

    Services.telemetry.scalarSet(
      "privacy.dfpi_rollout_enabledByDefault",
      dFPIEnabled ? 1 : 0
    );

    if (dFPIEnabled) {
      Services.prefs.setStringPref(
        "browser.search.param.google_channel_us",
        "tus7"
      );
      Services.prefs.setStringPref(
        "browser.search.param.google_channel_row",
        "trow7"
      );
      Services.prefs.setStringPref(
        "browser.search.param.bing_ptag",
        "MOZZ0000000031"
      );
    } else {
      Services.prefs.setStringPref(
        "browser.search.param.google_channel_us",
        "xus7"
      );
      Services.prefs.setStringPref(
        "browser.search.param.google_channel_row",
        "xrow7"
      );
      Services.prefs.setStringPref(
        "browser.search.param.bing_ptag",
        "MOZZ0000000032"
      );
    }
  },

  _setPrefExpectations() {
    ContentBlockingCategoriesPrefs.setPrefExpectations();
  },

  _setPrefExpectationsAndUpdate() {
    ContentBlockingCategoriesPrefs.setPrefExpectations();
    ContentBlockingCategoriesPrefs.updateCBCategory();
  },

  _matchCBCategory() {
    ContentBlockingCategoriesPrefs.matchCBCategory();
  },

  _updateCBCategory() {
    ContentBlockingCategoriesPrefs.updateCBCategory();
  },

  _recordContentBlockingTelemetry() {
    Services.telemetry.setEventRecordingEnabled(
      "security.ui.protectionspopup",
      Services.prefs.getBoolPref(
        "security.protectionspopup.recordEventTelemetry"
      )
    );
    Services.telemetry.setEventRecordingEnabled(
      "security.ui.app_menu",
      Services.prefs.getBoolPref("security.app_menu.recordEventTelemetry")
    );

    let tpEnabled = Services.prefs.getBoolPref(
      "privacy.trackingprotection.enabled"
    );
    Services.telemetry
      .getHistogramById("TRACKING_PROTECTION_ENABLED")
      .add(tpEnabled);

    let tpPBDisabled = Services.prefs.getBoolPref(
      "privacy.trackingprotection.pbmode.enabled"
    );
    Services.telemetry
      .getHistogramById("TRACKING_PROTECTION_PBM_DISABLED")
      .add(!tpPBDisabled);

    let cookieBehavior = Services.prefs.getIntPref(
      "network.cookie.cookieBehavior"
    );
    Services.telemetry.getHistogramById("COOKIE_BEHAVIOR").add(cookieBehavior);

    let fpEnabled = Services.prefs.getBoolPref(
      "privacy.trackingprotection.fingerprinting.enabled"
    );
    let cmEnabled = Services.prefs.getBoolPref(
      "privacy.trackingprotection.cryptomining.enabled"
    );
    let categoryPref;
    switch (
      Services.prefs.getStringPref("browser.contentblocking.category", null)
    ) {
      case "standard":
        categoryPref = 0;
        break;
      case "strict":
        categoryPref = 1;
        break;
      case "custom":
        categoryPref = 2;
        break;
      default:
        // Any other value is unsupported.
        categoryPref = 3;
        break;
    }

    Services.telemetry.scalarSet(
      "contentblocking.fingerprinting_blocking_enabled",
      fpEnabled
    );
    Services.telemetry.scalarSet(
      "contentblocking.cryptomining_blocking_enabled",
      cmEnabled
    );
    Services.telemetry.scalarSet("contentblocking.category", categoryPref);
  },

  _recordDataSanitizationPrefs() {
    Services.telemetry.scalarSet(
      "datasanitization.network_cookie_lifetimePolicy",
      Services.prefs.getIntPref("network.cookie.lifetimePolicy")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_sanitize_sanitizeOnShutdown",
      Services.prefs.getBoolPref("privacy.sanitize.sanitizeOnShutdown")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_cookies",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.cookies")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_history",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.history")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_formdata",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.formdata")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_downloads",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.downloads")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_cache",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.cache")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_sessions",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.sessions")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_offlineApps",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.offlineApps")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_siteSettings",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.siteSettings")
    );
    Services.telemetry.scalarSet(
      "datasanitization.privacy_clearOnShutdown_openWindows",
      Services.prefs.getBoolPref("privacy.clearOnShutdown.openWindows")
    );

    let exceptions = 0;
    for (let permission of Services.perms.all) {
      // We consider just permissions set for http, https and file URLs.
      if (
        permission.type == "cookie" &&
        permission.capability == Ci.nsICookiePermission.ACCESS_SESSION &&
        ["http", "https", "file"].some(scheme =>
          permission.principal.schemeIs(scheme)
        )
      ) {
        exceptions++;
      }
    }
    Services.telemetry.scalarSet(
      "datasanitization.session_permission_exceptions",
      exceptions
    );
  },

  /**
   * Application shutdown handler.
   */
  _onQuitApplicationGranted() {
    let tasks = [
      // This pref must be set here because SessionStore will use its value
      // on quit-application.
      () => this._setPrefToSaveSession(),

      // Call trackStartupCrashEnd here in case the delayed call on startup hasn't
      // yet occurred (see trackStartupCrashEnd caller in browser.js).
      () => Services.startup.trackStartupCrashEnd(),

      () => {
        if (this._bookmarksBackupIdleTime) {
          this._userIdleService.removeIdleObserver(
            this,
            this._bookmarksBackupIdleTime
          );
          this._bookmarksBackupIdleTime = null;
        }
      },

      () => BrowserUsageTelemetry.uninit(),
      () => SearchSERPTelemetry.uninit(),
      () => Interactions.uninit(),
      () => PageDataService.uninit(),
      () => PageThumbs.uninit(),
      () => NewTabUtils.uninit(),
      () => Normandy.uninit(),
      () => RFPHelper.uninit(),
      () => ASRouterNewTabHook.destroy(),
    ];

    tasks.push(
      ...Object.values(initializedModules)
        .filter(m => m.uninit)
        .map(m => () => m.uninit())
    );

    for (let task of tasks) {
      try {
        task();
      } catch (ex) {
        console.error(`Error during quit-application-granted: ${ex}`);
        if (Cu.isInAutomation) {
          // This usually happens after the test harness is done collecting
          // test errors, thus we can't easily add a failure to it. The only
          // noticeable solution we have is crashing.
          Cc["@mozilla.org/xpcom/debug;1"]
            .getService(Ci.nsIDebug2)
            .abort(ex.filename, ex.lineNumber);
        }
      }
    }
  },

  // Set up a listener to enable/disable the screenshots extension
  // based on its preference.
  _monitorScreenshotsPref() {
    const PREF = "extensions.screenshots.disabled";
    const ID = "screenshots@mozilla.org";
    const _checkScreenshotsPref = async () => {
      let addon = await AddonManager.getAddonByID(ID);
      let disabled = Services.prefs.getBoolPref(PREF, false);
      if (disabled) {
        await addon.disable({ allowSystemAddons: true });
      } else {
        await addon.enable({ allowSystemAddons: true });
      }
    };
    Services.prefs.addObserver(PREF, _checkScreenshotsPref);
    _checkScreenshotsPref();
  },

  _monitorWebcompatReporterPref() {
    const PREF = "extensions.webcompat-reporter.enabled";
    const ID = "webcompat-reporter@mozilla.org";
    Services.prefs.addObserver(PREF, async () => {
      let addon = await AddonManager.getAddonByID(ID);
      let enabled = Services.prefs.getBoolPref(PREF, false);
      if (enabled && !addon.isActive) {
        await addon.enable({ allowSystemAddons: true });
      } else if (!enabled && addon.isActive) {
        await addon.disable({ allowSystemAddons: true });
      }
    });
  },

  // Set up a listener to enable/disable the translation extension
  // based on its preference.
  _monitorTranslationsPref() {
    const PREF = "extensions.translations.disabled";
    const ID = "firefox-translations@mozilla.org";
    const oldID = "firefox-infobar-ui-bergamot-browser-extension@browser.mt";

    // First, try to uninstall the old extension, if exists.
    (async () => {
      let addon = await AddonManager.getAddonByID(oldID);
      if (addon) {
        addon.uninstall().catch(Cu.reportError);
      }
    })();

    const _checkTranslationsPref = async () => {
      let addon = await AddonManager.getAddonByID(ID);
      let disabled = Services.prefs.getBoolPref(PREF, false);
      if (!addon && disabled) {
        // not installed, bail out early.
        return;
      }
      if (!disabled) {
        // first time install of addon and install on firefox update
        addon =
          (await AddonManager.maybeInstallBuiltinAddon(
            ID,
            "0.4.3",
            "resource://builtin-addons/translations/"
          )) || addon;
        await addon.enable();
      } else if (addon) {
        await addon.disable();
      }
    };
    Services.prefs.addObserver(PREF, _checkTranslationsPref);
    _checkTranslationsPref();
  },

  async _setupSearchDetection() {
    // There is no pref for this add-on because it shouldn't be disabled.
    const ID = "addons-search-detection@mozilla.com";

    let addon = await AddonManager.getAddonByID(ID);

    // first time install of addon and install on firefox update
    addon =
      (await AddonManager.maybeInstallBuiltinAddon(
        ID,
        "2.0.0",
        "resource://builtin-addons/search-detection/"
      )) || addon;

    if (!addon.isActive) {
      addon.enable();
    }
  },

  _monitorHTTPSOnlyPref() {
    const PREF_ENABLED = "dom.security.https_only_mode";
    const PREF_WAS_ENABLED = "dom.security.https_only_mode_ever_enabled";
    const _checkHTTPSOnlyPref = async () => {
      const enabled = Services.prefs.getBoolPref(PREF_ENABLED, false);
      const was_enabled = Services.prefs.getBoolPref(PREF_WAS_ENABLED, false);
      let value = 0;
      if (enabled) {
        value = 1;
        Services.prefs.setBoolPref(PREF_WAS_ENABLED, true);
      } else if (was_enabled) {
        value = 2;
      }
      Services.telemetry.scalarSet("security.https_only_mode_enabled", value);
    };

    Services.prefs.addObserver(PREF_ENABLED, _checkHTTPSOnlyPref);
    _checkHTTPSOnlyPref();

    const PREF_PBM_WAS_ENABLED =
      "dom.security.https_only_mode_ever_enabled_pbm";
    const PREF_PBM_ENABLED = "dom.security.https_only_mode_pbm";

    const _checkHTTPSOnlyPBMPref = async () => {
      const enabledPBM = Services.prefs.getBoolPref(PREF_PBM_ENABLED, false);
      const was_enabledPBM = Services.prefs.getBoolPref(
        PREF_PBM_WAS_ENABLED,
        false
      );
      let valuePBM = 0;
      if (enabledPBM) {
        valuePBM = 1;
        Services.prefs.setBoolPref(PREF_PBM_WAS_ENABLED, true);
      } else if (was_enabledPBM) {
        valuePBM = 2;
      }
      Services.telemetry.scalarSet(
        "security.https_only_mode_enabled_pbm",
        valuePBM
      );
    };

    Services.prefs.addObserver(PREF_PBM_ENABLED, _checkHTTPSOnlyPBMPref);
    _checkHTTPSOnlyPBMPref();
  },

  _monitorIonPref() {
    const PREF_ION_ID = "toolkit.telemetry.pioneerId";

    const _checkIonPref = async () => {
      for (let win of Services.wm.getEnumerator("navigator:browser")) {
        win.document.getElementById(
          "ion-button"
        ).hidden = !Services.prefs.getStringPref(PREF_ION_ID, null);
      }
    };

    const windowListener = {
      onOpenWindow(xulWindow) {
        const win = xulWindow.docShell.domWindow;
        win.addEventListener("load", () => {
          const ionButton = win.document.getElementById("ion-button");
          if (ionButton) {
            ionButton.hidden = !Services.prefs.getStringPref(PREF_ION_ID, null);
          }
        });
      },
      onCloseWindow() {},
    };

    Services.prefs.addObserver(PREF_ION_ID, _checkIonPref);
    Services.wm.addListener(windowListener);
    _checkIonPref();
  },

  _monitorIonStudies() {
    const STUDY_ADDON_COLLECTION_KEY = "pioneer-study-addons-v1";
    const PREF_ION_NEW_STUDIES_AVAILABLE =
      "toolkit.telemetry.pioneer-new-studies-available";

    const _badgeIcon = async () => {
      for (let win of Services.wm.getEnumerator("navigator:browser")) {
        win.document
          .getElementById("ion-button")
          .querySelector(".toolbarbutton-badge")
          .classList.add("feature-callout");
      }
    };

    const windowListener = {
      onOpenWindow(xulWindow) {
        const win = xulWindow.docShell.domWindow;
        win.addEventListener("load", () => {
          const ionButton = win.document.getElementById("ion-button");
          if (ionButton) {
            const badge = ionButton.querySelector(".toolbarbutton-badge");
            if (
              Services.prefs.getBoolPref(PREF_ION_NEW_STUDIES_AVAILABLE, false)
            ) {
              badge.classList.add("feature-callout");
            } else {
              badge.classList.remove("feature-callout");
            }
          }
        });
      },
      onCloseWindow() {},
    };

    // Update all open windows if the pref changes.
    Services.prefs.addObserver(PREF_ION_NEW_STUDIES_AVAILABLE, _badgeIcon);

    // Badge any currently-open windows.
    if (Services.prefs.getBoolPref(PREF_ION_NEW_STUDIES_AVAILABLE, false)) {
      _badgeIcon();
    }

    RemoteSettings(STUDY_ADDON_COLLECTION_KEY).on("sync", async event => {
      Services.prefs.setBoolPref(PREF_ION_NEW_STUDIES_AVAILABLE, true);
    });

    // When a new window opens, check if we need to badge the icon.
    Services.wm.addListener(windowListener);
  },

  _monitorGPCPref() {
    const FEATURE_PREF_ENABLED = "privacy.globalprivacycontrol.enabled";
    const FUNCTIONALITY_PREF_ENABLED =
      "privacy.globalprivacycontrol.functionality.enabled";
    const PREF_WAS_ENABLED = "privacy.globalprivacycontrol.was_ever_enabled";
    const _checkGPCPref = async () => {
      const feature_enabled = Services.prefs.getBoolPref(
        FEATURE_PREF_ENABLED,
        false
      );
      const functionality_enabled = Services.prefs.getBoolPref(
        FUNCTIONALITY_PREF_ENABLED,
        false
      );
      const was_enabled = Services.prefs.getBoolPref(PREF_WAS_ENABLED, false);
      let value = 0;
      if (feature_enabled && functionality_enabled) {
        value = 1;
        Services.prefs.setBoolPref(PREF_WAS_ENABLED, true);
      } else if (was_enabled) {
        value = 2;
      }
      Services.telemetry.scalarSet(
        "security.global_privacy_control_enabled",
        value
      );
    };

    Services.prefs.addObserver(FEATURE_PREF_ENABLED, _checkGPCPref);
    Services.prefs.addObserver(FUNCTIONALITY_PREF_ENABLED, _checkGPCPref);
    _checkGPCPref();
  },

  // All initial windows have opened.
  _onWindowsRestored: function BG__onWindowsRestored() {
    if (this._windowsWereRestored) {
      return;
    }
    this._windowsWereRestored = true;

    BrowserUsageTelemetry.init();
    SearchSERPTelemetry.init();

    Interactions.init();
    PageDataService.init();
    ExtensionsUI.init();

    let signingRequired;
    if (AppConstants.MOZ_REQUIRE_SIGNING) {
      signingRequired = true;
    } else {
      signingRequired = Services.prefs.getBoolPref(
        "xpinstall.signatures.required"
      );
    }

    if (signingRequired) {
      let disabledAddons = AddonManager.getStartupChanges(
        AddonManager.STARTUP_CHANGE_DISABLED
      );
      AddonManager.getAddonsByIDs(disabledAddons).then(addons => {
        for (let addon of addons) {
          if (addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
            this._notifyUnsignedAddonsDisabled();
            break;
          }
        }
      });
    }

    if (AppConstants.MOZ_CRASHREPORTER) {
      UnsubmittedCrashHandler.init();
      UnsubmittedCrashHandler.scheduleCheckForUnsubmittedCrashReports();
      FeatureGate.annotateCrashReporter();
      FeatureGate.observePrefChangesForCrashReportAnnotation();
    }

    if (AppConstants.ASAN_REPORTER) {
      var { AsanReporter } = ChromeUtils.import(
        "resource:///modules/AsanReporter.jsm"
      );
      AsanReporter.init();
    }

    Sanitizer.onStartup();
    this._maybeShowRestoreSessionInfoBar();
    this._scheduleStartupIdleTasks();
    this._lateTasksIdleObserver = (idleService, topic, data) => {
      if (topic == "idle") {
        idleService.removeIdleObserver(
          this._lateTasksIdleObserver,
          LATE_TASKS_IDLE_TIME_SEC
        );
        delete this._lateTasksIdleObserver;
        this._scheduleBestEffortUserIdleTasks();
      }
    };
    this._userIdleService.addIdleObserver(
      this._lateTasksIdleObserver,
      LATE_TASKS_IDLE_TIME_SEC
    );

    this._monitorScreenshotsPref();
    this._monitorWebcompatReporterPref();
    this._monitorHTTPSOnlyPref();
    this._monitorIonPref();
    this._monitorIonStudies();
    this._setupSearchDetection();

    if (AppConstants.NIGHTLY_BUILD) {
      this._monitorTranslationsPref();
    }
    this._monitorGPCPref();
  },

  /**
   * Use this function as an entry point to schedule tasks that
   * need to run only once after startup, and can be scheduled
   * by using an idle callback.
   *
   * The functions scheduled here will fire from idle callbacks
   * once every window has finished being restored by session
   * restore, and it's guaranteed that they will run before
   * the equivalent per-window idle tasks
   * (from _schedulePerWindowIdleTasks in browser.js).
   *
   * If you have something that can wait even further than the
   * per-window initialization, and is okay with not being run in some
   * sessions, please schedule them using
   * _scheduleBestEffortUserIdleTasks.
   * Don't be fooled by thinking that the use of the timeout parameter
   * will delay your function: it will just ensure that it potentially
   * happens _earlier_ than expected (when the timeout limit has been reached),
   * but it will not make it happen later (and out of order) compared
   * to the other ones scheduled together.
   */
  _scheduleStartupIdleTasks() {
    const idleTasks = [
      // It's important that SafeBrowsing is initialized reasonably
      // early, so we use a maximum timeout for it.
      {
        task: () => {
          SafeBrowsing.init();
        },
        timeout: 5000,
      },

      {
        task: async () => {
          await ContextualIdentityService.load();
          Discovery.update();
        },
      },

      {
        task: () => {
          // We postponed loading bookmarks toolbar content until startup
          // has finished, so we can start loading it now:
          PlacesUIUtils.unblockToolbars();
        },
      },

      {
        condition: TelemetryUtils.isTelemetryEnabled,
        task: () => {
          PlacesDBUtils.telemetry().catch(console.error);
        },
      },

      // Begin listening for incoming push messages.
      {
        task: () => {
          try {
            PushService.wrappedJSObject.ensureReady();
          } catch (ex) {
            // NS_ERROR_NOT_AVAILABLE will get thrown for the PushService
            // getter if the PushService is disabled.
            if (ex.result != Cr.NS_ERROR_NOT_AVAILABLE) {
              throw ex;
            }
          }
        },
      },

      {
        task: () => {
          this._recordContentBlockingTelemetry();
        },
      },

      {
        task: () => {
          this._recordDataSanitizationPrefs();
        },
      },

      {
        task: () => {
          let enableCertErrorUITelemetry = Services.prefs.getBoolPref(
            "security.certerrors.recordEventTelemetry",
            true
          );
          Services.telemetry.setEventRecordingEnabled(
            "security.ui.certerror",
            enableCertErrorUITelemetry
          );
        },
      },

      // Load the Login Manager data from disk off the main thread, some time
      // after startup.  If the data is required before this runs, for example
      // because a restored page contains a password field, it will be loaded on
      // the main thread, and this initialization request will be ignored.
      {
        task: () => {
          try {
            Services.logins;
          } catch (ex) {
            Cu.reportError(ex);
          }
        },
        timeout: 3000,
      },

      // Add breach alerts pref observer reasonably early so the pref flip works
      {
        task: () => {
          this._addBreachAlertsPrefObserver();
        },
      },

      // Report pinning status and the type of shortcut used to launch
      {
        condition: AppConstants.platform == "win",
        task: async () => {
          let shellService = Cc[
            "@mozilla.org/browser/shell-service;1"
          ].getService(Ci.nsIWindowsShellService);

          try {
            Services.telemetry.scalarSet(
              "os.environment.is_taskbar_pinned",
              await shellService.isCurrentAppPinnedToTaskbarAsync()
            );
          } catch (ex) {
            Cu.reportError(ex);
          }

          let classification;
          let shortcut;
          try {
            shortcut = Services.appinfo.processStartupShortcut;
            classification = shellService.classifyShortcut(shortcut);
          } catch (ex) {
            Cu.reportError(ex);
          }

          if (!classification) {
            if (shortcut) {
              classification = "OtherShortcut";
            } else {
              classification = "Other";
            }
          }

          Services.telemetry.scalarSet(
            "os.environment.launch_method",
            classification
          );
        },
      },

      // Install built-in themes. We already installed the active built-in
      // theme, if any, before UI startup.
      {
        task: async () => {
          await BuiltInThemes.ensureBuiltInThemes();
        },
      },

      {
        condition: AppConstants.platform == "win",
        task: () => {
          // For Windows 7, initialize the jump list module.
          const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
          if (
            WINTASKBAR_CONTRACTID in Cc &&
            Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available
          ) {
            let temp = {};
            ChromeUtils.import(
              "resource:///modules/WindowsJumpLists.jsm",
              temp
            );
            temp.WinTaskbarJumpList.startup();
          }
        },
      },

      // Report macOS Dock status
      {
        condition: AppConstants.platform == "macosx",
        task: () => {
          try {
            Services.telemetry.scalarSet(
              "os.environment.is_kept_in_dock",
              Cc["@mozilla.org/widget/macdocksupport;1"].getService(
                Ci.nsIMacDockSupport
              ).isAppInDock
            );
          } catch (ex) {
            Cu.reportError(ex);
          }
        },
      },

      {
        task: () => {
          this._maybeShowDefaultBrowserPrompt();
        },
      },

      {
        task: () => {
          if (
            Services.prefs.getBoolPref("screenshots.browser.component.enabled")
          ) {
            ScreenshotsUtils.initialize();
          }
        },
      },

      {
        task: () => {
          UrlbarQuickSuggest.maybeShowOnboardingDialog();
        },
      },

      {
        task: () => {
          let { setTimeout } = ChromeUtils.import(
            "resource://gre/modules/Timer.jsm"
          );
          setTimeout(function() {
            Services.tm.idleDispatchToMainThread(
              Services.startup.trackStartupCrashEnd
            );
          }, STARTUP_CRASHES_END_DELAY_MS);
        },
      },

      {
        task: () => {
          let handlerService = Cc[
            "@mozilla.org/uriloader/handler-service;1"
          ].getService(Ci.nsIHandlerService);
          handlerService.asyncInit();
        },
      },

      {
        condition: AppConstants.platform == "win",
        task: () => {
          JawsScreenReaderVersionCheck.onWindowsRestored();
        },
      },

      {
        task: () => {
          RFPHelper.init();
        },
      },

      {
        task: () => {
          Blocklist.loadBlocklistAsync();
        },
      },

      {
        task: () => {
          TabUnloader.init();
        },
      },

      {
        task: () => {
          // Init the url query stripping list.
          let urlQueryStrippingListService = Cc[
            "@mozilla.org/query-stripping-list-service;1"
          ].getService(Ci.nsIURLQueryStrippingListService);
          urlQueryStrippingListService.init();
        },
      },

      // Run TRR performance measurements for DoH.
      {
        task: () => {
          let enabledPref = "doh-rollout.trrRace.enabled";
          let completePref = "doh-rollout.trrRace.complete";

          if (Services.prefs.getBoolPref(enabledPref, false)) {
            if (!Services.prefs.getBoolPref(completePref, false)) {
              new TRRRacer().run(() => {
                Services.prefs.setBoolPref(completePref, true);
              });
            }
          } else {
            Services.prefs.addObserver(enabledPref, function observer() {
              if (Services.prefs.getBoolPref(enabledPref, false)) {
                Services.prefs.removeObserver(enabledPref, observer);

                if (!Services.prefs.getBoolPref(completePref, false)) {
                  new TRRRacer().run(() => {
                    Services.prefs.setBoolPref(completePref, true);
                  });
                }
              }
            });
          }
        },
      },

      // FOG doesn't need to be initialized _too_ early because it has a
      // pre-init buffer.
      {
        task: () => {
          let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(
            Ci.nsIFOG
          );
          FOG.initializeFOG();
        },
      },

      // Add the import button if this is the first startup.
      {
        task: async () => {
          // First check if we've already added the import button, in which
          // case we should check for events indicating we can remove it.
          if (
            Services.prefs.getBoolPref(
              "browser.bookmarks.addedImportButton",
              false
            )
          ) {
            PlacesUIUtils.removeImportButtonWhenImportSucceeds();
            return;
          }

          // Otherwise, check if this is a new profile where we need to add it.
          // `maybeAddImportButton` will call
          // `removeImportButtonWhenImportSucceeds`itself if/when it adds the
          // button. Doing things in this order avoids listening for removal
          // more than once.
          if (
            this._isNewProfile &&
            // Not in automation: the button changes CUI state, breaking tests
            !Cu.isInAutomation
          ) {
            await PlacesUIUtils.maybeAddImportButton();
          }
        },
      },

      {
        task: () => {
          ASRouterNewTabHook.createInstance(ASRouterDefaultConfig());
        },
      },

      {
        task: () => {
          PlacesUIUtils.ensureBookmarkToolbarTelemetryListening();
        },
      },

      {
        condition: AppConstants.MOZ_UPDATE_AGENT,
        task: () => {
          // Never in automation!  This is close to
          // `UpdateService.disabledForTesting`, but without creating the
          // service, which can perform a good deal of I/O in order to log its
          // state.  Since this is in the startup path, we avoid all of that.
          // We also don't test for Marionette and Remote Agent since they are
          // not yet initialized.
          let disabledForTesting =
            Cu.isInAutomation &&
            Services.prefs.getBoolPref("app.update.disabledForTesting", false);
          if (!disabledForTesting) {
            BackgroundUpdate.maybeScheduleBackgroundUpdateTask();
          }
        },
      },

      // Login detection service is used in fission to identify high value sites.
      {
        task: () => {
          let loginDetection = Cc[
            "@mozilla.org/login-detection-service;1"
          ].createInstance(Ci.nsILoginDetectionService);
          loginDetection.init();
        },
      },

      // WebDriver components (Remote Agent and Marionette) need to be
      // initialized as very last step.
      {
        condition: AppConstants.ENABLE_WEBDRIVER,
        task: () => {
          // Use idleDispatch a second time to run this after the per-window
          // idle tasks.
          ChromeUtils.idleDispatch(() => {
            Services.obs.notifyObservers(
              null,
              "browser-startup-idle-tasks-finished"
            );

            // Request startup of the Remote Agent (support for WebDriver BiDi
            // and the partial Chrome DevTools protocol) before Marionette.
            Services.obs.notifyObservers(null, "remote-startup-requested");
            Services.obs.notifyObservers(null, "marionette-startup-requested");
          });
        },
      },
      // Do NOT add anything after WebDriver initialization.
    ];

    for (let task of idleTasks) {
      if ("condition" in task && !task.condition) {
        continue;
      }

      ChromeUtils.idleDispatch(
        () => {
          if (!Services.startup.shuttingDown) {
            let startTime = Cu.now();
            try {
              task.task();
            } catch (ex) {
              Cu.reportError(ex);
            } finally {
              ChromeUtils.addProfilerMarker("startupIdleTask", startTime);
            }
          }
        },
        task.timeout ? { timeout: task.timeout } : undefined
      );
    }
  },

  /**
   * Use this function as an entry point to schedule tasks that we hope
   * to run once per session, at any arbitrary point in time, and which we
   * are okay with sometimes not running at all.
   *
   * This function will be called from an idle observer. Check the value of
   * LATE_TASKS_IDLE_TIME_SEC to see the current value for this idle
   * observer.
   *
   * Note: this function may never be called if the user is never idle for the
   * requisite time (LATE_TASKS_IDLE_TIME_SEC). Be certain before adding
   * something here that it's okay that it never be run.
   */
  _scheduleBestEffortUserIdleTasks() {
    const idleTasks = [
      () => {
        // Telemetry for master-password - we do this after a delay as it
        // can cause IO if NSS/PSM has not already initialized.
        let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(
          Ci.nsIPK11TokenDB
        );
        let token = tokenDB.getInternalKeyToken();
        let mpEnabled = token.hasPassword;
        if (mpEnabled) {
          Services.telemetry
            .getHistogramById("MASTER_PASSWORD_ENABLED")
            .add(mpEnabled);
        }
      },

      () => {
        let obj = {};
        ChromeUtils.import("resource://gre/modules/GMPInstallManager.jsm", obj);
        this._gmpInstallManager = new obj.GMPInstallManager();
        // We don't really care about the results, if someone is interested they
        // can check the log.
        this._gmpInstallManager.simpleCheckAndInstall().catch(() => {});
      },

      () => {
        RemoteSettings.init();
        this._addBreachesSyncHandler();
      },

      () => {
        PublicSuffixList.init();
      },

      () => {
        RemoteSecuritySettings.init();
      },

      () => {
        if (Services.prefs.getBoolPref("corroborator.enabled", false)) {
          Corroborate.init().catch(Cu.reportError);
        }
      },

      () => BrowserUsageTelemetry.reportProfileCount(),

      () => OsEnvironment.reportAllowedAppSources(),

      () => Services.search.runBackgroundChecks(),

      () => BrowserUsageTelemetry.reportInstallationTelemetry(),
    ];

    for (let task of idleTasks) {
      ChromeUtils.idleDispatch(async () => {
        if (!Services.startup.shuttingDown) {
          let startTime = Cu.now();
          try {
            await task();
          } catch (ex) {
            Cu.reportError(ex);
          } finally {
            ChromeUtils.addProfilerMarker("startupLateIdleTask", startTime);
          }
        }
      });
    }
  },

  _addBreachesSyncHandler() {
    if (
      Services.prefs.getBoolPref(
        "signon.management.page.breach-alerts.enabled",
        false
      )
    ) {
      RemoteSettings(LoginBreaches.REMOTE_SETTINGS_COLLECTION).on(
        "sync",
        async event => {
          await LoginBreaches.update(event.data.current);
        }
      );
    }
  },

  _addBreachAlertsPrefObserver() {
    const BREACH_ALERTS_PREF = "signon.management.page.breach-alerts.enabled";
    const clearVulnerablePasswordsIfBreachAlertsDisabled = async function() {
      if (!Services.prefs.getBoolPref(BREACH_ALERTS_PREF)) {
        await LoginBreaches.clearAllPotentiallyVulnerablePasswords();
      }
    };
    clearVulnerablePasswordsIfBreachAlertsDisabled();
    Services.prefs.addObserver(
      BREACH_ALERTS_PREF,
      clearVulnerablePasswordsIfBreachAlertsDisabled
    );
  },

  _quitSource: "unknown",
  _registerQuitSource(source) {
    this._quitSource = source;
  },

  _onQuitRequest: function BG__onQuitRequest(aCancelQuit, aQuitType) {
    // If user has already dismissed quit request, then do nothing
    if (aCancelQuit instanceof Ci.nsISupportsPRBool && aCancelQuit.data) {
      return;
    }

    // There are several cases where we won't show a dialog here:
    // 1. There is only 1 tab open in 1 window
    // 2. browser.warnOnQuit == false
    // 3. The browser is currently in Private Browsing mode
    // 4. The browser will be restarted.
    // 5. The user has automatic session restore enabled and
    //    browser.sessionstore.warnOnQuit is not set to true.
    // 6. The user doesn't have automatic session restore enabled
    //    and browser.tabs.warnOnClose is not set to true.
    //
    // Otherwise, we will show the "closing multiple tabs" dialog.
    //
    // aQuitType == "lastwindow" is overloaded. "lastwindow" is used to indicate
    // "the last window is closing but we're not quitting (a non-browser window is open)"
    // and also "we're quitting by closing the last window".

    if (aQuitType == "restart" || aQuitType == "os-restart") {
      return;
    }

    // browser.warnOnQuit is a hidden global boolean to override all quit prompts.
    if (!Services.prefs.getBoolPref("browser.warnOnQuit")) {
      return;
    }

    var windowcount = 0;
    var pagecount = 0;
    let pinnedcount = 0;
    for (let win of BrowserWindowTracker.orderedWindows) {
      if (win.closed) {
        continue;
      }
      windowcount++;
      let tabbrowser = win.gBrowser;
      if (tabbrowser) {
        pinnedcount += tabbrowser._numPinnedTabs;
        pagecount +=
          tabbrowser.browsers.length -
          tabbrowser._numPinnedTabs -
          tabbrowser._removingTabs.length;
      }
    }

    // No windows open so no need for a warning.
    if (!windowcount) {
      return;
    }

    // browser.warnOnQuitShortcut is checked when quitting using the shortcut key.
    // The warning will appear even when only one window/tab is open. For other
    // methods of quitting, the warning only appears when there is more than one
    // window or tab open.
    let shouldWarnForShortcut =
      this._quitSource == "shortcut" &&
      Services.prefs.getBoolPref("browser.warnOnQuitShortcut");
    let shouldWarnForTabs =
      pagecount >= 2 && Services.prefs.getBoolPref("browser.tabs.warnOnClose");
    if (!shouldWarnForTabs && !shouldWarnForShortcut) {
      return;
    }

    if (!aQuitType) {
      aQuitType = "quit";
    }

    let win = BrowserWindowTracker.getTopWindow();

    // Our prompt for quitting is most important, so replace others.
    win.gDialogBox.replaceDialogIfOpen();

    let title, buttonLabel;
    // More than 1 window. Compose our own message.
    if (windowcount > 1) {
      title = gTabbrowserBundle.GetStringFromName("tabs.closeWindowsTitle");
      title = PluralForm.get(windowcount, title).replace(/#1/, windowcount);

      buttonLabel =
        AppConstants.platform == "win"
          ? "tabs.closeWindowsButtonWin"
          : "tabs.closeWindowsButton";
      buttonLabel = gTabbrowserBundle.GetStringFromName(buttonLabel);
    } else if (shouldWarnForShortcut) {
      let productName = gBrandBundle.GetStringFromName("brandShorterName");
      title = gTabbrowserBundle.formatStringFromName(
        "tabs.closeTabsWithKeyTitle",
        [productName]
      );
      buttonLabel = gTabbrowserBundle.formatStringFromName(
        "tabs.closeTabsWithKeyButton",
        [productName]
      );
    } else {
      title = gTabbrowserBundle.GetStringFromName("tabs.closeTabsTitle");
      title = PluralForm.get(pagecount, title).replace("#1", pagecount);
      buttonLabel = gTabbrowserBundle.GetStringFromName(
        "tabs.closeButtonMultiple"
      );
    }

    // The checkbox label is different depending on whether the shortcut
    // was used to quit or not.
    let checkboxLabel;
    if (shouldWarnForShortcut) {
      let quitKeyElement = win.document.getElementById("key_quitApplication");
      let quitKey = ShortcutUtils.prettifyShortcut(quitKeyElement);

      checkboxLabel = gTabbrowserBundle.formatStringFromName(
        "tabs.closeTabsWithKeyConfirmCheckbox",
        [quitKey]
      );
    } else {
      checkboxLabel = gTabbrowserBundle.GetStringFromName(
        "tabs.closeTabsConfirmCheckbox"
      );
    }

    let warnOnClose = { value: true };
    let flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1;
    // buttonPressed will be 0 for closing, 1 for cancel (don't close/quit)
    let buttonPressed = Services.prompt.confirmEx(
      win,
      title,
      null,
      flags,
      buttonLabel,
      null,
      null,
      checkboxLabel,
      warnOnClose
    );
    Services.telemetry.setEventRecordingEnabled("close_tab_warning", true);
    let warnCheckbox = warnOnClose.value ? "checked" : "unchecked";

    let sessionWillBeRestored =
      Services.prefs.getIntPref("browser.startup.page") == 3 ||
      Services.prefs.getBoolPref("browser.sessionstore.resume_session_once");
    Services.telemetry.recordEvent(
      "close_tab_warning",
      "shown",
      "application",
      null,
      {
        source: this._quitSource,
        button: buttonPressed == 0 ? "close" : "cancel",
        warn_checkbox: warnCheckbox,
        closing_wins: "" + windowcount,
        closing_tabs: "" + (pagecount + pinnedcount),
        will_restore: sessionWillBeRestored ? "yes" : "no",
      }
    );

    // If the user has unticked the box, and has confirmed closing, stop showing
    // the warning.
    if (buttonPressed == 0 && !warnOnClose.value) {
      if (shouldWarnForShortcut) {
        Services.prefs.setBoolPref("browser.warnOnQuitShortcut", false);
      } else {
        Services.prefs.setBoolPref("browser.tabs.warnOnClose", false);
      }
    }

    this._quitSource = "unknown";

    aCancelQuit.data = buttonPressed != 0;
  },

  /**
   * Initialize Places
   * - imports the bookmarks html file if bookmarks database is empty, try to
   *   restore bookmarks from a JSON backup if the backend indicates that the
   *   database was corrupt.
   *
   * These prefs can be set up by the frontend:
   *
   * WARNING: setting these preferences to true will overwite existing bookmarks
   *
   * - browser.places.importBookmarksHTML
   *   Set to true will import the bookmarks.html file from the profile folder.
   * - browser.bookmarks.restore_default_bookmarks
   *   Set to true by safe-mode dialog to indicate we must restore default
   *   bookmarks.
   */
  _initPlaces: function BG__initPlaces(aInitialMigrationPerformed) {
    if (this._placesInitialized) {
      throw new Error("Cannot initialize Places more than once");
    }
    this._placesInitialized = true;

    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    let dbStatus = PlacesUtils.history.databaseStatus;

    // Show a notification with a "more info" link for a locked places.sqlite.
    if (dbStatus == PlacesUtils.history.DATABASE_STATUS_LOCKED) {
      // Note: initPlaces should always happen when the first window is ready,
      // in any case, better safe than sorry.
      this._firstWindowReady.then(() => {
        this._showPlacesLockedNotificationBox();
        this._placesBrowserInitComplete = true;
        Services.obs.notifyObservers(null, "places-browser-init-complete");
      });
      return;
    }

    let importBookmarks =
      !aInitialMigrationPerformed &&
      (dbStatus == PlacesUtils.history.DATABASE_STATUS_CREATE ||
        dbStatus == PlacesUtils.history.DATABASE_STATUS_CORRUPT);

    // Check if user or an extension has required to import bookmarks.html
    let importBookmarksHTML = false;
    try {
      importBookmarksHTML = Services.prefs.getBoolPref(
        "browser.places.importBookmarksHTML"
      );
      if (importBookmarksHTML) {
        importBookmarks = true;
      }
    } catch (ex) {}

    // Support legacy bookmarks.html format for apps that depend on that format.
    let autoExportHTML = Services.prefs.getBoolPref(
      "browser.bookmarks.autoExportHTML",
      false
    ); // Do not export.
    if (autoExportHTML) {
      // Sqlite.jsm and Places shutdown happen at profile-before-change, thus,
      // to be on the safe side, this should run earlier.
      AsyncShutdown.profileChangeTeardown.addBlocker(
        "Places: export bookmarks.html",
        () => BookmarkHTMLUtils.exportToFile(BookmarkHTMLUtils.defaultPath)
      );
    }

    (async () => {
      // Check if Safe Mode or the user has required to restore bookmarks from
      // default profile's bookmarks.html
      let restoreDefaultBookmarks = false;
      try {
        restoreDefaultBookmarks = Services.prefs.getBoolPref(
          "browser.bookmarks.restore_default_bookmarks"
        );
        if (restoreDefaultBookmarks) {
          // Ensure that we already have a bookmarks backup for today.
          await this._backupBookmarks();
          importBookmarks = true;
        }
      } catch (ex) {}

      // If the user did not require to restore default bookmarks, or import
      // from bookmarks.html, we will try to restore from JSON
      if (importBookmarks && !restoreDefaultBookmarks && !importBookmarksHTML) {
        // get latest JSON backup
        let lastBackupFile = await PlacesBackups.getMostRecentBackup();
        if (lastBackupFile) {
          // restore from JSON backup
          await BookmarkJSONUtils.importFromFile(lastBackupFile, {
            replace: true,
            source: PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
          });
          importBookmarks = false;
        } else {
          // We have created a new database but we don't have any backup available
          importBookmarks = true;
          if (await IOUtils.exists(BookmarkHTMLUtils.defaultPath)) {
            // If bookmarks.html is available in current profile import it...
            importBookmarksHTML = true;
          } else {
            // ...otherwise we will restore defaults
            restoreDefaultBookmarks = true;
          }
        }
      }

      // Import default bookmarks when necessary.
      // Otherwise, if any kind of import runs, default bookmarks creation should be
      // delayed till the import operations has finished.  Not doing so would
      // cause them to be overwritten by the newly imported bookmarks.
      if (!importBookmarks) {
        // Now apply distribution customized bookmarks.
        // This should always run after Places initialization.
        try {
          await this._distributionCustomizer.applyBookmarks();
        } catch (e) {
          Cu.reportError(e);
        }
      } else {
        // An import operation is about to run.
        let bookmarksUrl = null;
        if (restoreDefaultBookmarks) {
          // User wants to restore bookmarks.html file from default profile folder
          bookmarksUrl = "chrome://browser/locale/bookmarks.html";
        } else if (await IOUtils.exists(BookmarkHTMLUtils.defaultPath)) {
          bookmarksUrl = PathUtils.toFileURI(BookmarkHTMLUtils.defaultPath);
        }

        if (bookmarksUrl) {
          // Import from bookmarks.html file.
          try {
            if (Services.policies.isAllowed("defaultBookmarks")) {
              await BookmarkHTMLUtils.importFromURL(bookmarksUrl, {
                replace: true,
                source: PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
              });
            }
          } catch (e) {
            Cu.reportError("Bookmarks.html file could be corrupt. " + e);
          }
          try {
            // Now apply distribution customized bookmarks.
            // This should always run after Places initialization.
            await this._distributionCustomizer.applyBookmarks();
          } catch (e) {
            Cu.reportError(e);
          }
        } else {
          Cu.reportError(new Error("Unable to find bookmarks.html file."));
        }

        // Reset preferences, so we won't try to import again at next run
        if (importBookmarksHTML) {
          Services.prefs.setBoolPref(
            "browser.places.importBookmarksHTML",
            false
          );
        }
        if (restoreDefaultBookmarks) {
          Services.prefs.setBoolPref(
            "browser.bookmarks.restore_default_bookmarks",
            false
          );
        }
      }

      // Initialize bookmark archiving on idle.
      // If the last backup has been created before the last browser session,
      // and is days old, be more aggressive with the idle timer.
      let idleTime = BOOKMARKS_BACKUP_IDLE_TIME_SEC;
      if (!(await PlacesBackups.hasRecentBackup())) {
        idleTime /= 2;
      }
      this._userIdleService.addIdleObserver(this, idleTime);
      this._bookmarksBackupIdleTime = idleTime;

      if (this._isNewProfile) {
        try {
          // New profiles may have existing bookmarks (imported from another browser or
          // copied into the profile) and we want to show the bookmark toolbar for them
          // in some cases.
          PlacesUIUtils.maybeToggleBookmarkToolbarVisibility();
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    })()
      .catch(ex => {
        Cu.reportError(ex);
      })
      .then(() => {
        // NB: deliberately after the catch so that we always do this, even if
        // we threw halfway through initializing in the Task above.
        this._placesBrowserInitComplete = true;
        Services.obs.notifyObservers(null, "places-browser-init-complete");
      });
  },

  /**
   * If a backup for today doesn't exist, this creates one.
   */
  _backupBookmarks: function BG__backupBookmarks() {
    return (async function() {
      let lastBackupFile = await PlacesBackups.getMostRecentBackup();
      // Should backup bookmarks if there are no backups or the maximum
      // interval between backups elapsed.
      if (
        !lastBackupFile ||
        new Date() - PlacesBackups.getDateForFile(lastBackupFile) >
          BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS * 86400000
      ) {
        let maxBackups = Services.prefs.getIntPref(
          "browser.bookmarks.max_backups"
        );
        await PlacesBackups.create(maxBackups);
      }
    })();
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  _showPlacesLockedNotificationBox: function BG__showPlacesLockedNotificationBox() {
    var applicationName = gBrandBundle.GetStringFromName("brandShortName");
    var placesBundle = Services.strings.createBundle(
      "chrome://browser/locale/places/places.properties"
    );
    var text = placesBundle.formatStringFromName("lockPrompt.text", [
      applicationName,
    ]);

    var win = BrowserWindowTracker.getTopWindow();
    var buttons = [{ supportPage: "places-locked" }];

    var notifyBox = win.gBrowser.getNotificationBox();
    var notification = notifyBox.appendNotification(
      "places-locked",
      {
        label: text,
        priority: win.gNotificationBox.PRIORITY_CRITICAL_MEDIUM,
      },
      buttons
    );
    notification.persistence = -1; // Until user closes it
  },

  _onThisDeviceConnected() {
    let bundle = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
    let title = bundle.GetStringFromName("deviceConnDisconnTitle");
    let body = bundle.GetStringFromName("thisDeviceConnectedBody");

    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      this._openPreferences("sync");
    };
    this.AlertsService.showAlertNotification(
      null,
      title,
      body,
      true,
      null,
      clickCallback
    );
  },

  _migrateXULStoreForDocument(fromURL, toURL) {
    Array.from(Services.xulStore.getIDsEnumerator(fromURL)).forEach(id => {
      Array.from(Services.xulStore.getAttributeEnumerator(fromURL, id)).forEach(
        attr => {
          let value = Services.xulStore.getValue(fromURL, id, attr);
          Services.xulStore.setValue(toURL, id, attr, value);
        }
      );
    });
  },

  _migrateHashedKeysForXULStoreForDocument(docUrl) {
    Array.from(Services.xulStore.getIDsEnumerator(docUrl))
      .filter(id => id.startsWith("place:"))
      .forEach(id => {
        Services.xulStore.removeValue(docUrl, id, "open");
        let hashedId = PlacesUIUtils.obfuscateUrlForXulStore(id);
        Services.xulStore.setValue(docUrl, hashedId, "open", "true");
      });
  },

  // eslint-disable-next-line complexity
  _migrateUI: function BG__migrateUI() {
    // Use an increasing number to keep track of the current migration state.
    // Completely unrelated to the current Firefox release number.
    const UI_VERSION = 121;
    const BROWSER_DOCURL = AppConstants.BROWSER_CHROME_URL;

    const PROFILE_DIR = Services.dirsvc.get("ProfD", Ci.nsIFile).path;

    if (!Services.prefs.prefHasUserValue("browser.migration.version")) {
      // This is a new profile, nothing to migrate.
      Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
      this._isNewProfile = true;
      return;
    }

    this._isNewProfile = false;
    let currentUIVersion = Services.prefs.getIntPref(
      "browser.migration.version"
    );
    if (currentUIVersion >= UI_VERSION) {
      return;
    }

    let xulStore = Services.xulStore;

    if (currentUIVersion < 64) {
      IOUtils.remove(PathUtils.join(PROFILE_DIR, "directoryLinks.json"), {
        ignoreAbsent: true,
      });
    }

    if (
      currentUIVersion < 65 &&
      Services.prefs.getCharPref("general.config.filename", "") ==
        "dsengine.cfg"
    ) {
      let searchInitializedPromise = new Promise(resolve => {
        if (Services.search.isInitialized) {
          resolve();
        }
        const SEARCH_SERVICE_TOPIC = "browser-search-service";
        Services.obs.addObserver(function observer(subject, topic, data) {
          if (data != "init-complete") {
            return;
          }
          Services.obs.removeObserver(observer, SEARCH_SERVICE_TOPIC);
          resolve();
        }, SEARCH_SERVICE_TOPIC);
      });
      searchInitializedPromise.then(() => {
        let engineNames = [
          "Bing Search Engine",
          "Yahoo! Search Engine",
          "Yandex Search Engine",
        ];
        for (let engineName of engineNames) {
          let engine = Services.search.getEngineByName(engineName);
          if (engine) {
            Services.search.removeEngine(engine);
          }
        }
      });
    }

    if (currentUIVersion < 67) {
      // Migrate devtools firebug theme users to light theme (bug 1378108):
      if (Services.prefs.getCharPref("devtools.theme") == "firebug") {
        Services.prefs.setCharPref("devtools.theme", "light");
      }
    }

    if (currentUIVersion < 68) {
      // Remove blocklists legacy storage, now relying on IndexedDB.
      IOUtils.remove(PathUtils.join(PROFILE_DIR, "kinto.sqlite"), {
        ignoreAbsent: true,
      });
    }

    if (currentUIVersion < 69) {
      // Clear old social prefs from profile (bug 1460675)
      let socialPrefs = Services.prefs.getBranch("social.");
      if (socialPrefs) {
        let socialPrefsArray = socialPrefs.getChildList("");
        for (let item of socialPrefsArray) {
          Services.prefs.clearUserPref("social." + item);
        }
      }
    }

    if (currentUIVersion < 70) {
      // Migrate old ctrl-tab pref to new one in existing profiles. (This code
      // doesn't run at all in new profiles.)
      Services.prefs.setBoolPref(
        "browser.ctrlTab.recentlyUsedOrder",
        Services.prefs.getBoolPref("browser.ctrlTab.previews", false)
      );
      Services.prefs.clearUserPref("browser.ctrlTab.previews");
      // Remember that we migrated the pref in case we decide to flip it for
      // these users.
      Services.prefs.setBoolPref("browser.ctrlTab.migrated", true);
    }

    if (currentUIVersion < 71) {
      // Clear legacy saved prefs for content handlers.
      let savedContentHandlers = Services.prefs.getChildList(
        "browser.contentHandlers.types"
      );
      for (let savedHandlerPref of savedContentHandlers) {
        Services.prefs.clearUserPref(savedHandlerPref);
      }
    }

    if (currentUIVersion < 72) {
      // Migrate performance tool's recording interval value from msec to usec.
      let pref = "devtools.performance.recording.interval";
      Services.prefs.setIntPref(
        pref,
        Services.prefs.getIntPref(pref, 1) * 1000
      );
    }

    if (currentUIVersion < 73) {
      // Remove blocklist JSON local dumps in profile.
      IOUtils.remove(PathUtils.join(PROFILE_DIR, "blocklists"), {
        recursive: true,
        ignoreAbsent: true,
      });
      IOUtils.remove(PathUtils.join(PROFILE_DIR, "blocklists-preview"), {
        recursive: true,
        ignoreAbsent: true,
      });
      for (const filename of ["addons.json", "plugins.json", "gfx.json"]) {
        // Some old versions used to dump without subfolders. Clean them while we are at it.
        const path = PathUtils.join(PROFILE_DIR, `blocklists-${filename}`);
        IOUtils.remove(path, { ignoreAbsent: true });
      }
    }

    if (currentUIVersion < 76) {
      // Clear old onboarding prefs from profile (bug 1462415)
      let onboardingPrefs = Services.prefs.getBranch("browser.onboarding.");
      if (onboardingPrefs) {
        let onboardingPrefsArray = onboardingPrefs.getChildList("");
        for (let item of onboardingPrefsArray) {
          Services.prefs.clearUserPref("browser.onboarding." + item);
        }
      }
    }

    if (currentUIVersion < 77) {
      // Remove currentset from all the toolbars
      let toolbars = [
        "nav-bar",
        "PersonalToolbar",
        "TabsToolbar",
        "toolbar-menubar",
      ];
      for (let toolbarId of toolbars) {
        xulStore.removeValue(BROWSER_DOCURL, toolbarId, "currentset");
      }
    }

    if (currentUIVersion < 78) {
      Services.prefs.clearUserPref("browser.search.region");
    }

    if (currentUIVersion < 79) {
      // The handler app service will read this. We need to wait with migrating
      // until the handler service has started up, so just set a pref here.
      Services.prefs.setCharPref("browser.handlers.migrations", "30boxes");
    }

    if (currentUIVersion < 80) {
      let hosts = Services.prefs.getCharPref("network.proxy.no_proxies_on");
      // remove "localhost" and "127.0.0.1" from the no_proxies_on list
      const kLocalHosts = new Set(["localhost", "127.0.0.1"]);
      hosts = hosts
        .split(/[ ,]+/)
        .filter(host => !kLocalHosts.has(host))
        .join(", ");
      Services.prefs.setCharPref("network.proxy.no_proxies_on", hosts);
    }

    if (currentUIVersion < 81) {
      // Reset homepage pref for users who have it set to a default from before Firefox 4:
      //   <locale>.(start|start2|start3).mozilla.(com|org)
      if (HomePage.overridden) {
        const DEFAULT = HomePage.getDefault();
        let value = HomePage.get();
        let updated = value.replace(
          /https?:\/\/([\w\-]+\.)?start\d*\.mozilla\.(org|com)[^|]*/gi,
          DEFAULT
        );
        if (updated != value) {
          if (updated == DEFAULT) {
            HomePage.reset();
          } else {
            value = updated;
            HomePage.safeSet(value);
          }
        }
      }
    }

    if (currentUIVersion < 82) {
      this._migrateXULStoreForDocument(
        "chrome://browser/content/browser.xul",
        "chrome://browser/content/browser.xhtml"
      );
    }

    if (currentUIVersion < 83) {
      Services.prefs.clearUserPref("browser.search.reset.status");
    }

    if (currentUIVersion < 84) {
      // Reset flash "always allow/block" permissions
      // We keep session and policy permissions, which could both be
      // the result of enterprise policy settings. "Never/Always allow"
      // settings for flash were actually time-bound on recent-ish Firefoxen,
      // so we remove EXPIRE_TIME entries, too.
      const { EXPIRE_NEVER, EXPIRE_TIME } = Services.perms;
      let flashPermissions = Services.perms
        .getAllWithTypePrefix("plugin:flash")
        .filter(
          p =>
            p.type == "plugin:flash" &&
            (p.expireType == EXPIRE_NEVER || p.expireType == EXPIRE_TIME)
        );
      flashPermissions.forEach(p => Services.perms.removePermission(p));
    }

    // currentUIVersion < 85 is missing due to the following:
    // Origianlly, Bug #1568900 added currentUIVersion 85 but was targeting FF70 release.
    // In between it landing in FF70, Bug #1562601 (currentUIVersion 86) landed and
    // was uplifted to Beta. To make sure the migration doesn't get skipped, the
    // code block that was at 85 has been moved/bumped to currentUIVersion 87.

    if (currentUIVersion < 86) {
      // If the user has set "media.autoplay.allow-muted" to false
      // migrate that to media.autoplay.default=BLOCKED_ALL.
      if (
        Services.prefs.prefHasUserValue("media.autoplay.allow-muted") &&
        !Services.prefs.getBoolPref("media.autoplay.allow-muted") &&
        !Services.prefs.prefHasUserValue("media.autoplay.default") &&
        Services.prefs.getIntPref("media.autoplay.default") ==
          Ci.nsIAutoplay.BLOCKED
      ) {
        Services.prefs.setIntPref(
          "media.autoplay.default",
          Ci.nsIAutoplay.BLOCKED_ALL
        );
      }
      Services.prefs.clearUserPref("media.autoplay.allow-muted");
    }

    if (currentUIVersion < 87) {
      const TRACKING_TABLE_PREF = "urlclassifier.trackingTable";
      const CUSTOM_BLOCKING_PREF =
        "browser.contentblocking.customBlockList.preferences.ui.enabled";
      // Check if user has set custom tables pref, and show custom block list UI
      // in the about:preferences#privacy custom panel.
      if (Services.prefs.prefHasUserValue(TRACKING_TABLE_PREF)) {
        Services.prefs.setBoolPref(CUSTOM_BLOCKING_PREF, true);
      }
    }

    if (currentUIVersion < 88) {
      // If the user the has "browser.contentblocking.category = custom", but has
      // the exact same settings as "standard", move them once to "standard". This is
      // to reset users who we may have moved accidentally, or moved to get ETP early.
      let category_prefs = [
        "network.cookie.cookieBehavior",
        "privacy.trackingprotection.pbmode.enabled",
        "privacy.trackingprotection.enabled",
        "privacy.trackingprotection.socialtracking.enabled",
        "privacy.trackingprotection.fingerprinting.enabled",
        "privacy.trackingprotection.cryptomining.enabled",
      ];
      if (
        Services.prefs.getStringPref(
          "browser.contentblocking.category",
          "standard"
        ) == "custom"
      ) {
        let shouldMigrate = true;
        for (let pref of category_prefs) {
          if (Services.prefs.prefHasUserValue(pref)) {
            shouldMigrate = false;
          }
        }
        if (shouldMigrate) {
          Services.prefs.setStringPref(
            "browser.contentblocking.category",
            "standard"
          );
        }
      }
    }

    if (currentUIVersion < 89) {
      // This file was renamed in https://bugzilla.mozilla.org/show_bug.cgi?id=1595636.
      this._migrateXULStoreForDocument(
        "chrome://devtools/content/framework/toolbox-window.xul",
        "chrome://devtools/content/framework/toolbox-window.xhtml"
      );
    }

    if (currentUIVersion < 90) {
      this._migrateXULStoreForDocument(
        "chrome://browser/content/places/historySidebar.xul",
        "chrome://browser/content/places/historySidebar.xhtml"
      );
      this._migrateXULStoreForDocument(
        "chrome://browser/content/places/places.xul",
        "chrome://browser/content/places/places.xhtml"
      );
      this._migrateXULStoreForDocument(
        "chrome://browser/content/places/bookmarksSidebar.xul",
        "chrome://browser/content/places/bookmarksSidebar.xhtml"
      );
    }

    // Clear socks proxy values if they were shared from http, to prevent
    // websocket breakage after bug 1577862 (see bug 969282).
    if (
      currentUIVersion < 91 &&
      Services.prefs.getBoolPref("network.proxy.share_proxy_settings", false) &&
      Services.prefs.getIntPref("network.proxy.type", 0) == 1
    ) {
      let httpProxy = Services.prefs.getCharPref("network.proxy.http", "");
      let httpPort = Services.prefs.getIntPref("network.proxy.http_port", 0);
      let socksProxy = Services.prefs.getCharPref("network.proxy.socks", "");
      let socksPort = Services.prefs.getIntPref("network.proxy.socks_port", 0);
      if (httpProxy && httpProxy == socksProxy && httpPort == socksPort) {
        Services.prefs.setCharPref(
          "network.proxy.socks",
          Services.prefs.getCharPref("network.proxy.backup.socks", "")
        );
        Services.prefs.setIntPref(
          "network.proxy.socks_port",
          Services.prefs.getIntPref("network.proxy.backup.socks_port", 0)
        );
      }
    }

    if (currentUIVersion < 92) {
      // privacy.userContext.longPressBehavior pref was renamed and changed to a boolean
      let longpress = Services.prefs.getIntPref(
        "privacy.userContext.longPressBehavior",
        0
      );
      if (longpress == 1) {
        Services.prefs.setBoolPref(
          "privacy.userContext.newTabContainerOnLeftClick.enabled",
          true
        );
      }
    }

    if (currentUIVersion < 93) {
      // The Gecko Profiler Addon is now an internal component. Remove the old
      // addon, and enable the new UI.

      function enableProfilerButton(wasAddonActive) {
        // Enable the feature pref. This will add it to the customization palette,
        // but not to the the navbar.
        Services.prefs.setBoolPref(
          "devtools.performance.popup.feature-flag",
          true
        );

        if (wasAddonActive) {
          const { ProfilerMenuButton } = ChromeUtils.import(
            "resource://devtools/client/performance-new/popup/menu-button.jsm.js"
          );
          if (!ProfilerMenuButton.isInNavbar()) {
            // The profiler menu button is not enabled. Turn it on now.
            const win = BrowserWindowTracker.getTopWindow();
            if (win && win.document) {
              ProfilerMenuButton.addToNavbar(win.document);
            }
          }
        }
      }

      let addonPromise;
      try {
        addonPromise = AddonManager.getAddonByID("geckoprofiler@mozilla.com");
      } catch (error) {
        Cu.reportError(
          "Could not access the AddonManager to upgrade the profile. This is most " +
            "likely because the upgrader is being run from an xpcshell test where " +
            "the AddonManager is not initialized."
        );
      }
      Promise.resolve(addonPromise).then(addon => {
        if (!addon) {
          // Either the addon wasn't installed, or the call to getAddonByID failed.
          return;
        }
        // Remove the old addon.
        const wasAddonActive = addon.isActive;
        addon
          .uninstall()
          .catch(Cu.reportError)
          .then(() => enableProfilerButton(wasAddonActive))
          .catch(Cu.reportError);
      }, Cu.reportError);
    }

    // Clear unused socks proxy backup values - see bug 1625773.
    if (currentUIVersion < 94) {
      let backup = Services.prefs.getCharPref("network.proxy.backup.socks", "");
      let backupPort = Services.prefs.getIntPref(
        "network.proxy.backup.socks_port",
        0
      );
      let socksProxy = Services.prefs.getCharPref("network.proxy.socks", "");
      let socksPort = Services.prefs.getIntPref("network.proxy.socks_port", 0);
      if (backup == socksProxy) {
        Services.prefs.clearUserPref("network.proxy.backup.socks");
      }
      if (backupPort == socksPort) {
        Services.prefs.clearUserPref("network.proxy.backup.socks_port");
      }
    }

    if (currentUIVersion < 95) {
      const oldPrefName = "media.autoplay.enabled.user-gestures-needed";
      const oldPrefValue = Services.prefs.getBoolPref(oldPrefName, true);
      const newPrefValue = oldPrefValue ? 0 : 1;
      Services.prefs.setIntPref("media.autoplay.blocking_policy", newPrefValue);
      Services.prefs.clearUserPref(oldPrefName);
    }

    if (currentUIVersion < 96) {
      const oldPrefName = "browser.urlbar.openViewOnFocus";
      const oldPrefValue = Services.prefs.getBoolPref(oldPrefName, true);
      Services.prefs.setBoolPref(
        "browser.urlbar.suggest.topsites",
        oldPrefValue
      );
      Services.prefs.clearUserPref(oldPrefName);
    }

    if (currentUIVersion < 97) {
      let userCustomizedWheelMax = Services.prefs.prefHasUserValue(
        "general.smoothScroll.mouseWheel.durationMaxMS"
      );
      let userCustomizedWheelMin = Services.prefs.prefHasUserValue(
        "general.smoothScroll.mouseWheel.durationMinMS"
      );

      if (!userCustomizedWheelMin && !userCustomizedWheelMax) {
        // If the user has an existing profile but hasn't customized the wheel
        // animation duration, they will now get the new default values. This
        // condition used to set a migrationPercent pref to 0, so that users
        // upgrading an older profile would gradually have their wheel animation
        // speed migrated to the new values. However, that "gradual migration"
        // was phased out by FF 86, so we don't need to set that pref anymore.
      } else if (userCustomizedWheelMin && !userCustomizedWheelMax) {
        // If they customized just one of the two, save the old value for the
        // other one as well, because the two values go hand-in-hand and we
        // don't want to move just one to a new value and leave the other one
        // at a customized value. In both of these cases, we leave the "migration
        // complete" percentage at 100, because they have customized this and
        // don't need any further migration.
        Services.prefs.setIntPref(
          "general.smoothScroll.mouseWheel.durationMaxMS",
          400
        );
      } else if (!userCustomizedWheelMin && userCustomizedWheelMax) {
        // Same as above case, but for the other pref.
        Services.prefs.setIntPref(
          "general.smoothScroll.mouseWheel.durationMinMS",
          200
        );
      } else {
        // The last remaining case is if they customized both values, in which
        // case also don't need to do anything; the user's customized values
        // will be retained and respected.
      }
    }

    if (currentUIVersion < 98) {
      Services.prefs.clearUserPref("browser.search.cohort");
    }

    if (currentUIVersion < 99) {
      Services.prefs.clearUserPref("security.tls.version.enable-deprecated");
    }

    if (currentUIVersion < 102) {
      // In Firefox 83, we moved to a dynamic button, so it needs to be removed
      // from default placement. This is done early enough that it doesn't
      // impact adding new managed bookmarks.
      const { CustomizableUI } = ChromeUtils.import(
        "resource:///modules/CustomizableUI.jsm"
      );
      CustomizableUI.removeWidgetFromArea("managed-bookmarks");
    }

    // We have to rerun these because we had to use 102 on beta.
    // They were 101 and 102 before.
    if (currentUIVersion < 103) {
      // Set a pref if the bookmarks toolbar was already visible,
      // so we can keep it visible when navigating away from newtab
      let bookmarksToolbarWasVisible =
        Services.xulStore.getValue(
          BROWSER_DOCURL,
          "PersonalToolbar",
          "collapsed"
        ) == "false";
      if (bookmarksToolbarWasVisible) {
        // Migrate the user to the "always visible" value. See firefox.js for
        // the other possible states.
        Services.prefs.setCharPref(
          "browser.toolbars.bookmarks.visibility",
          "always"
        );
      }
      Services.xulStore.removeValue(
        BROWSER_DOCURL,
        "PersonalToolbar",
        "collapsed"
      );

      Services.prefs.clearUserPref(
        "browser.livebookmarks.migrationAttemptsLeft"
      );
    }

    // For existing profiles, continue putting bookmarks in the
    // "other bookmarks" folder.
    if (currentUIVersion < 104) {
      Services.prefs.setCharPref(
        "browser.bookmarks.defaultLocation",
        "unfiled"
      );
    }

    // Renamed and flipped the logic of a pref to make its purpose more clear.
    if (currentUIVersion < 105) {
      const oldPrefName = "browser.urlbar.imeCompositionClosesPanel";
      const oldPrefValue = Services.prefs.getBoolPref(oldPrefName, true);
      Services.prefs.setBoolPref(
        "browser.urlbar.keepPanelOpenDuringImeComposition",
        !oldPrefValue
      );
      Services.prefs.clearUserPref(oldPrefName);
    }

    // Initialize the new browser.urlbar.showSuggestionsBeforeGeneral pref.
    if (currentUIVersion < 106) {
      UrlbarPrefs.initializeShowSearchSuggestionsFirstPref();
    }

    if (currentUIVersion < 107) {
      // Migrate old http URIs for mailto handlers to their https equivalents.
      // The handler service will do this. We need to wait with migrating
      // until the handler service has started up, so just set a pref here.
      const kPref = "browser.handlers.migrations";
      // We might have set up another migration further up. Create an array,
      // and drop empty strings resulting from the `split`:
      let migrations = Services.prefs
        .getCharPref(kPref, "")
        .split(",")
        .filter(x => !!x);
      migrations.push("secure-mail");
      Services.prefs.setCharPref(kPref, migrations.join(","));
    }

    if (currentUIVersion < 108) {
      // Migrate old ctrlTab pref to new ctrlTab pref
      let defaultValue = false;
      let oldPrefName = "browser.ctrlTab.recentlyUsedOrder";
      let oldPrefDefault = true;
      // Use old pref value if the user used Ctrl+Tab before, elsewise use new default value
      if (Services.prefs.getBoolPref("browser.engagement.ctrlTab.has-used")) {
        let newPrefValue = Services.prefs.getBoolPref(
          oldPrefName,
          oldPrefDefault
        );
        Services.prefs.setBoolPref(
          "browser.ctrlTab.sortByRecentlyUsed",
          newPrefValue
        );
      } else {
        Services.prefs.setBoolPref(
          "browser.ctrlTab.sortByRecentlyUsed",
          defaultValue
        );
      }
      Services.prefs.clearUserPref(oldPrefName);
    }

    if (currentUIVersion < 109) {
      // Migrate old pref to new pref
      if (
        Services.prefs.prefHasUserValue("signon.recipes.remoteRecipesEnabled")
      ) {
        // Fetch the previous value of signon.recipes.remoteRecipesEnabled and assign it to signon.recipes.remoteRecipes.enabled.
        Services.prefs.setBoolPref(
          "signon.recipes.remoteRecipes.enabled",
          Services.prefs.getBoolPref(
            "signon.recipes.remoteRecipesEnabled",
            true
          )
        );
        //Then clear user pref
        Services.prefs.clearUserPref("signon.recipes.remoteRecipesEnabled");
      }
    }

    if (currentUIVersion < 117) {
      // Update urlbar result groups for the following changes:
      // 110 (bug 1662167): Add INPUT_HISTORY group
      // 111 (bug 1677126): Add REMOTE_TABS group
      // 112 (bug 1712352): Add ABOUT_PAGES group
      // 113 (bug 1714409): Add HEURISTIC_ENGINE_ALIAS group
      // 114 (bug 1662172): Add HEURISTIC_BOOKMARK_KEYWORD group
      // 115 (bug 1713322): Move TAIL_SUGGESTION group and rename properties
      // 116 (bug 1717509): Remove HEURISTIC_UNIFIED_COMPLETE group
      // 117 (bug 1710518): Add GENERAL_PARENT group
      UrlbarPrefs.migrateResultGroups();
    }

    if (currentUIVersion < 119 && AppConstants.NIGHTLY_BUILD) {
      // Uninstall outdated monochromatic themes for the following UI versions:
      // 118: Uninstall prototype monochromatic purple theme.
      // 119 (bug 1732957): Uninstall themes with old IDs.
      const themeIdsToMigrate = [
        "firefox-monochromatic-purple@mozilla.org",
        "firefox-lush-soft@mozilla.org",
        "firefox-lush-balanced@mozilla.org",
        "firefox-lush-bold@mozilla.org",
        "firefox-abstract-soft@mozilla.org",
        "firefox-abstract-balanced@mozilla.org",
        "firefox-abstract-bold@mozilla.org",
        "firefox-elemental-soft@mozilla.org",
        "firefox-elemental-balanced@mozilla.org",
        "firefox-elemental-bold@mozilla.org",
        "firefox-cheers-soft@mozilla.org",
        "firefox-cheers-balanced@mozilla.org",
        "firefox-cheers-bold@mozilla.org",
        "firefox-graffiti-soft@mozilla.org",
        "firefox-graffiti-balanced@mozilla.org",
        "firefox-graffiti-bold@mozilla.org",
        "firefox-foto-soft@mozilla.org",
        "firefox-foto-balanced@mozilla.org",
        "firefox-foto-bold@mozilla.org",
      ];
      try {
        for (let id of themeIdsToMigrate) {
          AddonManager.getAddonByID(id).then(addon => {
            if (!addon) {
              // Either the addon wasn't installed, or the call to getAddonByID failed.
              return;
            }
            addon.uninstall().catch(Cu.reportError);
          }, Cu.reportError);
        }
      } catch (error) {
        Cu.reportError(
          "Could not access the AddonManager to upgrade the profile. This is most " +
            "likely because the upgrader is being run from an xpcshell test where " +
            "the AddonManager is not initialized."
        );
      }
    }

    if (currentUIVersion < 120) {
      // Migrate old titlebar bool pref to new int-based one.
      const oldPref = "browser.tabs.drawInTitlebar";
      const newPref = "browser.tabs.inTitlebar";
      if (Services.prefs.prefHasUserValue(oldPref)) {
        // We may have int prefs for builds between bug 1736518 and bug 1739539.
        const oldPrefType = Services.prefs.getPrefType(oldPref);
        if (oldPrefType == Services.prefs.PREF_BOOL) {
          Services.prefs.setIntPref(
            newPref,
            Services.prefs.getBoolPref(oldPref) ? 1 : 0
          );
        } else {
          Services.prefs.setIntPref(
            newPref,
            Services.prefs.getIntPref(oldPref)
          );
        }
        Services.prefs.clearUserPref(oldPref);
      }
    }

    if (currentUIVersion < 121) {
      // Migrate stored uris and convert them to use hashed keys
      this._migrateHashedKeysForXULStoreForDocument(BROWSER_DOCURL);
      this._migrateHashedKeysForXULStoreForDocument(
        "chrome://browser/content/places/bookmarksSidebar.xhtml"
      );
      this._migrateHashedKeysForXULStoreForDocument(
        "chrome://browser/content/places/historySidebar.xhtml"
      );
    }

    // Update the migration version.
    Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  },

  _showUpgradeDialog() {
    BrowserWindowTracker.getTopWindow().gDialogBox.open(
      "chrome://browser/content/upgradeDialog.html"
    );
  },

  async _maybeShowDefaultBrowserPrompt() {
    // Highest priority is the upgrade dialog, which can include a "primary
    // browser" request and is limited in various ways, e.g., major upgrades.
    const dialogVersion = 94;
    const dialogVersionPref = "browser.startup.upgradeDialog.version";
    const dialogReason = await (async () => {
      if (!BrowserHandler.majorUpgrade) {
        return "not-major";
      }
      const lastVersion = Services.prefs.getIntPref(dialogVersionPref, 0);
      if (lastVersion > dialogVersion) {
        return "newer-shown";
      }
      if (lastVersion === dialogVersion) {
        return "already-shown";
      }

      // Check the default branch as enterprise policies can set prefs there.
      const defaultPrefs = Services.prefs.getDefaultBranch("");
      if (
        !defaultPrefs.getBoolPref(
          "browser.messaging-system.whatsNewPanel.enabled",
          true
        )
      ) {
        return "no-whatsNew";
      }
      if (!defaultPrefs.getBoolPref("browser.aboutwelcome.enabled", true)) {
        return "no-welcome";
      }
      if (!Services.policies.isAllowed("postUpdateCustomPage")) {
        return "disallow-postUpdate";
      }

      return NimbusFeatures.upgradeDialog.isEnabled() ? "" : "disabled";
    })();

    // Record why the dialog is showing or not.
    Services.telemetry.setEventRecordingEnabled("upgrade_dialog", true);
    Services.telemetry.recordEvent(
      "upgrade_dialog",
      "trigger",
      "reason",
      dialogReason || "satisfied"
    );

    // Show the upgrade dialog if allowed and remember the version.
    if (!dialogReason) {
      Services.prefs.setIntPref(dialogVersionPref, dialogVersion);

      // Show Firefox Home behind the upgrade dialog to see theme changes.
      const { gBrowser } = BrowserWindowTracker.getTopWindow();
      gBrowser.selectedTab = gBrowser.addTrustedTab("about:home");
      this._showUpgradeDialog();
      return;
    }

    const willPrompt = await DefaultBrowserCheck.willCheckDefaultBrowser(
      /* isStartupCheck */ true
    );
    if (willPrompt) {
      let win = BrowserWindowTracker.getTopWindow();
      DefaultBrowserCheck.prompt(win);
    }
    await ASRouter.waitForInitialized;
    ASRouter.sendTriggerMessage({
      browser: BrowserWindowTracker.getTopWindow()?.gBrowser.selectedBrowser,
      // triggerId and triggerContext
      id: "defaultBrowserCheck",
      context: { willShowDefaultPrompt: willPrompt, source: "startup" },
    });
  },

  /**
   * Only show the infobar when canRestoreLastSession and the pref value == 1
   */
  async _maybeShowRestoreSessionInfoBar() {
    let count = Services.prefs.getIntPref(
      "browser.startup.couldRestoreSession.count",
      0
    );
    if (count < 0 || count >= 2) {
      return;
    }
    if (count == 0) {
      // We don't show the infobar right after the update which establishes this pref
      // Increment the counter so we can consider it next time
      Services.prefs.setIntPref(
        "browser.startup.couldRestoreSession.count",
        ++count
      );
      return;
    }

    const win = BrowserWindowTracker.getTopWindow();
    // We've restarted at least once; we will show the notification if possible.
    // We can't do that if there's no session to restore, or this is a private window.
    if (
      !SessionStore.canRestoreLastSession ||
      PrivateBrowsingUtils.isWindowPrivate(win)
    ) {
      return;
    }

    Services.prefs.setIntPref(
      "browser.startup.couldRestoreSession.count",
      ++count
    );

    const messageFragment = win.document.createDocumentFragment();
    const message = win.document.createElement("span");
    const icon = win.document.createElement("img");
    icon.src = "chrome://browser/skin/menu.svg";
    icon.setAttribute("data-l10n-name", "icon");
    icon.className = "inline-icon";
    message.appendChild(icon);
    messageFragment.appendChild(message);
    win.document.l10n.setAttributes(
      message,
      "restore-session-startup-suggestion-message"
    );

    const buttons = [
      {
        "l10n-id": "restore-session-startup-suggestion-button",
        primary: true,
        callback: () => {
          win.PanelUI.selectAndMarkItem([
            "appMenu-history-button",
            "appMenu-restoreSession",
          ]);
        },
      },
    ];

    const notifyBox = win.gBrowser.getNotificationBox();
    const notification = notifyBox.appendNotification(
      "startup-restore-session-suggestion",
      {
        label: messageFragment,
        priority: notifyBox.PRIORITY_INFO_MEDIUM,
      },
      buttons
    );
    // Don't allow it to be immediately hidden:
    notification.timeout = Date.now() + 3000;
  },

  /**
   * Open preferences even if there are no open windows.
   */
  _openPreferences(...args) {
    let chromeWindow = BrowserWindowTracker.getTopWindow();
    if (chromeWindow) {
      chromeWindow.openPreferences(...args);
      return;
    }

    if (Services.appShell.hiddenDOMWindow.openPreferences) {
      Services.appShell.hiddenDOMWindow.openPreferences(...args);
    }
  },

  _openURLInNewWindow(url) {
    let urlString = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    urlString.data = url;
    return new Promise(resolve => {
      let win = Services.ww.openWindow(
        null,
        AppConstants.BROWSER_CHROME_URL,
        "_blank",
        "chrome,all,dialog=no",
        urlString
      );
      win.addEventListener(
        "load",
        () => {
          resolve(win);
        },
        { once: true }
      );
    });
  },

  /**
   * Called as an observer when Sync's "display URIs" notification is fired.
   *
   * We open the received URIs in background tabs.
   */
  async _onDisplaySyncURIs(data) {
    try {
      // The payload is wrapped weirdly because of how Sync does notifications.
      const URIs = data.wrappedJSObject.object;

      // win can be null, but it's ok, we'll assign it later in openTab()
      let win = BrowserWindowTracker.getTopWindow({ private: false });

      const openTab = async URI => {
        let tab;
        if (!win) {
          win = await this._openURLInNewWindow(URI.uri);
          let tabs = win.gBrowser.tabs;
          tab = tabs[tabs.length - 1];
        } else {
          tab = win.gBrowser.addWebTab(URI.uri);
        }
        tab.setAttribute("attention", true);
        return tab;
      };

      const firstTab = await openTab(URIs[0]);
      await Promise.all(URIs.slice(1).map(URI => openTab(URI)));

      const deviceName = URIs[0].sender && URIs[0].sender.name;
      let title, body;
      const bundle = Services.strings.createBundle(
        "chrome://browser/locale/accounts.properties"
      );
      if (URIs.length == 1) {
        // Due to bug 1305895, tabs from iOS may not have device information, so
        // we have separate strings to handle those cases. (See Also
        // unnamedTabsArrivingNotificationNoDevice.body below)
        if (deviceName) {
          title = bundle.formatStringFromName(
            "tabArrivingNotificationWithDevice.title",
            [deviceName]
          );
        } else {
          title = bundle.GetStringFromName("tabArrivingNotification.title");
        }
        // Use the page URL as the body. We strip the fragment and query (after
        // the `?` and `#` respectively) to reduce size, and also format it the
        // same way that the url bar would.
        body = URIs[0].uri.replace(/([?#]).*$/, "$1");
        let wasTruncated = body.length < URIs[0].uri.length;
        body = BrowserUIUtils.trimURL(body);
        if (wasTruncated) {
          body = bundle.formatStringFromName(
            "singleTabArrivingWithTruncatedURL.body",
            [body]
          );
        }
      } else {
        title = bundle.GetStringFromName(
          "multipleTabsArrivingNotification.title"
        );
        const allKnownSender = URIs.every(URI => URI.sender != null);
        const allSameDevice =
          allKnownSender &&
          URIs.every(URI => URI.sender.id == URIs[0].sender.id);
        let tabArrivingBody;
        if (allSameDevice) {
          if (deviceName) {
            tabArrivingBody = "unnamedTabsArrivingNotification2.body";
          } else {
            tabArrivingBody = "unnamedTabsArrivingNotificationNoDevice.body";
          }
        } else {
          tabArrivingBody = "unnamedTabsArrivingNotificationMultiple2.body";
        }

        body = bundle.GetStringFromName(tabArrivingBody);
        body = PluralForm.get(URIs.length, body);
        body = body.replace("#1", URIs.length);
        body = body.replace("#2", deviceName);
      }

      const clickCallback = (obsSubject, obsTopic, obsData) => {
        if (obsTopic == "alertclickcallback") {
          win.gBrowser.selectedTab = firstTab;
        }
      };

      // Specify an icon because on Windows no icon is shown at the moment
      let imageURL;
      if (AppConstants.platform == "win") {
        imageURL = "chrome://branding/content/icon64.png";
      }
      this.AlertsService.showAlertNotification(
        imageURL,
        title,
        body,
        true,
        null,
        clickCallback
      );
    } catch (ex) {
      Cu.reportError("Error displaying tab(s) received by Sync: " + ex);
    }
  },

  async _onVerifyLoginNotification({ body, title, url }) {
    let tab;
    let imageURL;
    if (AppConstants.platform == "win") {
      imageURL = "chrome://branding/content/icon64.png";
    }
    let win = BrowserWindowTracker.getTopWindow({ private: false });
    if (!win) {
      win = await this._openURLInNewWindow(url);
      let tabs = win.gBrowser.tabs;
      tab = tabs[tabs.length - 1];
    } else {
      tab = win.gBrowser.addWebTab(url);
    }
    tab.setAttribute("attention", true);
    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      win.gBrowser.selectedTab = tab;
    };

    try {
      this.AlertsService.showAlertNotification(
        imageURL,
        title,
        body,
        true,
        null,
        clickCallback
      );
    } catch (ex) {
      Cu.reportError("Error notifying of a verify login event: " + ex);
    }
  },

  _onDeviceConnected(deviceName) {
    let accountsBundle = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
    let title = accountsBundle.GetStringFromName("deviceConnDisconnTitle");
    let body = accountsBundle.formatStringFromName(
      "otherDeviceConnectedBody" + (deviceName ? "" : ".noDeviceName"),
      [deviceName]
    );

    let clickCallback = async (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      let url = await FxAccounts.config.promiseManageDevicesURI(
        "device-connected-notification"
      );
      let win = BrowserWindowTracker.getTopWindow({ private: false });
      if (!win) {
        this._openURLInNewWindow(url);
      } else {
        win.gBrowser.addWebTab(url);
      }
    };

    try {
      this.AlertsService.showAlertNotification(
        null,
        title,
        body,
        true,
        null,
        clickCallback
      );
    } catch (ex) {
      Cu.reportError("Error notifying of a new Sync device: " + ex);
    }
  },

  _onDeviceDisconnected() {
    let bundle = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
    let title = bundle.GetStringFromName("deviceConnDisconnTitle");
    let body = bundle.GetStringFromName("thisDeviceDisconnectedBody");

    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      this._openPreferences("sync");
    };
    this.AlertsService.showAlertNotification(
      null,
      title,
      body,
      true,
      null,
      clickCallback
    );
  },

  _handleFlashHang() {
    ++this._flashHangCount;
    if (this._flashHangCount < 2) {
      return;
    }
    // protected mode only applies to win32
    if (Services.appinfo.XPCOMABI != "x86-msvc") {
      return;
    }

    if (
      Services.prefs.getBoolPref("dom.ipc.plugins.flash.disable-protected-mode")
    ) {
      return;
    }
    if (
      !Services.prefs.getBoolPref("browser.flash-protected-mode-flip.enable")
    ) {
      return;
    }
    if (Services.prefs.getBoolPref("browser.flash-protected-mode-flip.done")) {
      return;
    }
    Services.prefs.setBoolPref(
      "dom.ipc.plugins.flash.disable-protected-mode",
      true
    );
    Services.prefs.setBoolPref("browser.flash-protected-mode-flip.done", true);

    let win = BrowserWindowTracker.getTopWindow();
    if (!win) {
      return;
    }
    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let message = win.gNavigatorBundle.getFormattedString("flashHang.message", [
      productName,
    ]);
    let buttons = [
      {
        label: win.gNavigatorBundle.getString("flashHang.helpButton.label"),
        accessKey: win.gNavigatorBundle.getString(
          "flashHang.helpButton.accesskey"
        ),
        link:
          "https://support.mozilla.org/kb/flash-protected-mode-autodisabled",
      },
    ];

    // XXXndeakin is this notification still relevant?
    win.gNotificationBox.appendNotification(
      "flash-hang",
      {
        label: message,
        priority: win.gNotificationBox.PRIORITY_INFO_MEDIUM,
      },
      buttons
    );
  },

  _updateFxaBadges(win) {
    let fxaButton = win.document.getElementById("fxa-toolbar-menu-button");
    let badge = fxaButton?.querySelector(".toolbarbutton-badge");

    let state = UIState.get();
    if (
      state.status == UIState.STATUS_LOGIN_FAILED ||
      state.status == UIState.STATUS_NOT_VERIFIED
    ) {
      // If the fxa toolbar button is in the toolbox, we display the notification
      // on the fxa button instead of the app menu.
      let navToolbox = win.document.getElementById("navigator-toolbox");
      let isFxAButtonShown = navToolbox.contains(fxaButton);
      if (isFxAButtonShown) {
        state.status == UIState.STATUS_LOGIN_FAILED
          ? fxaButton?.setAttribute("badge-status", state.status)
          : badge?.classList.add("feature-callout");
      } else {
        AppMenuNotifications.showBadgeOnlyNotification(
          "fxa-needs-authentication"
        );
      }
    } else {
      fxaButton?.removeAttribute("badge-status");
      badge?.classList.remove("feature-callout");
      AppMenuNotifications.removeNotification("fxa-needs-authentication");
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};

var ContentBlockingCategoriesPrefs = {
  PREF_CB_CATEGORY: "browser.contentblocking.category",
  PREF_STRICT_DEF: "browser.contentblocking.features.strict",
  switchingCategory: false,

  setPrefExpectations() {
    // The prefs inside CATEGORY_PREFS are initial values.
    // If the pref remains null, then it will expect the default value.
    // The "standard" category is defined as expecting all 5 default values.
    this.CATEGORY_PREFS = {
      strict: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault": null,
        "privacy.partition.network_state.ocsp_cache": null,
      },
      standard: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault": null,
        "privacy.partition.network_state.ocsp_cache": null,
      },
    };
    let type = "strict";
    let rulesArray = Services.prefs
      .getStringPref(this.PREF_STRICT_DEF)
      .split(",");
    for (let item of rulesArray) {
      switch (item) {
        case "tp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.enabled"
          ] = true;
          break;
        case "-tp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.enabled"
          ] = false;
          break;
        case "tpPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.pbmode.enabled"
          ] = true;
          break;
        case "-tpPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.pbmode.enabled"
          ] = false;
          break;
        case "fp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.fingerprinting.enabled"
          ] = true;
          break;
        case "-fp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.fingerprinting.enabled"
          ] = false;
          break;
        case "cm":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.cryptomining.enabled"
          ] = true;
          break;
        case "-cm":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.cryptomining.enabled"
          ] = false;
          break;
        case "stp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.socialtracking.enabled"
          ] = true;
          break;
        case "-stp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.socialtracking.enabled"
          ] = false;
          break;
        case "lvl2":
          this.CATEGORY_PREFS[type][
            "privacy.annotate_channels.strict_list.enabled"
          ] = true;
          break;
        case "-lvl2":
          this.CATEGORY_PREFS[type][
            "privacy.annotate_channels.strict_list.enabled"
          ] = false;
          break;
        case "rp":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault"
          ] = true;
          break;
        case "-rp":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault"
          ] = false;
          break;
        case "ocsp":
          this.CATEGORY_PREFS[type][
            "privacy.partition.network_state.ocsp_cache"
          ] = true;
          break;
        case "-ocsp":
          this.CATEGORY_PREFS[type][
            "privacy.partition.network_state.ocsp_cache"
          ] = false;
          break;
        case "cookieBehavior0":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_ACCEPT;
          break;
        case "cookieBehavior1":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
          break;
        case "cookieBehavior2":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT;
          break;
        case "cookieBehavior3":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
          break;
        case "cookieBehavior4":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
          break;
        case "cookieBehavior5":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
          break;
        case "cookieBehaviorPBM0":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_ACCEPT;
          break;
        case "cookieBehaviorPBM1":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
          break;
        case "cookieBehaviorPBM2":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT;
          break;
        case "cookieBehaviorPBM3":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
          break;
        case "cookieBehaviorPBM4":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
          break;
        case "cookieBehaviorPBM5":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
          break;
        default:
          Cu.reportError(`Error: Unknown rule observed ${item}`);
      }
    }
  },

  /**
   * Checks if CB prefs match perfectly with one of our pre-defined categories.
   */
  prefsMatch(category) {
    // The category pref must be either unset, or match.
    if (
      Services.prefs.prefHasUserValue(this.PREF_CB_CATEGORY) &&
      Services.prefs.getStringPref(this.PREF_CB_CATEGORY) != category
    ) {
      return false;
    }
    for (let pref in this.CATEGORY_PREFS[category]) {
      let value = this.CATEGORY_PREFS[category][pref];
      if (value == null) {
        if (Services.prefs.prefHasUserValue(pref)) {
          return false;
        }
      } else {
        let prefType = Services.prefs.getPrefType(pref);
        if (
          (prefType == Services.prefs.PREF_BOOL &&
            Services.prefs.getBoolPref(pref) != value) ||
          (prefType == Services.prefs.PREF_INT &&
            Services.prefs.getIntPref(pref) != value) ||
          (prefType == Services.prefs.PREF_STRING &&
            Services.prefs.getStringPref(pref) != value)
        ) {
          return false;
        }
      }
    }
    return true;
  },

  matchCBCategory() {
    if (this.switchingCategory) {
      return;
    }
    // If PREF_CB_CATEGORY is not set match users to a Content Blocking category. Check if prefs fit
    // perfectly into strict or standard, otherwise match with custom. If PREF_CB_CATEGORY has previously been set,
    // a change of one of these prefs necessarily puts us in "custom".
    if (this.prefsMatch("standard")) {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "standard");
    } else if (this.prefsMatch("strict")) {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "strict");
    } else {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "custom");
    }

    // If there is a custom policy which changes a related pref, then put the user in custom so
    // they still have access to other content blocking prefs, and to keep our default definitions
    // from changing.
    let policy = Services.policies.getActivePolicies();
    if (policy && (policy.EnableTrackingProtection || policy.Cookies)) {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "custom");
    }
  },

  updateCBCategory() {
    if (
      this.switchingCategory ||
      !Services.prefs.prefHasUserValue(this.PREF_CB_CATEGORY)
    ) {
      return;
    }
    // Turn on switchingCategory flag, to ensure that when the individual prefs that change as a result
    // of the category change do not trigger yet another category change.
    this.switchingCategory = true;
    let value = Services.prefs.getStringPref(this.PREF_CB_CATEGORY);
    this.setPrefsToCategory(value);
    this.switchingCategory = false;
  },

  /**
   * Sets all user-exposed content blocking preferences to values that match the selected category.
   */
  setPrefsToCategory(category) {
    // Leave prefs as they were if we are switching to "custom" category.
    if (category == "custom") {
      return;
    }

    for (let pref in this.CATEGORY_PREFS[category]) {
      let value = this.CATEGORY_PREFS[category][pref];
      if (!Services.prefs.prefIsLocked(pref)) {
        if (value == null) {
          Services.prefs.clearUserPref(pref);
        } else {
          switch (Services.prefs.getPrefType(pref)) {
            case Services.prefs.PREF_BOOL:
              Services.prefs.setBoolPref(pref, value);
              break;
            case Services.prefs.PREF_INT:
              Services.prefs.setIntPref(pref, value);
              break;
            case Services.prefs.PREF_STRING:
              Services.prefs.setStringPref(pref, value);
              break;
          }
        }
      }
    }
  },
};

/**
 * ContentPermissionIntegration is responsible for showing the user
 * simple permission prompts when content requests additional
 * capabilities.
 *
 * While there are some built-in permission prompts, createPermissionPrompt
 * can also be overridden by system add-ons or tests to provide new ones.
 *
 * This override ability is provided by Integration.jsm. See
 * PermissionUI.jsm for an example of how to provide a new prompt
 * from an add-on.
 */
const ContentPermissionIntegration = {
  /**
   * Creates a PermissionPrompt for a given permission type and
   * nsIContentPermissionRequest.
   *
   * @param {string} type
   *        The type of the permission request from content. This normally
   *        matches the "type" field of an nsIContentPermissionType, but it
   *        can be something else if the permission does not use the
   *        nsIContentPermissionRequest model. Note that this type might also
   *        be different from the permission key used in the permissions
   *        database.
   *        Example: "geolocation"
   * @param {nsIContentPermissionRequest} request
   *        The request for a permission from content.
   * @return {PermissionPrompt} (see PermissionUI.jsm),
   *         or undefined if the type cannot be handled.
   */
  createPermissionPrompt(type, request) {
    switch (type) {
      case "geolocation": {
        return new PermissionUI.GeolocationPermissionPrompt(request);
      }
      case "xr": {
        return new PermissionUI.XRPermissionPrompt(request);
      }
      case "desktop-notification": {
        return new PermissionUI.DesktopNotificationPermissionPrompt(request);
      }
      case "persistent-storage": {
        return new PermissionUI.PersistentStoragePermissionPrompt(request);
      }
      case "midi": {
        return new PermissionUI.MIDIPermissionPrompt(request);
      }
      case "storage-access": {
        return new PermissionUI.StorageAccessPermissionPrompt(request);
      }
    }
    return undefined;
  },
};

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{d8903bf6-68d5-4e97-bcd1-e4d3012f721a}"),

  QueryInterface: ChromeUtils.generateQI(["nsIContentPermissionPrompt"]),

  /**
   * This implementation of nsIContentPermissionPrompt.prompt ensures
   * that there's only one nsIContentPermissionType in the request,
   * and that it's of type nsIContentPermissionType. Failing to
   * satisfy either of these conditions will result in this method
   * throwing NS_ERRORs. If the combined ContentPermissionIntegration
   * cannot construct a prompt for this particular request, an
   * NS_ERROR_FAILURE will be thrown.
   *
   * Any time an error is thrown, the nsIContentPermissionRequest is
   * cancelled automatically.
   *
   * @param {nsIContentPermissionRequest} request
   *        The request that we're to show a prompt for.
   */
  prompt(request) {
    if (request.element && request.element.fxrPermissionPrompt) {
      // For Firefox Reality on Desktop, switch to a different mechanism to
      // prompt the user since fewer permissions are available and since many
      // UI dependencies are not availabe.
      request.element.fxrPermissionPrompt(request);
      return;
    }

    let type;
    try {
      // Only allow exactly one permission request here.
      let types = request.types.QueryInterface(Ci.nsIArray);
      if (types.length != 1) {
        throw Components.Exception(
          "Expected an nsIContentPermissionRequest with only 1 type.",
          Cr.NS_ERROR_UNEXPECTED
        );
      }

      type = types.queryElementAt(0, Ci.nsIContentPermissionType).type;
      let combinedIntegration = Integration.contentPermission.getCombined(
        ContentPermissionIntegration
      );

      let permissionPrompt = combinedIntegration.createPermissionPrompt(
        type,
        request
      );
      if (!permissionPrompt) {
        throw Components.Exception(
          `Failed to handle permission of type ${type}`,
          Cr.NS_ERROR_FAILURE
        );
      }

      permissionPrompt.prompt();
    } catch (ex) {
      Cu.reportError(ex);
      request.cancel();
      throw ex;
    }

    let schemeHistogram = Services.telemetry.getKeyedHistogramById(
      "PERMISSION_REQUEST_ORIGIN_SCHEME"
    );
    let scheme = 0;
    try {
      if (request.principal.schemeIs("http")) {
        scheme = 1;
      } else if (request.principal.schemeIs("https")) {
        scheme = 2;
      }
    } catch (ex) {
      // If the request principal is not available at this point,
      // the request has likely been cancelled before being shown to the
      // user. We shouldn't record this request.
      if (ex.result != Cr.NS_ERROR_FAILURE) {
        Cu.reportError(ex);
      }
      return;
    }
    schemeHistogram.add(type, scheme);

    let userInputHistogram = Services.telemetry.getKeyedHistogramById(
      "PERMISSION_REQUEST_HANDLING_USER_INPUT"
    );
    userInputHistogram.add(
      type,
      request.hasValidTransientUserGestureActivation
    );
  },
};

var DefaultBrowserCheck = {
  async prompt(win) {
    const shellService = win.getShellService();
    const needPin = await shellService.doesAppNeedPin();

    win.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");
    win.MozXULElement.insertFTLIfNeeded(
      "browser/defaultBrowserNotification.ftl"
    );
    // Resolve the translations for the prompt elements and return only the
    // string values
    const pinMessage =
      AppConstants.platform == "macosx"
        ? "default-browser-prompt-message-pin-mac"
        : "default-browser-prompt-message-pin";
    let [promptTitle, promptMessage, askLabel, yesButton, notNowButton] = (
      await win.document.l10n.formatMessages([
        {
          id: needPin
            ? "default-browser-prompt-title-pin"
            : "default-browser-prompt-title-alt",
        },
        {
          id: needPin ? pinMessage : "default-browser-prompt-message-alt",
        },
        { id: "default-browser-prompt-checkbox-not-again-label" },
        {
          id: needPin
            ? "default-browser-prompt-button-primary-pin"
            : "default-browser-prompt-button-primary-alt",
        },
        { id: "default-browser-prompt-button-secondary" },
      ])
    ).map(({ value }) => value);

    let ps = Services.prompt;
    let buttonFlags =
      ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0 +
      ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_1 +
      ps.BUTTON_POS_0_DEFAULT;
    let rv = await ps.asyncConfirmEx(
      win.browsingContext,
      ps.MODAL_TYPE_INTERNAL_WINDOW,
      promptTitle,
      promptMessage,
      buttonFlags,
      yesButton,
      notNowButton,
      null,
      askLabel,
      false, // checkbox state
      { headerIconURL: "chrome://branding/content/icon32.png" }
    );
    let buttonNumClicked = rv.get("buttonNumClicked");
    let checkboxState = rv.get("checked");
    if (buttonNumClicked == 0) {
      shellService.setAsDefault();
      shellService.pinToTaskbar();
    } else if (checkboxState) {
      shellService.shouldCheckDefaultBrowser = false;
    }

    try {
      let resultEnum = buttonNumClicked * 2 + !checkboxState;
      Services.telemetry
        .getHistogramById("BROWSER_SET_DEFAULT_RESULT")
        .add(resultEnum);
    } catch (ex) {
      /* Don't break if Telemetry is acting up. */
    }
  },

  /**
   * Checks if the default browser check prompt will be shown.
   * @param {boolean} isStartupCheck
   *   If true, prefs will be set and telemetry will be recorded.
   * @returns {boolean} True if the default browser check prompt will be shown.
   */
  async willCheckDefaultBrowser(isStartupCheck) {
    let win = BrowserWindowTracker.getTopWindow();
    let shellService = win.getShellService();

    // Perform default browser checking.
    if (!shellService) {
      return false;
    }

    let shouldCheck =
      !AppConstants.DEBUG && shellService.shouldCheckDefaultBrowser;

    // Even if we shouldn't check the default browser, we still continue when
    // isStartupCheck = true to set prefs and telemetry.
    if (!shouldCheck && !isStartupCheck) {
      return false;
    }

    // Skip the "Set Default Browser" check during first-run or after the
    // browser has been run a few times.
    const skipDefaultBrowserCheck =
      Services.prefs.getBoolPref(
        "browser.shell.skipDefaultBrowserCheckOnFirstRun"
      ) &&
      !Services.prefs.getBoolPref(
        "browser.shell.didSkipDefaultBrowserCheckOnFirstRun"
      );

    let promptCount = Services.prefs.getIntPref(
      "browser.shell.defaultBrowserCheckCount",
      0
    );

    // If SessionStartup's state is not initialized, checking sessionType will set
    // its internal state to "do not restore".
    await SessionStartup.onceInitialized;
    let willRecoverSession =
      SessionStartup.sessionType == SessionStartup.RECOVER_SESSION;

    // Don't show the prompt if we're already the default browser.
    let isDefault = false;
    let isDefaultError = false;
    try {
      isDefault = shellService.isDefaultBrowser(isStartupCheck, false);
    } catch (ex) {
      isDefaultError = true;
    }

    if (isDefault && isStartupCheck) {
      let now = Math.floor(Date.now() / 1000).toString();
      Services.prefs.setCharPref(
        "browser.shell.mostRecentDateSetAsDefault",
        now
      );
    }

    let willPrompt = shouldCheck && !isDefault && !willRecoverSession;

    if (willPrompt) {
      if (skipDefaultBrowserCheck) {
        if (isStartupCheck) {
          Services.prefs.setBoolPref(
            "browser.shell.didSkipDefaultBrowserCheckOnFirstRun",
            true
          );
        }
        willPrompt = false;
      } else {
        promptCount++;
        if (isStartupCheck) {
          Services.prefs.setIntPref(
            "browser.shell.defaultBrowserCheckCount",
            promptCount
          );
        }
        if (!AppConstants.RELEASE_OR_BETA && promptCount > 3) {
          willPrompt = false;
        }
      }
    }

    if (isStartupCheck) {
      try {
        // Report default browser status on startup to telemetry
        // so we can track whether we are the default.
        Services.telemetry
          .getHistogramById("BROWSER_IS_USER_DEFAULT")
          .add(isDefault);
        Services.telemetry
          .getHistogramById("BROWSER_IS_USER_DEFAULT_ERROR")
          .add(isDefaultError);
        Services.telemetry
          .getHistogramById("BROWSER_SET_DEFAULT_ALWAYS_CHECK")
          .add(shouldCheck);
        Services.telemetry
          .getHistogramById("BROWSER_SET_DEFAULT_DIALOG_PROMPT_RAWCOUNT")
          .add(promptCount);
      } catch (ex) {
        /* Don't break the default prompt if telemetry is broken. */
      }
    }

    return willPrompt;
  },
};

/*
 * Prompts users who have an outdated JAWS screen reader informing
 * them they need to update JAWS or switch to esr. Can be removed
 * 12/31/2018.
 */
var JawsScreenReaderVersionCheck = {
  _prompted: false,

  init() {
    Services.obs.addObserver(this, "a11y-init-or-shutdown", true);
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  observe(subject, topic, data) {
    if (topic == "a11y-init-or-shutdown" && data == "1") {
      Services.tm.dispatchToMainThread(() => this._checkVersionAndPrompt());
    }
  },

  onWindowsRestored() {
    Services.tm.dispatchToMainThread(() => this._checkVersionAndPrompt());
  },

  _checkVersionAndPrompt() {
    // Make sure we only prompt for versions of JAWS we do not
    // support and never prompt if e10s is disabled or if we're on
    // nightly.
    if (
      !Services.appinfo.shouldBlockIncompatJaws ||
      !Services.appinfo.browserTabsRemoteAutostart ||
      AppConstants.NIGHTLY_BUILD
    ) {
      return;
    }

    let win = BrowserWindowTracker.getTopWindow();
    if (!win || !win.gBrowser || !win.gBrowser.selectedBrowser) {
      Services.console.logStringMessage(
        "Content access support for older versions of JAWS is disabled " +
          "due to compatibility issues with this version of Firefox."
      );
      this._prompted = false;
      return;
    }

    // Only prompt once per session
    if (this._prompted) {
      return;
    }
    this._prompted = true;

    let browser = win.gBrowser.selectedBrowser;

    // Prompt JAWS users to let them know they need to update
    let promptMessage = win.gNavigatorBundle.getFormattedString(
      "e10s.accessibilityNotice.jawsMessage",
      [gBrandBundle.GetStringFromName("brandShortName")]
    );
    let notification;
    // main option: an Ok button, keeps running with content accessibility disabled
    let mainAction = {
      label: win.gNavigatorBundle.getString(
        "e10s.accessibilityNotice.acceptButton.label"
      ),
      accessKey: win.gNavigatorBundle.getString(
        "e10s.accessibilityNotice.acceptButton.accesskey"
      ),
      callback() {
        // If the user invoked the button option remove the notification,
        // otherwise keep the alert icon around in the address bar.
        notification.remove();
      },
    };
    let options = {
      popupIconURL: "chrome://browser/skin/e10s-64@2x.png",
      persistWhileVisible: true,
      persistent: true,
      persistence: 100,
    };

    notification = win.PopupNotifications.show(
      browser,
      "e10s_enabled_with_incompat_jaws",
      promptMessage,
      null,
      mainAction,
      null,
      options
    );
  },
};

/**
 * AboutHomeStartupCache is responsible for reading and writing the
 * initial about:home document from the HTTP cache as a startup
 * performance optimization. It only works when the "privileged about
 * content process" is enabled and when ENABLED_PREF is set to true.
 *
 * See https://firefox-source-docs.mozilla.org/browser/components/newtab/docs/v2-system-addon/about_home_startup_cache.html
 * for further details.
 */
var AboutHomeStartupCache = {
  ABOUT_HOME_URI_STRING: "about:home",
  SCRIPT_EXTENSION: "script",
  ENABLED_PREF: "browser.startup.homepage.abouthome_cache.enabled",
  PRELOADED_NEWTAB_PREF: "browser.newtab.preload",
  LOG_LEVEL_PREF: "browser.startup.homepage.abouthome_cache.loglevel",

  // It's possible that the layout of about:home will change such that
  // we want to invalidate any pre-existing caches. We do this by setting
  // this meta key in the nsICacheEntry for the page.
  //
  // The version is currently set to the build ID, meaning that the cache
  // is invalidated after every upgrade (like the main startup cache).
  CACHE_VERSION_META_KEY: "version",

  LOG_NAME: "AboutHomeStartupCache",

  // These messages are used to request the "privileged about content process"
  // to create the cached document, and then to receive that document.
  CACHE_REQUEST_MESSAGE: "AboutHomeStartupCache:CacheRequest",
  CACHE_RESPONSE_MESSAGE: "AboutHomeStartupCache:CacheResponse",
  CACHE_USAGE_RESULT_MESSAGE: "AboutHomeStartupCache:UsageResult",

  // When a "privileged about content process" is launched, this message is
  // sent to give it some nsIInputStream's for the about:home document they
  // should load.
  SEND_STREAMS_MESSAGE: "AboutHomeStartupCache:InputStreams",

  // This time in ms is used to debounce messages that are broadcast to
  // all about:newtab's, or the preloaded about:newtab. We use those
  // messages as a signal that it's likely time to refresh the cache.
  CACHE_DEBOUNCE_RATE_MS: 5000,

  // This is how long we'll block the AsyncShutdown while waiting for
  // the cache to write. If we fail to write within that time, we will
  // allow the shutdown to proceed.
  SHUTDOWN_CACHE_WRITE_TIMEOUT_MS: 1000,

  // The following values are as possible values for the
  // browser.startup.abouthome_cache_result scalar. Keep these in sync with the
  // scalar definition in Scalars.yaml. See setDeferredResult for more
  // information.
  CACHE_RESULT_SCALARS: {
    UNSET: 0,
    DOES_NOT_EXIST: 1,
    CORRUPT_PAGE: 2,
    CORRUPT_SCRIPT: 3,
    INVALIDATED: 4,
    LATE: 5,
    VALID_AND_USED: 6,
    DISABLED: 7,
    NOT_LOADING_ABOUTHOME: 8,
    PRELOADING_DISABLED: 9,
  },

  // This will be set to one of the values of CACHE_RESULT_SCALARS
  // once it is determined which result best suits what occurred.
  _cacheDeferredResultScalar: -1,

  // A reference to the nsICacheEntry to read from and write to.
  _cacheEntry: null,

  // These nsIPipe's are sent down to the "privileged about content process"
  // immediately after the process launches. This allows us to race the loading
  // of the cache entry in the parent process with the load of the about:home
  // page in the content process, since we'll connect the InputStream's to
  // the pipes as soon as the nsICacheEntry is available.
  //
  // The page pipe is for the HTML markup for the page.
  _pagePipe: null,
  // The script pipe is for the JavaScript that the HTML markup loads
  // to set its internal state.
  _scriptPipe: null,
  _cacheDeferred: null,

  _enabled: false,
  _initted: false,
  _hasWrittenThisSession: false,
  _finalized: false,
  _firstPrivilegedProcessCreated: false,

  init() {
    if (this._initted) {
      throw new Error("AboutHomeStartupCache already initted.");
    }

    this.setDeferredResult(this.CACHE_RESULT_SCALARS.UNSET);

    this._enabled = NimbusFeatures.abouthomecache.isEnabled();

    if (!this._enabled) {
      this.recordResult(this.CACHE_RESULT_SCALARS.DISABLED);
      return;
    }

    this.log = Log.repository.getLogger(this.LOG_NAME);
    this.log.manageLevelFromPref(this.LOG_LEVEL_PREF);
    this._appender = new Log.ConsoleAppender(new Log.BasicFormatter());
    this.log.addAppender(this._appender);

    this.log.trace("Initting.");

    // If the user is not configured to load about:home at startup, then
    // let's not bother with the cache - loading it needlessly is more likely
    // to hinder what we're actually trying to load.
    let willLoadAboutHome =
      !HomePage.overridden &&
      Services.prefs.getIntPref("browser.startup.page") === 1;

    if (!willLoadAboutHome) {
      this.log.trace("Not configured to load about:home by default.");
      this.recordResult(this.CACHE_RESULT_SCALARS.NOT_LOADING_ABOUTHOME);
      return;
    }

    if (!Services.prefs.getBoolPref(this.PRELOADED_NEWTAB_PREF, false)) {
      this.log.trace("Preloaded about:newtab disabled.");
      this.recordResult(this.CACHE_RESULT_SCALARS.PRELOADING_DISABLED);
      return;
    }

    Services.obs.addObserver(this, "ipc:content-created");
    Services.obs.addObserver(this, "process-type-set");
    Services.obs.addObserver(this, "ipc:content-shutdown");
    Services.obs.addObserver(this, "intl:app-locales-changed");

    this.log.trace("Constructing pipes.");
    this._pagePipe = this.makePipe();
    this._scriptPipe = this.makePipe();

    this._cacheEntryPromise = new Promise(resolve => {
      this._cacheEntryResolver = resolve;
    });

    let lci = Services.loadContextInfo.default;
    let storage = Services.cache2.diskCacheStorage(lci);
    try {
      storage.asyncOpenURI(
        this.aboutHomeURI,
        "",
        Ci.nsICacheStorage.OPEN_PRIORITY,
        this
      );
    } catch (e) {
      this.log.error("Failed to open about:home cache entry", e);
    }

    this._cacheTask = new DeferredTask(async () => {
      await this.cacheNow();
    }, this.CACHE_DEBOUNCE_RATE_MS);

    AsyncShutdown.quitApplicationGranted.addBlocker(
      "AboutHomeStartupCache: Writing cache",
      async () => {
        await this.onShutdown();
      },
      () => this._cacheProgress
    );

    this._cacheDeferred = null;
    this._initted = true;
    this.log.trace("Initialized.");
  },

  get initted() {
    return this._initted;
  },

  uninit() {
    if (!this._enabled) {
      return;
    }

    try {
      Services.obs.removeObserver(this, "ipc:content-created");
      Services.obs.removeObserver(this, "process-type-set");
      Services.obs.removeObserver(this, "ipc:content-shutdown");
      Services.obs.removeObserver(this, "intl:app-locales-changed");
    } catch (e) {
      // If we failed to initialize and register for these observer
      // notifications, then attempting to remove them will throw.
      // It's fine to ignore that case on shutdown.
    }

    if (this._cacheTask) {
      this._cacheTask.disarm();
      this._cacheTask = null;
    }

    this._pagePipe = null;
    this._scriptPipe = null;
    this._initted = false;
    this._cacheEntry = null;
    this._hasWrittenThisSession = false;
    this._cacheEntryPromise = null;
    this._cacheEntryResolver = null;
    this._cacheDeferredResultScalar = -1;

    if (this.log) {
      this.log.trace("Uninitialized.");
      this.log.removeAppender(this._appender);
      this.log = null;
    }

    this._procManager = null;
    this._procManagerID = null;
    this._appender = null;
    this._cacheDeferred = null;
    this._finalized = false;
    this._firstPrivilegedProcessCreated = false;
  },

  _aboutHomeURI: null,

  get aboutHomeURI() {
    if (this._aboutHomeURI) {
      return this._aboutHomeURI;
    }

    this._aboutHomeURI = Services.io.newURI(this.ABOUT_HOME_URI_STRING);
    return this._aboutHomeURI;
  },

  // For the AsyncShutdown blocker, this is used to populate the progress
  // value.
  _cacheProgress: "Not yet begun",

  /**
   * Called by the AsyncShutdown blocker on quit-application-granted
   * to potentially flush the most recent cache to disk. If one was
   * never written during the session, one is generated and written
   * before the async function resolves.
   *
   * @param withTimeout (boolean)
   *   Whether or not the timeout mechanism should be used. Defaults
   *   to true.
   * @returns Promise
   * @resolves boolean
   *   If a cache has never been written, or a cache write is in
   *   progress, resolves true when the cache has been written. Also
   *   resolves to true if a cache didn't need to be written.
   *
   *   Resolves to false if a cache write unexpectedly timed out.
   */
  async onShutdown(withTimeout = true) {
    // If we never wrote this session, arm the task so that the next
    // step can finalize.
    if (!this._hasWrittenThisSession) {
      this.log.trace("Never wrote a cache this session. Arming cache task.");
      this._cacheTask.arm();
    }

    Services.telemetry.scalarSet(
      "browser.startup.abouthome_cache_shutdownwrite",
      this._cacheTask.isArmed
    );

    if (this._cacheTask.isArmed) {
      this.log.trace("Finalizing cache task on shutdown");
      this._finalized = true;

      // To avoid hanging shutdowns, we'll ensure that we wait a maximum of
      // SHUTDOWN_CACHE_WRITE_TIMEOUT_MS millseconds before giving up.
      let { setTimeout, clearTimeout } = ChromeUtils.import(
        "resource://gre/modules/Timer.jsm"
      );

      const TIMED_OUT = Symbol();
      let timeoutID = 0;

      let timeoutPromise = new Promise(resolve => {
        timeoutID = setTimeout(
          () => resolve(TIMED_OUT),
          this.SHUTDOWN_CACHE_WRITE_TIMEOUT_MS
        );
      });

      let promises = [this._cacheTask.finalize()];
      if (withTimeout) {
        this.log.trace("Using timeout mechanism.");
        promises.push(timeoutPromise);
      } else {
        this.log.trace("Skipping timeout mechanism.");
      }

      let result = await Promise.race(promises);
      this.log.trace("Done blocking shutdown.");
      clearTimeout(timeoutID);
      if (result === TIMED_OUT) {
        this.log.error("Timed out getting cache streams. Skipping cache task.");
        return false;
      }
    }
    this.log.trace("onShutdown is exiting");
    return true;
  },

  /**
   * Called by the _cacheTask DeferredTask to actually do the work of
   * caching the about:home document.
   *
   * @returns Promise
   * @resolves undefined
   *   Resolves when a fresh version of the cache has been written.
   */
  async cacheNow() {
    this.log.trace("Caching now.");
    this._cacheProgress = "Getting cache streams";

    let { pageInputStream, scriptInputStream } = await this.requestCache();

    if (!pageInputStream || !scriptInputStream) {
      this.log.trace("Failed to get cache streams.");
      this._cacheProgress = "Failed to get streams";
      return;
    }

    this.log.trace("Got cache streams.");

    this._cacheProgress = "Writing to cache";

    try {
      this.log.trace("Populating cache.");
      await this.populateCache(pageInputStream, scriptInputStream);
    } catch (e) {
      this._cacheProgress = "Failed to populate cache";
      this.log.error("Populating the cache failed: ", e);
      return;
    }

    this._cacheProgress = "Done";
    this.log.trace("Done writing to cache.");
    this._hasWrittenThisSession = true;
  },

  /**
   * Requests the cached document streams from the "privileged about content
   * process".
   *
   * @returns Promise
   * @resolves Object
   *   Resolves with an Object with the following properties:
   *
   *   pageInputStream (nsIInputStream)
   *     The page content to write to the cache, or null if request the streams
   *     failed.
   *
   *   scriptInputStream (nsIInputStream)
   *     The script content to write to the cache, or null if request the streams
   *     failed.
   */
  requestCache() {
    this.log.trace("Parent is requesting Activity Stream state object.");
    if (!this._procManager) {
      this.log.error("requestCache called with no _procManager!");
      return { pageInputStream: null, scriptInputStream: null };
    }

    if (this._procManager.remoteType != E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE) {
      this.log.error("Somehow got the wrong process type.");
      return { pageInputStream: null, scriptInputStream: null };
    }

    let state = AboutNewTab.activityStream.store.getState();
    return new Promise(resolve => {
      this._cacheDeferred = resolve;
      this.log.trace("Parent is requesting cache streams.");
      this._procManager.sendAsyncMessage(this.CACHE_REQUEST_MESSAGE, { state });
    });
  },

  /**
   * Helper function that returns a newly constructed nsIPipe instance.
   *
   * @return nsIPipe
   */
  makePipe() {
    let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
    pipe.init(
      true /* non-blocking input */,
      true /* non-blocking output */,
      0 /* segment size */,
      0 /* max segments */
    );
    return pipe;
  },

  get pagePipe() {
    return this._pagePipe;
  },

  get scriptPipe() {
    return this._scriptPipe;
  },

  /**
   * Called when the nsICacheEntry has been accessed. If the nsICacheEntry
   * has content that we want to send down to the "privileged about content
   * process", then we connect that content to the nsIPipe's that may or
   * may not have already been sent down to the process.
   *
   * In the event that the nsICacheEntry doesn't contain anything usable,
   * the nsInputStreams on the nsIPipe's are closed.
   */
  connectToPipes() {
    this.log.trace(`Connecting nsICacheEntry to pipes.`);

    // If the cache doesn't yet exist, we'll know because the version metadata
    // won't exist yet.
    let version;
    try {
      this.log.trace("");
      version = this._cacheEntry.getMetaDataElement(
        this.CACHE_VERSION_META_KEY
      );
    } catch (e) {
      if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
        this.log.debug("Cache meta data does not exist. Closing streams.");
        this.pagePipe.outputStream.close();
        this.scriptPipe.outputStream.close();
        this.setDeferredResult(this.CACHE_RESULT_SCALARS.DOES_NOT_EXIST);
        return;
      }

      throw e;
    }

    this.log.info("Version retrieved is", version);

    if (version != Services.appinfo.appBuildID) {
      this.log.info("Version does not match! Dooming and closing streams.\n");
      // This cache is no good - doom it, and prepare for a new one.
      this.clearCache();
      this.pagePipe.outputStream.close();
      this.scriptPipe.outputStream.close();
      this.setDeferredResult(this.CACHE_RESULT_SCALARS.INVALIDATED);
      return;
    }

    let cachePageInputStream;

    try {
      cachePageInputStream = this._cacheEntry.openInputStream(0);
    } catch (e) {
      this.log.error("Failed to open main input stream for cache entry", e);
      this.pagePipe.outputStream.close();
      this.scriptPipe.outputStream.close();
      this.setDeferredResult(this.CACHE_RESULT_SCALARS.CORRUPT_PAGE);
      return;
    }

    this.log.trace("Connecting page stream to pipe.");
    NetUtil.asyncCopy(cachePageInputStream, this.pagePipe.outputStream, () => {
      this.log.info("Page stream connected to pipe.");
    });

    let cacheScriptInputStream;
    try {
      this.log.trace("Connecting script stream to pipe.");
      cacheScriptInputStream = this._cacheEntry.openAlternativeInputStream(
        "script"
      );
      NetUtil.asyncCopy(
        cacheScriptInputStream,
        this.scriptPipe.outputStream,
        () => {
          this.log.info("Script stream connected to pipe.");
        }
      );
    } catch (e) {
      if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
        // For some reason, the script was not available. We'll close the pipe
        // without sending anything into it. The privileged about content process
        // will notice that there's nothing available in the pipe, and fall back
        // to dynamically generating the page.
        this.log.error("Script stream not available! Closing pipe.");
        this.scriptPipe.outputStream.close();
        this.setDeferredResult(this.CACHE_RESULT_SCALARS.CORRUPT_SCRIPT);
      } else {
        throw e;
      }
    }

    this.setDeferredResult(this.CACHE_RESULT_SCALARS.VALID_AND_USED);
    this.log.trace("Streams connected to pipes.");
  },

  /**
   * Called when we have received a the cache values from the "privileged
   * about content process". The page and script streams are written to
   * the nsICacheEntry.
   *
   * This writing is asynchronous, and if a write happens to already be
   * underway when this function is called, that latter call will be
   * ignored.
   *
   * @param pageInputStream (nsIInputStream)
   *   A stream containing the HTML markup to be saved to the cache.
   * @param scriptInputStream (nsIInputStream)
   *   A stream containing the JS hydration script to be saved to the cache.
   * @returns Promise
   * @resolves undefined
   *   When the cache has been successfully written to.
   * @rejects Error
   *   Rejects with a JS Error if writing any part of the cache happens to
   *   fail.
   */
  async populateCache(pageInputStream, scriptInputStream) {
    await this.ensureCacheEntry();

    await new Promise((resolve, reject) => {
      // Doom the old cache entry, so we can start writing to a new one.
      this.log.trace("Populating the cache. Dooming old entry.");
      this.clearCache();

      this.log.trace("Opening the page output stream.");
      let pageOutputStream;
      try {
        pageOutputStream = this._cacheEntry.openOutputStream(0, -1);
      } catch (e) {
        reject(e);
        return;
      }

      this.log.info("Writing the page cache.");
      NetUtil.asyncCopy(pageInputStream, pageOutputStream, pageResult => {
        if (!Components.isSuccessCode(pageResult)) {
          this.log.error("Failed to write page. Result: " + pageResult);
          reject(new Error(pageResult));
          return;
        }

        this.log.trace(
          "Writing the page data is complete. Now opening the " +
            "script output stream."
        );

        let scriptOutputStream;
        try {
          scriptOutputStream = this._cacheEntry.openAlternativeOutputStream(
            "script",
            -1
          );
        } catch (e) {
          reject(e);
          return;
        }

        this.log.info("Writing the script cache.");
        NetUtil.asyncCopy(
          scriptInputStream,
          scriptOutputStream,
          scriptResult => {
            if (!Components.isSuccessCode(scriptResult)) {
              this.log.error("Failed to write script. Result: " + scriptResult);
              reject(new Error(scriptResult));
              return;
            }

            this.log.trace(
              "Writing the script cache is done. Setting version."
            );
            try {
              this._cacheEntry.setMetaDataElement(
                "version",
                Services.appinfo.appBuildID
              );
            } catch (e) {
              this.log.error("Failed to write version.");
              reject(e);
              return;
            }
            this.log.trace(`Version is set to ${Services.appinfo.appBuildID}.`);
            this.log.info("Caching of page and script is done.");
            resolve();
          }
        );
      });
    });

    this.log.trace("populateCache has finished.");
  },

  /**
   * Returns a Promise that resolves once the nsICacheEntry for the cache
   * is available to write to and read from.
   *
   * @returns Promise
   * @resolves nsICacheEntry
   *   Once the cache entry has become available.
   * @rejects String
   *   Rejects with an error message if getting the cache entry is attempted
   *   before the AboutHomeStartupCache component has been initialized.
   */
  ensureCacheEntry() {
    if (!this._initted) {
      return Promise.reject(
        "Cannot ensureCacheEntry - AboutHomeStartupCache is not initted"
      );
    }

    return this._cacheEntryPromise;
  },

  /**
   * Clears the contents of the cache.
   */
  clearCache() {
    this.log.trace("Clearing the cache.");
    this._cacheEntry = this._cacheEntry.recreate();
    this._cacheEntryPromise = new Promise(resolve => {
      resolve(this._cacheEntry);
    });
    this._hasWrittenThisSession = false;
  },

  /**
   * Called when a content process is created. If this is the "privileged
   * about content process", then the cache streams will be sent to it.
   *
   * @param childID (Number)
   *   The unique ID for the content process that was created, as passed by
   *   ipc:content-created.
   * @param procManager (ProcessMessageManager)
   *   The ProcessMessageManager for the created content process.
   * @param processParent
   *   The nsIDOMProcessParent for the tab.
   */
  onContentProcessCreated(childID, procManager, processParent) {
    if (procManager.remoteType == E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE) {
      if (this._finalized) {
        this.log.trace(
          "Ignoring privileged about content process launch after finalization."
        );
        return;
      }

      if (this._firstPrivilegedProcessCreated) {
        this.log.trace(
          "Ignoring non-first privileged about content processes."
        );
        return;
      }

      this.log.trace(
        `A privileged about content process is launching with ID ${childID}.`
      );

      this.log.info("Sending input streams down to content process.");
      let actor = processParent.getActor("BrowserProcess");
      actor.sendAsyncMessage(this.SEND_STREAMS_MESSAGE, {
        pageInputStream: this.pagePipe.inputStream,
        scriptInputStream: this.scriptPipe.inputStream,
      });

      procManager.addMessageListener(this.CACHE_RESPONSE_MESSAGE, this);
      procManager.addMessageListener(this.CACHE_USAGE_RESULT_MESSAGE, this);
      this._procManager = procManager;
      this._procManagerID = childID;
      this._firstPrivilegedProcessCreated = true;
    }
  },

  /**
   * Called when a content process is destroyed. Either it shut down normally,
   * or it crashed. If this is the "privileged about content process", then some
   * internal state is cleared.
   *
   * @param childID (Number)
   *   The unique ID for the content process that was created, as passed by
   *   ipc:content-shutdown.
   */
  onContentProcessShutdown(childID) {
    this.log.info(`Content process shutdown: ${childID}`);
    if (this._procManagerID == childID) {
      this.log.info("It was the current privileged about process.");
      if (this._cacheDeferred) {
        this.log.error(
          "A privileged about content process shut down while cache streams " +
            "were still en route."
        );
        // The crash occurred while we were waiting on cache input streams to
        // be returned to us. Resolve with null streams instead.
        this._cacheDeferred({ pageInputStream: null, scriptInputStream: null });
        this._cacheDeferred = null;
      }

      this._procManager.removeMessageListener(
        this.CACHE_RESPONSE_MESSAGE,
        this
      );
      this._procManager.removeMessageListener(
        this.CACHE_USAGE_RESULT_MESSAGE,
        this
      );
      this._procManager = null;
      this._procManagerID = null;
    }
  },

  /**
   * Called externally by ActivityStreamMessageChannel anytime
   * a message is broadcast to all about:newtabs, or sent to the
   * preloaded about:newtab. This is used to determine if we need
   * to refresh the cache.
   */
  onPreloadedNewTabMessage() {
    if (!this._initted || !this._enabled) {
      return;
    }

    if (this._finalized) {
      this.log.trace("Ignoring preloaded newtab update after finalization.");
      return;
    }

    this.log.trace("Preloaded about:newtab was updated.");

    this._cacheTask.disarm();
    this._cacheTask.arm();
  },

  /**
   * Stores the CACHE_RESULT_SCALARS value that most accurately represents
   * the current notion of how the cache has operated so far. It is stored
   * temporarily like this because we need to hear from the privileged
   * about content process to hear whether or not retrieving the cache
   * actually worked on that end. The success state reported back from
   * the privileged about content process will be compared against the
   * deferred result scalar to compute what will be recorded to
   * Telemetry.
   *
   * Note that this value will only be recorded if its value is GREATER
   * than the currently recorded value. This is because it's possible for
   * certain functions that record results to re-enter - but we want to record
   * the _first_ condition that caused the cache to not be read from.
   *
   * @param result (Number)
   *   One of the CACHE_RESULT_SCALARS values. If this value is less than
   *   the currently recorded value, it is ignored.
   */
  setDeferredResult(result) {
    if (this._cacheDeferredResultScalar < result) {
      this._cacheDeferredResultScalar = result;
    }
  },

  /**
   * Records the final result of how the cache operated for the user
   * during this session to Telemetry.
   */
  recordResult(result) {
    // Note: this can be called very early on in the lifetime of
    // AboutHomeStartupCache, so things like this.log might not exist yet.
    Services.telemetry.scalarSet(
      "browser.startup.abouthome_cache_result",
      result
    );
  },

  /**
   * Called when the parent process receives a message from the privileged
   * about content process saying whether or not reading from the cache
   * was successful.
   *
   * @param success (boolean)
   *   True if reading from the cache succeeded.
   */
  onUsageResult(success) {
    this.log.trace(`Received usage result. Success = ${success}`);
    if (success) {
      if (
        this._cacheDeferredResultScalar !=
        this.CACHE_RESULT_SCALARS.VALID_AND_USED
      ) {
        this.log.error(
          "Somehow got a success result despite having never " +
            "successfully sent down the cache streams"
        );
        this.recordResult(this._cacheDeferredResultScalar);
      } else {
        this.recordResult(this.CACHE_RESULT_SCALARS.VALID_AND_USED);
      }

      return;
    }

    if (
      this._cacheDeferredResultScalar ==
      this.CACHE_RESULT_SCALARS.VALID_AND_USED
    ) {
      // We failed to read from the cache despite having successfully
      // sent it down to the content process. We presume then that the
      // streams just didn't provide any bytes in time.
      this.recordResult(this.CACHE_RESULT_SCALARS.LATE);
    } else {
      // We failed to read the cache, but already knew why. We can
      // now record that value.
      this.recordResult(this._cacheDeferredResultScalar);
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsICacheEntryOpenallback",
    "nsIObserver",
  ]),

  /** MessageListener **/

  receiveMessage(message) {
    // Only the privileged about content process can write to the cache.
    if (message.target.remoteType != E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE) {
      this.log.error(
        "Received a message from a non-privileged content process!"
      );
      return;
    }

    switch (message.name) {
      case this.CACHE_RESPONSE_MESSAGE: {
        this.log.trace("Parent received cache streams.");
        if (!this._cacheDeferred) {
          this.log.error("Parent doesn't have _cacheDeferred set up!");
          return;
        }

        this._cacheDeferred(message.data);
        this._cacheDeferred = null;
        break;
      }
      case this.CACHE_USAGE_RESULT_MESSAGE: {
        this.onUsageResult(message.data.success);
        break;
      }
    }
  },

  /** nsIObserver **/

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "intl:app-locales-changed": {
        this.clearCache();
        break;
      }
      case "process-type-set":
      // Intentional fall-through
      case "ipc:content-created": {
        let childID = aData;
        let procManager = aSubject
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIMessageSender);
        let pp = aSubject.QueryInterface(Ci.nsIDOMProcessParent);
        this.onContentProcessCreated(childID, procManager, pp);
        break;
      }

      case "ipc:content-shutdown": {
        let childID = aData;
        this.onContentProcessShutdown(childID);
        break;
      }
    }
  },

  /** nsICacheEntryOpenCallback **/

  onCacheEntryCheck(aEntry) {
    return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
  },

  onCacheEntryAvailable(aEntry, aNew, aResult) {
    this.log.trace("Cache entry is available.");

    this._cacheEntry = aEntry;
    this.connectToPipes();
    this._cacheEntryResolver(this._cacheEntry);
  },
};
