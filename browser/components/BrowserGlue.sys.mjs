/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
  AWToolbarButton: "resource:///modules/aboutwelcome/AWToolbarUtils.sys.mjs",
  ASRouter: "resource:///modules/asrouter/ASRouter.sys.mjs",
  ASRouterDefaultConfig:
    "resource:///modules/asrouter/ASRouterDefaultConfig.sys.mjs",
  ASRouterNewTabHook: "resource:///modules/asrouter/ASRouterNewTabHook.sys.mjs",
  ActorManagerParent: "resource://gre/modules/ActorManagerParent.sys.mjs",
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.sys.mjs",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  BackupService: "resource:///modules/backup/BackupService.sys.mjs",
  Blocklist: "resource://gre/modules/Blocklist.sys.mjs",
  BookmarkHTMLUtils: "resource://gre/modules/BookmarkHTMLUtils.sys.mjs",
  BookmarkJSONUtils: "resource://gre/modules/BookmarkJSONUtils.sys.mjs",
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.sys.mjs",
  BrowserUIUtils: "resource:///modules/BrowserUIUtils.sys.mjs",
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  BuiltInThemes: "resource:///modules/BuiltInThemes.sys.mjs",
  ContentRelevancyManager:
    "resource://gre/modules/ContentRelevancyManager.sys.mjs",
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
  DAPTelemetrySender: "resource://gre/modules/DAPTelemetrySender.sys.mjs",
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  Discovery: "resource:///modules/Discovery.sys.mjs",
  DoHController: "resource:///modules/DoHController.sys.mjs",
  DownloadsViewableInternally:
    "resource:///modules/DownloadsViewableInternally.sys.mjs",
  E10SUtils: "resource://gre/modules/E10SUtils.sys.mjs",
  ExtensionsUI: "resource:///modules/ExtensionsUI.sys.mjs",
  FeatureGate: "resource://featuregates/FeatureGate.sys.mjs",
  FirefoxBridgeExtensionUtils:
    "resource:///modules/FirefoxBridgeExtensionUtils.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  FxAccounts: "resource://gre/modules/FxAccounts.sys.mjs",
  HomePage: "resource:///modules/HomePage.sys.mjs",
  Integration: "resource://gre/modules/Integration.sys.mjs",
  Interactions: "resource:///modules/Interactions.sys.mjs",
  LoginBreaches: "resource:///modules/LoginBreaches.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  Normandy: "resource://normandy/Normandy.sys.mjs",
  OnboardingMessageProvider:
    "resource:///modules/asrouter/OnboardingMessageProvider.sys.mjs",
  OsEnvironment: "resource://gre/modules/OsEnvironment.sys.mjs",
  PageActions: "resource:///modules/PageActions.sys.mjs",
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
  PageThumbs: "resource://gre/modules/PageThumbs.sys.mjs",
  PdfJs: "resource://pdf.js/PdfJs.sys.mjs",
  PermissionUI: "resource:///modules/PermissionUI.sys.mjs",
  PlacesBackups: "resource://gre/modules/PlacesBackups.sys.mjs",
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  // PluginManager is used by listeners object below.
  // eslint-disable-next-line mozilla/valid-lazy
  PluginManager: "resource:///actors/PluginParent.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ProcessHangMonitor: "resource:///modules/ProcessHangMonitor.sys.mjs",
  PublicSuffixList:
    "resource://gre/modules/netwerk-dns/PublicSuffixList.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  RFPHelper: "resource://gre/modules/RFPHelper.sys.mjs",
  RemoteSecuritySettings:
    "resource://gre/modules/psm/RemoteSecuritySettings.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  ResetPBMPanel: "resource:///modules/ResetPBMPanel.sys.mjs",
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.sys.mjs",
  Sanitizer: "resource:///modules/Sanitizer.sys.mjs",
  SaveToPocket: "chrome://pocket/content/SaveToPocket.sys.mjs",
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SessionStartup: "resource:///modules/sessionstore/SessionStartup.sys.mjs",
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
  ShellService: "resource:///modules/ShellService.sys.mjs",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.sys.mjs",
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
  TRRRacer: "resource:///modules/TRRPerformance.sys.mjs",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.sys.mjs",
  TabUnloader: "resource:///modules/TabUnloader.sys.mjs",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  WebChannel: "resource://gre/modules/WebChannel.sys.mjs",
  WebProtocolHandlerRegistrar:
    "resource:///modules/WebProtocolHandlerRegistrar.sys.mjs",
  WindowsLaunchOnLogin: "resource://gre/modules/WindowsLaunchOnLogin.sys.mjs",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.sys.mjs",
  WindowsGPOParser: "resource://gre/modules/policies/WindowsGPOParser.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

if (AppConstants.MOZ_UPDATER) {
  ChromeUtils.defineESModuleGetters(lazy, {
    UpdateListener: "resource://gre/modules/UpdateListener.sys.mjs",
  });
  XPCOMUtils.defineLazyServiceGetters(lazy, {
    UpdateServiceStub: [
      "@mozilla.org/updates/update-service-stub;1",
      "nsIApplicationUpdateServiceStub",
    ],
  });
}
if (AppConstants.MOZ_UPDATE_AGENT) {
  ChromeUtils.defineESModuleGetters(lazy, {
    BackgroundUpdate: "resource://gre/modules/BackgroundUpdate.sys.mjs",
  });
}

// PluginManager is used in the listeners object below.
XPCOMUtils.defineLazyServiceGetters(lazy, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
  PushService: ["@mozilla.org/push/Service;1", "nsIPushService"],
});

ChromeUtils.defineLazyGetter(
  lazy,
  "accountsL10n",
  () => new Localization(["browser/accounts.ftl"], true)
);

if (AppConstants.ENABLE_WEBDRIVER) {
  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "Marionette",
    "@mozilla.org/remote/marionette;1",
    "nsIMarionette"
  );

  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "RemoteAgent",
    "@mozilla.org/remote/agent;1",
    "nsIRemoteAgent"
  );
} else {
  lazy.Marionette = { running: false };
  lazy.RemoteAgent = { running: false };
}

const PREF_PDFJS_ISDEFAULT_CACHE_STATE = "pdfjs.enabledCache.state";

const PRIVATE_BROWSING_BINARY = "private_browsing.exe";
// Index of Private Browsing icon in private_browsing.exe
// Must line up with IDI_PBICON_PB_PB_EXE in nsNativeAppSupportWin.h.
const PRIVATE_BROWSING_EXE_ICON_INDEX = 1;
const PREF_PRIVATE_BROWSING_SHORTCUT_CREATED =
  "browser.privacySegmentation.createdShortcut";
// Whether this launch was initiated by the OS.  A launch-on-login will contain
// the "os-autostart" flag in the initial launch command line.
let gThisInstanceIsLaunchOnLogin = false;
// Whether this launch was initiated by a taskbar tab shortcut. A launch from
// a taskbar tab shortcut will contain the "taskbar-tab" flag.
let gThisInstanceIsTaskbarTab = false;

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
      esModuleURI: "resource:///actors/BrowserProcessChild.sys.mjs",
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
      esModuleURI: "resource:///actors/RefreshBlockerChild.sys.mjs",
      observers: [
        "webnavigation-create",
        "chrome-webnavigation-create",
        "webnavigation-destroy",
        "chrome-webnavigation-destroy",
      ],
    },

    enablePreference: "accessibility.blockautorefresh",
    onPreferenceChanged: (prefName, prevValue, isEnabled) => {
      lazy.BrowserWindowTracker.orderedWindows.forEach(win => {
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
  Megalist: {
    parent: {
      esModuleURI: "resource://gre/actors/MegalistParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://gre/actors/MegalistChild.sys.mjs",
      events: {
        DOMContentLoaded: {},
      },
    },
    includeChrome: true,
    matches: ["chrome://global/content/megalist/megalist.html"],
    allFrames: true,
    enablePreference: "browser.megalist.enabled",
  },

  AboutLogins: {
    parent: {
      esModuleURI: "resource:///actors/AboutLoginsParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutLoginsChild.sys.mjs",
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
    allFrames: true,
    remoteTypes: ["privilegedabout"],
  },

  AboutMessagePreview: {
    parent: {
      esModuleURI: "resource:///actors/AboutMessagePreviewParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutMessagePreviewChild.sys.mjs",
      events: {
        DOMDocElementInserted: { capture: true },
      },
    },
    matches: ["about:messagepreview", "about:messagepreview?*"],
  },

  AboutNewTab: {
    parent: {
      esModuleURI: "resource:///actors/AboutNewTabParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutNewTabChild.sys.mjs",
      events: {
        DOMDocElementInserted: {},
        DOMContentLoaded: {},
        load: { capture: true },
        unload: { capture: true },
        pageshow: {},
        visibilitychange: {},
      },
    },
    // The wildcard on about:newtab is for the # parameter
    // that is used for the newtab devtools. The wildcard for about:home
    // is similar, and also allows for falling back to loading the
    // about:home document dynamically if an attempt is made to load
    // about:home?jscache from the AboutHomeStartupCache as a top-level
    // load.
    matches: ["about:home*", "about:welcome", "about:newtab*"],
    remoteTypes: ["privilegedabout"],
  },

  AboutPocket: {
    parent: {
      esModuleURI: "resource:///actors/AboutPocketParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutPocketChild.sys.mjs",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    remoteTypes: ["privilegedabout"],
    matches: [
      "about:pocket-saved*",
      "about:pocket-signup*",
      "about:pocket-home*",
      "about:pocket-style-guide*",
    ],
  },

  AboutPrivateBrowsing: {
    parent: {
      esModuleURI: "resource:///actors/AboutPrivateBrowsingParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutPrivateBrowsingChild.sys.mjs",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:privatebrowsing*"],
  },

  AboutProtections: {
    parent: {
      esModuleURI: "resource:///actors/AboutProtectionsParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutProtectionsChild.sys.mjs",

      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:protections", "about:protections?*"],
  },

  AboutReader: {
    parent: {
      esModuleURI: "resource:///actors/AboutReaderParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutReaderChild.sys.mjs",
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
      esModuleURI: "resource:///actors/AboutTabCrashedParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutTabCrashedChild.sys.mjs",
      events: {
        DOMDocElementInserted: { capture: true },
      },
    },

    matches: ["about:tabcrashed*"],
  },

  AboutWelcomeShopping: {
    parent: {
      esModuleURI: "resource:///actors/AboutWelcomeParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutWelcomeChild.sys.mjs",
      events: {
        Update: {},
      },
    },
    matches: ["about:shoppingsidebar"],
    remoteTypes: ["privilegedabout"],
  },

  AboutWelcome: {
    parent: {
      esModuleURI: "resource:///actors/AboutWelcomeParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/AboutWelcomeChild.sys.mjs",
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

  BackupUI: {
    parent: {
      esModuleURI: "resource:///actors/BackupUIParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/BackupUIChild.sys.mjs",
      events: {
        "BackupUI:InitWidget": { wantUntrusted: true },
        "BackupUI:ScheduledBackupsConfirm": { wantUntrusted: true },
      },
    },
    matches: ["about:preferences*", "about:settings*"],
    enablePreference: "browser.backup.preferences.ui.enabled",
  },

  BlockedSite: {
    parent: {
      esModuleURI: "resource:///actors/BlockedSiteParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/BlockedSiteChild.sys.mjs",
      events: {
        AboutBlockedLoaded: { wantUntrusted: true },
        click: {},
      },
    },
    matches: ["about:blocked?*"],
    allFrames: true,
  },

  BrowserTab: {
    child: {
      esModuleURI: "resource:///actors/BrowserTabChild.sys.mjs",
    },

    messageManagerGroups: ["browsers"],
  },

  ClickHandler: {
    parent: {
      esModuleURI: "resource:///actors/ClickHandlerParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/ClickHandlerChild.sys.mjs",
      events: {
        chromelinkclick: { capture: true, mozSystemGroup: true },
      },
    },

    allFrames: true,
  },

  /* Note: this uses the same JSMs as ClickHandler, but because it
   * relies on "normal" click events anywhere on the page (not just
   * links) and is expensive, and only does something for the
   * small group of people who have the feature enabled, it is its
   * own actor which is only registered if the pref is enabled.
   */
  MiddleMousePasteHandler: {
    parent: {
      esModuleURI: "resource:///actors/ClickHandlerParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/ClickHandlerChild.sys.mjs",
      events: {
        auxclick: { capture: true, mozSystemGroup: true },
      },
    },
    enablePreference: "middlemouse.contentLoadURL",

    allFrames: true,
  },

  ContentSearch: {
    parent: {
      esModuleURI: "resource:///actors/ContentSearchParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/ContentSearchChild.sys.mjs",
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
      esModuleURI: "resource:///actors/ContextMenuParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource:///actors/ContextMenuChild.sys.mjs",
      events: {
        contextmenu: { mozSystemGroup: true },
      },
    },

    allFrames: true,
  },

  DecoderDoctor: {
    parent: {
      esModuleURI: "resource:///actors/DecoderDoctorParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource:///actors/DecoderDoctorChild.sys.mjs",
      observers: ["decoder-doctor-notification"],
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  DOMFullscreen: {
    parent: {
      esModuleURI: "resource:///actors/DOMFullscreenParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource:///actors/DOMFullscreenChild.sys.mjs",
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
      esModuleURI: "resource:///actors/EncryptedMediaParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource:///actors/EncryptedMediaChild.sys.mjs",
      observers: ["mediakeys-request"],
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  FormValidation: {
    parent: {
      esModuleURI: "resource:///actors/FormValidationParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource:///actors/FormValidationChild.sys.mjs",
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
      esModuleURI: "resource:///actors/LightweightThemeChild.sys.mjs",
      events: {
        pageshow: { mozSystemGroup: true },
        DOMContentLoaded: {},
      },
    },
    includeChrome: true,
    allFrames: true,
    matches: [
      "about:asrouter",
      "about:home",
      "about:newtab",
      "about:welcome",
      "chrome://browser/content/syncedtabs/sidebar.xhtml",
      "chrome://browser/content/places/historySidebar.xhtml",
      "chrome://browser/content/places/bookmarksSidebar.xhtml",
      "about:firefoxview",
    ],
  },

  LinkHandler: {
    parent: {
      esModuleURI: "resource:///actors/LinkHandlerParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/LinkHandlerChild.sys.mjs",
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

  PageInfo: {
    child: {
      esModuleURI: "resource:///actors/PageInfoChild.sys.mjs",
    },

    allFrames: true,
  },

  PageStyle: {
    parent: {
      esModuleURI: "resource:///actors/PageStyleParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/PageStyleChild.sys.mjs",
      events: {
        pageshow: { createActor: false },
      },
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  Pdfjs: {
    parent: {
      esModuleURI: "resource://pdf.js/PdfjsParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://pdf.js/PdfjsChild.sys.mjs",
    },
    allFrames: true,
  },

  // GMP crash reporting
  Plugin: {
    parent: {
      esModuleURI: "resource:///actors/PluginParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/PluginChild.sys.mjs",
      events: {
        PluginCrashed: { capture: true },
      },
    },

    allFrames: true,
  },

  PointerLock: {
    parent: {
      esModuleURI: "resource:///actors/PointerLockParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/PointerLockChild.sys.mjs",
      events: {
        "MozDOMPointerLock:Entered": {},
        "MozDOMPointerLock:Exited": {},
      },
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  Profiles: {
    parent: {
      esModuleURI: "resource:///actors/ProfilesParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/ProfilesChild.sys.mjs",
    },
    matches: ["about:profilemanager"],
    enablePreference: "browser.profiles.enabled",
  },

  Prompt: {
    parent: {
      esModuleURI: "resource:///actors/PromptParent.sys.mjs",
    },
    includeChrome: true,
    allFrames: true,
  },

  RefreshBlocker: {
    parent: {
      esModuleURI: "resource:///actors/RefreshBlockerParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/RefreshBlockerChild.sys.mjs",
    },

    messageManagerGroups: ["browsers"],
    enablePreference: "accessibility.blockautorefresh",
  },

  ScreenshotsComponent: {
    parent: {
      esModuleURI: "resource:///modules/ScreenshotsUtils.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/ScreenshotsComponentChild.sys.mjs",
      events: {
        "Screenshots:Close": {},
        "Screenshots:Copy": {},
        "Screenshots:Download": {},
        "Screenshots:HidePanel": {},
        "Screenshots:OverlaySelection": {},
        "Screenshots:RecordEvent": {},
        "Screenshots:ShowPanel": {},
        "Screenshots:FocusPanel": {},
      },
    },
    enablePreference: "screenshots.browser.component.enabled",
  },

  ScreenshotsHelper: {
    parent: {
      esModuleURI: "resource:///modules/ScreenshotsUtils.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///modules/ScreenshotsHelperChild.sys.mjs",
    },
    allFrames: true,
    enablePreference: "screenshots.browser.component.enabled",
  },

  SearchSERPTelemetry: {
    parent: {
      esModuleURI: "resource:///actors/SearchSERPTelemetryParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/SearchSERPTelemetryChild.sys.mjs",
      events: {
        DOMContentLoaded: {},
        pageshow: { mozSystemGroup: true },
        // The 'pagehide' event is only used to clean up state, and should not
        // force actor creation.
        pagehide: { createActor: false },
        load: { mozSystemGroup: true, capture: true },
      },
    },
    matches: ["https://*/*"],
  },

  ShieldFrame: {
    parent: {
      esModuleURI: "resource://normandy-content/ShieldFrameParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://normandy-content/ShieldFrameChild.sys.mjs",
      events: {
        pageshow: {},
        pagehide: {},
        ShieldPageEvent: { wantUntrusted: true },
      },
    },
    matches: ["about:studies*"],
  },

  ShoppingSidebar: {
    parent: {
      esModuleURI: "resource:///actors/ShoppingSidebarParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/ShoppingSidebarChild.sys.mjs",
      events: {
        ContentReady: { wantUntrusted: true },
        PolledRequestMade: { wantUntrusted: true },
        // This is added so the actor instantiates immediately and makes
        // methods available to the page js on load.
        DOMDocElementInserted: {},
        ReportProductAvailable: { wantUntrusted: true },
        AdClicked: { wantUntrusted: true },
        AdImpression: { wantUntrusted: true },
        DisableShopping: { wantUntrusted: true },
      },
    },
    matches: ["about:shoppingsidebar"],
    remoteTypes: ["privilegedabout"],
  },

  SpeechDispatcher: {
    parent: {
      esModuleURI: "resource:///actors/SpeechDispatcherParent.sys.mjs",
    },

    child: {
      esModuleURI: "resource:///actors/SpeechDispatcherChild.sys.mjs",
      observers: ["chrome-synth-voices-error"],
    },

    messageManagerGroups: ["browsers"],
    allFrames: true,
  },

  ASRouter: {
    parent: {
      esModuleURI: "resource:///actors/ASRouterParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/ASRouterChild.sys.mjs",
      events: {
        // This is added so the actor instantiates immediately and makes
        // methods available to the page js on load.
        DOMDocElementInserted: {},
      },
    },
    matches: [
      "about:asrouter*",
      "about:home*",
      "about:newtab*",
      "about:welcome*",
      "about:privatebrowsing*",
    ],
    remoteTypes: ["privilegedabout"],
  },

  SwitchDocumentDirection: {
    child: {
      esModuleURI: "resource:///actors/SwitchDocumentDirectionChild.sys.mjs",
    },

    allFrames: true,
  },

  UITour: {
    parent: {
      esModuleURI: "resource:///modules/UITourParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///modules/UITourChild.sys.mjs",
      events: {
        mozUITour: { wantUntrusted: true },
      },
    },

    messageManagerGroups: ["browsers"],
  },

  WebRTC: {
    parent: {
      esModuleURI: "resource:///actors/WebRTCParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource:///actors/WebRTCChild.sys.mjs",
    },

    allFrames: true,
  },
};

ChromeUtils.defineLazyGetter(
  lazy,
  "WeaveService",
  () => Cc["@mozilla.org/weave/service;1"].getService().wrappedJSObject
);

if (AppConstants.MOZ_CRASHREPORTER) {
  ChromeUtils.defineESModuleGetters(lazy, {
    UnsubmittedCrashHandler: "resource:///modules/ContentCrashHandlers.sys.mjs",
  });
}

ChromeUtils.defineLazyGetter(lazy, "gBrandBundle", function () {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

ChromeUtils.defineLazyGetter(lazy, "gBrowserBundle", function () {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use lazy.log.debug() to create
    // detailed messages during development. See LOG_LEVELS in Console.sys.mjs
    // for details.
    maxLogLevel: "error",
    maxLogLevelPref: "browser.policies.loglevel",
    prefix: "BrowserGlue.sys.mjs",
  };
  return new ConsoleAPI(consoleOptions);
});

const listeners = {
  observers: {
    "gmp-plugin-crash": ["PluginManager"],
    "plugin-crashed": ["PluginManager"],
  },

  observe(subject, topic, data) {
    for (let module of this.observers[topic]) {
      try {
        lazy[module].observe(subject, topic, data);
      } catch (e) {
        console.error(e);
      }
    }
  },

  init() {
    for (let observer of Object.keys(this.observers)) {
      Services.obs.addObserver(this, observer);
    }
  },
};
if (AppConstants.MOZ_UPDATER) {
  listeners.observers["update-downloading"] = ["UpdateListener"];
  listeners.observers["update-staged"] = ["UpdateListener"];
  listeners.observers["update-downloaded"] = ["UpdateListener"];
  listeners.observers["update-available"] = ["UpdateListener"];
  listeners.observers["update-error"] = ["UpdateListener"];
  listeners.observers["update-swap"] = ["UpdateListener"];
}

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

export let BrowserInitState = {};
BrowserInitState.startupIdleTaskPromise = new Promise(resolve => {
  BrowserInitState._resolveStartupIdleTask = resolve;
});

export function BrowserGlue() {
  XPCOMUtils.defineLazyServiceGetter(
    this,
    "_userIdleService",
    "@mozilla.org/widget/useridleservice;1",
    "nsIUserIdleService"
  );

  ChromeUtils.defineLazyGetter(this, "_distributionCustomizer", function () {
    const { DistributionCustomizer } = ChromeUtils.importESModule(
      "resource:///modules/distribution.sys.mjs"
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

function WindowsRegPoliciesGetter(wrk, root, regLocation) {
  wrk.open(root, regLocation, wrk.ACCESS_READ);
  let policies;
  if (wrk.hasChild("Mozilla\\" + Services.appinfo.name)) {
    policies = lazy.WindowsGPOParser.readPolicies(wrk, policies);
  }
  wrk.close();
  return policies;
}

function isPrivateBrowsingAllowedInRegistry() {
  // If there is an attempt to open Private Browsing before
  // EnterprisePolicies are initialized the Windows registry
  // can be checked to determine if it is enabled
  if (Services.policies.status > Ci.nsIEnterprisePolicies.UNINITIALIZED) {
    // Yield to policies engine if initialized
    let privateAllowed = Services.policies.isAllowed("privatebrowsing");
    lazy.log.debug(
      `Yield to initialized policies engine: Private Browsing Allowed = ${privateAllowed}`
    );
    return privateAllowed;
  }
  if (AppConstants.platform !== "win") {
    // Not using Windows so no registry, return true
    lazy.log.debug(
      "AppConstants.platform is not 'win': Private Browsing allowed"
    );
    return true;
  }
  // If all other checks fail only then do we check registry
  let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
    Ci.nsIWindowsRegKey
  );
  let regLocation = "SOFTWARE\\Policies";
  let userPolicies, machinePolicies;
  // Only check HKEY_LOCAL_MACHINE if not in testing
  if (!Cu.isInAutomation) {
    machinePolicies = WindowsRegPoliciesGetter(
      wrk,
      wrk.ROOT_KEY_LOCAL_MACHINE,
      regLocation
    );
  }
  // Check machine policies before checking user policies
  // HKEY_LOCAL_MACHINE supersedes HKEY_CURRENT_USER so only check
  // HKEY_CURRENT_USER if the registry key is not present in
  // HKEY_LOCAL_MACHINE at all
  if (machinePolicies && "DisablePrivateBrowsing" in machinePolicies) {
    lazy.log.debug(
      `DisablePrivateBrowsing in HKEY_LOCAL_MACHINE is ${machinePolicies.DisablePrivateBrowsing}`
    );
    return !(machinePolicies.DisablePrivateBrowsing === 1);
  }
  userPolicies = WindowsRegPoliciesGetter(
    wrk,
    wrk.ROOT_KEY_CURRENT_USER,
    regLocation
  );
  if (userPolicies && "DisablePrivateBrowsing" in userPolicies) {
    lazy.log.debug(
      `DisablePrivateBrowsing in HKEY_CURRENT_USER is ${userPolicies.DisablePrivateBrowsing}`
    );
    return !(userPolicies.DisablePrivateBrowsing === 1);
  }
  // Private browsing allowed if no registry entry exists
  lazy.log.debug(
    "No DisablePrivateBrowsing registry entry: Private Browsing allowed"
  );
  return true;
}

BrowserGlue.prototype = {
  _saveSession: false,
  _migrationImportsDefaultBookmarks: false,
  _placesBrowserInitComplete: false,
  _isNewProfile: undefined,
  _defaultCookieBehaviorAtStartup: null,

  _setPrefToSaveSession: function BG__setPrefToSaveSession(aForce) {
    if (!this._saveSession && !aForce) {
      return;
    }

    if (!lazy.PrivateBrowsingUtils.permanentPrivateBrowsing) {
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
      case "fxaccounts:commands:close-uri":
        this._onIncomingCloseTabCommand(subject);
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
      case "handle-xul-text-link": {
        let linkHandled = subject.QueryInterface(Ci.nsISupportsPRBool);
        if (!linkHandled.data) {
          let win = lazy.BrowserWindowTracker.getTopWindow();
          if (win) {
            data = JSON.parse(data);
            let where = lazy.BrowserUtils.whereToOpenLink(data);
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
      }
      case "profile-before-change":
        // Any component depending on Places should be finalized in
        // _onPlacesShutdown.  Any component that doesn't need to act after
        // the UI has gone should be finalized in _onQuitApplicationGranted.
        this._dispose();
        break;
      case "keyword-search": {
        // This notification is broadcast by the docshell when it "fixes up" a
        // URI that it's been asked to load into a keyword search.
        let engine = null;
        try {
          engine = Services.search.getEngineByName(
            subject.QueryInterface(Ci.nsISupportsString).data
          );
        } catch (ex) {
          console.error(ex);
        }
        let win = lazy.BrowserWindowTracker.getTopWindow();
        lazy.BrowserSearchTelemetry.recordSearch(
          win.gBrowser.selectedBrowser,
          engine,
          "urlbar"
        );
        break;
      }
      case "xpi-signature-changed": {
        let disabledAddons = JSON.parse(data).disabled;
        let addons = await lazy.AddonManager.getAddonsByIDs(disabledAddons);
        if (addons.some(addon => addon)) {
          this._notifyUnsignedAddonsDisabled();
        }
        break;
      }
      case "sync-ui-state:update":
        this._updateFxaBadges(lazy.BrowserWindowTracker.getTopWindow());
        break;
      case "handlersvc-store-initialized":
        // Initialize PdfJs when running in-process and remote. This only
        // happens once since PdfJs registers global hooks. If the PdfJs
        // extension is installed the init method below will be overridden
        // leaving initialization to the extension.
        // parent only: configure default prefs, set up pref observers, register
        // pdf content handler, and initializes parent side message manager
        // shim for privileged api access.
        lazy.PdfJs.init(this._isNewProfile);

        // Allow certain viewable internally types to be opened from downloads.
        lazy.DownloadsViewableInternally.register();

        break;
      case "app-startup": {
        this._earlyBlankFirstPaint(subject);
        gThisInstanceIsTaskbarTab = subject.handleFlag("taskbar-tab", false);
        gThisInstanceIsLaunchOnLogin = subject.handleFlag(
          "os-autostart",
          false
        );
        let launchOnLoginPref = "browser.startup.windowsLaunchOnLogin.enabled";
        let profileSvc = Cc[
          "@mozilla.org/toolkit/profile-service;1"
        ].getService(Ci.nsIToolkitProfileService);
        if (
          AppConstants.platform == "win" &&
          !profileSvc.startWithLastProfile
        ) {
          // If we don't start with last profile, the user
          // likely sees the profile selector on launch.
          if (Services.prefs.getBoolPref(launchOnLoginPref)) {
            Services.telemetry.setEventRecordingEnabled(
              "launch_on_login",
              true
            );
            Services.telemetry.recordEvent(
              "launch_on_login",
              "last_profile_disable",
              "startup"
            );
          }
          Services.prefs.setBoolPref(launchOnLoginPref, false);
          // To reduce confusion when running multiple Gecko profiles,
          // delete launch on login shortcuts and registry keys so that
          // users are not presented with the outdated profile selector
          // dialog.
          lazy.WindowsLaunchOnLogin.removeLaunchOnLogin();
        }
        break;
      }
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
      "fxaccounts:onverified",
      "fxaccounts:device_connected",
      "fxaccounts:verify_login",
      "fxaccounts:device_disconnected",
      "fxaccounts:commands:open-uri",
      "fxaccounts:commands:close-uri",
      "session-save",
      "places-init-complete",
      "distribution-customization-complete",
      "handle-xul-text-link",
      "profile-before-change",
      "keyword-search",
      "restart-in-safe-mode",
      "xpi-signature-changed",
      "sync-ui-state:update",
      "handlersvc-store-initialized",
    ].forEach(topic => os.addObserver(this, topic, true));
    if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
      os.addObserver(this, "browser-lastwindow-close-requested", true);
      os.addObserver(this, "browser-lastwindow-close-granted", true);
    }

    lazy.ActorManagerParent.addJSProcessActors(JSPROCESSACTORS);
    lazy.ActorManagerParent.addJSWindowActors(JSWINDOWACTORS);

    this._firstWindowReady = new Promise(
      resolve => (this._firstWindowLoaded = resolve)
    );
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
      "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "privacy.partition.network_state.ocsp_cache",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "privacy.query_stripping.enabled",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "privacy.query_stripping.enabled.pbmode",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "privacy.fingerprintingProtection",
      this._matchCBCategory
    );
    Services.prefs.removeObserver(
      "privacy.fingerprintingProtection.pbmode",
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
  },

  // runs on startup, before the first command line handler is invoked
  // (i.e. before the first window is opened)
  _beforeUIStartup: function BG__beforeUIStartup() {
    lazy.SessionStartup.init();

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
      lazy.PdfJs.checkIsDefault(this._isNewProfile);
    }

    if (!AppConstants.NIGHTLY_BUILD && this._isNewProfile) {
      lazy.FormAutofillUtils.setOSAuthEnabled(
        lazy.FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
        false
      );
      lazy.LoginHelper.setOSAuthEnabled(
        lazy.LoginHelper.OS_AUTH_FOR_PASSWORDS_PREF,
        false
      );
    }

    listeners.init();

    lazy.SessionStore.init();

    lazy.BuiltInThemes.maybeInstallActiveBuiltInTheme();

    if (AppConstants.MOZ_NORMANDY) {
      lazy.Normandy.init();
    }

    lazy.SaveToPocket.init();

    lazy.ResetPBMPanel.init();

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
        // This is asynchronous, but just kick it off rather than waiting.
        Cc["@mozilla.org/updates/update-service;1"]
          .getService(Ci.nsIApplicationUpdateService)
          .checkForBackgroundUpdates();
      }
    }
  },

  async _onSafeModeRestart(window) {
    // prompt the user to confirm
    let productName = lazy.gBrandBundle.GetStringFromName("brandShortName");
    let strings = lazy.gBrowserBundle;
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
    let win = lazy.BrowserWindowTracker.getTopWindow();
    if (!win) {
      return;
    }

    const { ResetProfile } = ChromeUtils.importESModule(
      "resource://gre/modules/ResetProfile.sys.mjs"
    );
    if (!ResetProfile.resetSupported()) {
      return;
    }

    let productName = lazy.gBrandBundle.GetStringFromName("brandShortName");
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
    let win = lazy.BrowserWindowTracker.getTopWindow();
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
          win.BrowserAddonUI.openAddonsMgr(
            "addons://list/extension?unsigned=true"
          );
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

  _earlyBlankFirstPaint(cmdLine) {
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
    // This needs to be set when opening the window to ensure that the AppUserModelID
    // is set correctly on Windows. Without it, initial launches with `-private-window`
    // will show up under the regular Firefox taskbar icon first, and then switch
    // to the Private Browsing icon shortly thereafter.
    if (cmdLine.findFlag("private-window", false) != -1) {
      if (isPrivateBrowsingAllowedInRegistry()) {
        browserWindowFeatures += ",private";
      }
    }
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

    let { TelemetryTimestamps } = ChromeUtils.importESModule(
      "resource://gre/modules/TelemetryTimestamps.sys.mjs"
    );
    TelemetryTimestamps.add("blankWindowShown");
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
      console.error(ex);
    }
  },

  // the first browser window has finished initializing
  _onFirstWindowLoaded: function BG__onFirstWindowLoaded(aWindow) {
    lazy.AboutNewTab.init();

    lazy.TabCrashHandler.init();

    lazy.ProcessHangMonitor.init();

    lazy.UrlbarPrefs.updateFirefoxSuggestScenario();

    // A channel for "remote troubleshooting" code...
    let channel = new lazy.WebChannel(
      "remote-troubleshooting",
      "remote-troubleshooting"
    );
    channel.listen((id, data, target) => {
      if (data.command == "request") {
        let { Troubleshoot } = ChromeUtils.importESModule(
          "resource://gre/modules/Troubleshoot.sys.mjs"
        );
        Troubleshoot.snapshot().then(snapshotData => {
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
        updateChannel = ChromeUtils.importESModule(
          "resource://gre/modules/UpdateUtils.sys.mjs"
        ).UpdateUtils.UpdateChannel;
      } catch (ex) {}
      if (updateChannel) {
        let uninstalledValue = lazy.WindowsRegistry.readRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          "Software\\Mozilla\\Firefox",
          `Uninstalled-${updateChannel}`
        );
        let removalSuccessful = lazy.WindowsRegistry.removeRegKey(
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
      lazy.WeaveService.init();
    }

    lazy.PageThumbs.init();

    lazy.NewTabUtils.init();

    Services.telemetry.setEventRecordingEnabled(
      "security.ui.protections",
      true
    );

    Services.telemetry.setEventRecordingEnabled("security.doh.neterror", true);

    lazy.PageActions.init();

    lazy.DoHController.init();

    this._firstWindowTelemetry(aWindow);
    this._firstWindowLoaded();

    this._collectStartupConditionsTelemetry();

    // Set the default favicon size for UI views that use the page-icon protocol.
    lazy.PlacesUtils.favicons.setDefaultIconURIPreferredSize(
      16 * aWindow.devicePixelRatio
    );

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
      "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "privacy.partition.network_state.ocsp_cache",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "privacy.query_stripping.enabled",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "privacy.query_stripping.enabled.pbmode",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "privacy.fingerprintingProtection",
      this._matchCBCategory
    );
    Services.prefs.addObserver(
      "privacy.fingerprintingProtection.pbmode",
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

      () => lazy.BrowserUsageTelemetry.uninit(),
      () => lazy.SearchSERPTelemetry.uninit(),
      () => lazy.SearchSERPCategorization.uninit(),
      () => lazy.Interactions.uninit(),
      () => lazy.PageDataService.uninit(),
      () => lazy.PageThumbs.uninit(),
      () => lazy.NewTabUtils.uninit(),
      () => lazy.Normandy.uninit(),
      () => lazy.RFPHelper.uninit(),
      () => lazy.ShoppingUtils.uninit(),
      () => lazy.ASRouterNewTabHook.destroy(),
      () => {
        if (AppConstants.MOZ_UPDATER) {
          lazy.UpdateListener.reset();
        }
      },
      () => {
        // bug 1839426 - The FOG service needs to be instantiated reliably so it
        // can perform at-shutdown tasks later in shutdown.
        Services.fog;
      },
    ];

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
    const SCREENSHOTS_PREF = "extensions.screenshots.disabled";
    const COMPONENT_PREF = "screenshots.browser.component.enabled";
    const ID = "screenshots@mozilla.org";
    const _checkScreenshotsPref = async () => {
      let screenshotsDisabled = Services.prefs.getBoolPref(
        SCREENSHOTS_PREF,
        false
      );
      let componentEnabled = Services.prefs.getBoolPref(COMPONENT_PREF, true);

      let screenshotsAddon = await lazy.AddonManager.getAddonByID(ID);

      if (screenshotsDisabled) {
        if (componentEnabled) {
          lazy.ScreenshotsUtils.uninitialize();
        } else if (screenshotsAddon?.isActive) {
          await screenshotsAddon.disable({ allowSystemAddons: true });
        }
      } else if (componentEnabled) {
        lazy.ScreenshotsUtils.initialize();
        if (screenshotsAddon?.isActive) {
          await screenshotsAddon.disable({ allowSystemAddons: true });
        }
      } else {
        try {
          await screenshotsAddon.enable({ allowSystemAddons: true });
        } catch (ex) {
          console.error(`Error trying to enable screenshots extension: ${ex}`);
        }
        lazy.ScreenshotsUtils.uninitialize();
      }
    };
    Services.prefs.addObserver(SCREENSHOTS_PREF, _checkScreenshotsPref);
    Services.prefs.addObserver(COMPONENT_PREF, _checkScreenshotsPref);
    _checkScreenshotsPref();
  },

  _monitorWebcompatReporterPref() {
    const PREF = "extensions.webcompat-reporter.enabled";
    const ID = "webcompat-reporter@mozilla.org";
    Services.prefs.addObserver(PREF, async () => {
      let addon = await lazy.AddonManager.getAddonByID(ID);
      if (!addon) {
        return;
      }
      let enabled = Services.prefs.getBoolPref(PREF, false);
      if (enabled && !addon.isActive) {
        await addon.enable({ allowSystemAddons: true });
      } else if (!enabled && addon.isActive) {
        await addon.disable({ allowSystemAddons: true });
      }
    });
  },

  async _setupSearchDetection() {
    // There is no pref for this add-on because it shouldn't be disabled.
    const ID = "addons-search-detection@mozilla.com";

    let addon = await lazy.AddonManager.getAddonByID(ID);

    // first time install of addon and install on firefox update
    addon =
      (await lazy.AddonManager.maybeInstallBuiltinAddon(
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
        win.document.getElementById("ion-button").hidden =
          !Services.prefs.getStringPref(PREF_ION_ID, null);
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

    lazy.RemoteSettings(STUDY_ADDON_COLLECTION_KEY).on("sync", async () => {
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

    lazy.BrowserUsageTelemetry.init();
    lazy.SearchSERPTelemetry.init();

    lazy.Interactions.init();
    lazy.PageDataService.init();
    lazy.ExtensionsUI.init();

    let signingRequired;
    if (AppConstants.MOZ_REQUIRE_SIGNING) {
      signingRequired = true;
    } else {
      signingRequired = Services.prefs.getBoolPref(
        "xpinstall.signatures.required"
      );
    }

    if (signingRequired) {
      let disabledAddons = lazy.AddonManager.getStartupChanges(
        lazy.AddonManager.STARTUP_CHANGE_DISABLED
      );
      lazy.AddonManager.getAddonsByIDs(disabledAddons).then(addons => {
        for (let addon of addons) {
          if (addon.signedState <= lazy.AddonManager.SIGNEDSTATE_MISSING) {
            this._notifyUnsignedAddonsDisabled();
            break;
          }
        }
      });
    }

    if (AppConstants.MOZ_CRASHREPORTER) {
      lazy.UnsubmittedCrashHandler.init();
      lazy.UnsubmittedCrashHandler.scheduleCheckForUnsubmittedCrashReports();
      lazy.FeatureGate.annotateCrashReporter();
      lazy.FeatureGate.observePrefChangesForCrashReportAnnotation();
    }

    if (AppConstants.ASAN_REPORTER) {
      var { AsanReporter } = ChromeUtils.importESModule(
        "resource://gre/modules/AsanReporter.sys.mjs"
      );
      AsanReporter.init();
    }

    lazy.Sanitizer.onStartup();
    this._maybeShowRestoreSessionInfoBar();
    this._scheduleStartupIdleTasks();
    this._lateTasksIdleObserver = (idleService, topic) => {
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

    this._monitorWebcompatReporterPref();
    this._monitorHTTPSOnlyPref();
    this._monitorIonPref();
    this._monitorIonStudies();
    this._setupSearchDetection();

    this._monitorGPCPref();

    // Loading the MigrationUtils module does the work of registering the
    // migration wizard JSWindowActor pair. In case nothing else has done
    // this yet, load the MigrationUtils so that the wizard is ready to be
    // used.
    lazy.MigrationUtils;
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
        name: "SafeBrowsing.init",
        task: () => {
          lazy.SafeBrowsing.init();
        },
        timeout: 5000,
      },

      {
        name: "ContextualIdentityService.load",
        task: async () => {
          await lazy.ContextualIdentityService.load();
          lazy.Discovery.update();
        },
      },

      {
        name: "PlacesUIUtils.unblockToolbars",
        task: () => {
          // We postponed loading bookmarks toolbar content until startup
          // has finished, so we can start loading it now:
          lazy.PlacesUIUtils.unblockToolbars();
        },
      },

      {
        name: "PlacesDBUtils.telemetry",
        condition: lazy.TelemetryUtils.isTelemetryEnabled,
        task: () => {
          lazy.PlacesDBUtils.telemetry().catch(console.error);
        },
      },

      // Begin listening for incoming push messages.
      {
        name: "PushService.ensureReady",
        task: () => {
          try {
            lazy.PushService.wrappedJSObject.ensureReady();
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
        name: "BrowserGlue._recordContentBlockingTelemetry",
        task: () => {
          this._recordContentBlockingTelemetry();
        },
      },

      {
        name: "BrowserGlue._recordDataSanitizationPrefs",
        task: () => {
          this._recordDataSanitizationPrefs();
        },
      },

      {
        name: "enableCertErrorUITelemetry",
        task: () => {
          let enableCertErrorUITelemetry = Services.prefs.getBoolPref(
            "security.certerrors.recordEventTelemetry",
            true
          );
          Services.telemetry.setEventRecordingEnabled(
            "security.ui.certerror",
            enableCertErrorUITelemetry
          );
          Services.telemetry.setEventRecordingEnabled(
            "security.ui.tlserror",
            enableCertErrorUITelemetry
          );
        },
      },

      // Load the Login Manager data from disk off the main thread, some time
      // after startup.  If the data is required before this runs, for example
      // because a restored page contains a password field, it will be loaded on
      // the main thread, and this initialization request will be ignored.
      {
        name: "Services.logins",
        task: () => {
          try {
            Services.logins;
          } catch (ex) {
            console.error(ex);
          }
        },
        timeout: 3000,
      },

      // Add breach alerts pref observer reasonably early so the pref flip works
      {
        name: "_addBreachAlertsPrefObserver",
        task: () => {
          this._addBreachAlertsPrefObserver();
        },
      },

      // Report pinning status and the type of shortcut used to launch
      {
        name: "pinningStatusTelemetry",
        condition: AppConstants.platform == "win",
        task: async () => {
          let shellService = Cc[
            "@mozilla.org/browser/shell-service;1"
          ].getService(Ci.nsIWindowsShellService);
          let winTaskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
            Ci.nsIWinTaskbar
          );

          try {
            Services.telemetry.scalarSet(
              "os.environment.is_taskbar_pinned",
              await shellService.isCurrentAppPinnedToTaskbarAsync(
                winTaskbar.defaultGroupId
              )
            );
            Services.telemetry.scalarSet(
              "os.environment.is_taskbar_pinned_private",
              await shellService.isCurrentAppPinnedToTaskbarAsync(
                winTaskbar.defaultPrivateGroupId
              )
            );
          } catch (ex) {
            console.error(ex);
          }

          let classification;
          let shortcut;
          try {
            shortcut = Services.appinfo.processStartupShortcut;
            classification = shellService.classifyShortcut(shortcut);
          } catch (ex) {
            console.error(ex);
          }

          if (!classification) {
            if (gThisInstanceIsLaunchOnLogin) {
              classification = "Autostart";
            } else if (shortcut) {
              classification = "OtherShortcut";
            } else {
              classification = "Other";
            }
          }
          // Because of how taskbar tabs work, it may be classifed as a taskbar
          // shortcut, in which case we want to overwrite it.
          if (gThisInstanceIsTaskbarTab) {
            classification = "TaskbarTab";
          }
          Services.telemetry.scalarSet(
            "os.environment.launch_method",
            classification
          );
        },
      },

      {
        name: "firefoxBridgeNativeMessaging",
        condition:
          (AppConstants.platform == "macosx" ||
            AppConstants.platform == "win") &&
          Services.prefs.getBoolPref("browser.firefoxbridge.enabled", false),
        task: async () => {
          let profileService = Cc[
            "@mozilla.org/toolkit/profile-service;1"
          ].getService(Ci.nsIToolkitProfileService);
          if (
            profileService.defaultProfile &&
            profileService.currentProfile == profileService.defaultProfile
          ) {
            await lazy.FirefoxBridgeExtensionUtils.ensureRegistered();
          } else {
            lazy.log.debug(
              "FirefoxBridgeExtensionUtils failed to register due to non-default current profile."
            );
          }
        },
      },

      // Ensure a Private Browsing Shortcut exists. This is needed in case
      // a user tries to use Windows functionality to pin our Private Browsing
      // mode icon to the Taskbar (eg: the "Pin to Taskbar" context menu item).
      // This is also created by the installer, but it's possible that a user
      // has removed it, or is running out of a zip build. The consequences of not
      // having a Shortcut for this are that regular Firefox will be pinned instead
      // of the Private Browsing version -- so it's quite important we do our best
      // to make sure one is available.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1762994 for additional
      // background.
      {
        name: "ensurePrivateBrowsingShortcutExists",
        condition:
          AppConstants.platform == "win" &&
          // We don't want a shortcut if it's been disabled, eg: by enterprise policy.
          lazy.PrivateBrowsingUtils.enabled &&
          // Private Browsing shortcuts for packaged builds come with the package,
          // if they exist at all. We shouldn't try to create our own.
          !Services.sysinfo.getProperty("hasWinPackageId") &&
          // If we've ever done this successfully before, don't try again. The
          // user may have deleted the shortcut, and we don't want to force it
          // on them.
          !Services.prefs.getBoolPref(
            PREF_PRIVATE_BROWSING_SHORTCUT_CREATED,
            false
          ),
        task: async () => {
          let shellService = Cc[
            "@mozilla.org/browser/shell-service;1"
          ].getService(Ci.nsIWindowsShellService);
          let winTaskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
            Ci.nsIWinTaskbar
          );

          if (
            !(await shellService.hasMatchingShortcut(
              winTaskbar.defaultPrivateGroupId,
              true
            ))
          ) {
            let appdir = Services.dirsvc.get("GreD", Ci.nsIFile);
            let exe = appdir.clone();
            exe.append(PRIVATE_BROWSING_BINARY);
            let strings = new Localization(
              ["branding/brand.ftl", "browser/browser.ftl"],
              true
            );
            let [desc] = await strings.formatValues([
              "private-browsing-shortcut-text-2",
            ]);
            await shellService.createShortcut(
              exe,
              [],
              desc,
              exe,
              // The code we're calling indexes from 0 instead of 1
              PRIVATE_BROWSING_EXE_ICON_INDEX - 1,
              winTaskbar.defaultPrivateGroupId,
              "Programs",
              desc + ".lnk",
              appdir
            );
          }
          // We always set this as long as no exception has been thrown. This
          // ensure that it is `true` both if we created one because it didn't
          // exist, or if it already existed (most likely because it was created
          // by the installer). This avoids the need to call `hasMatchingShortcut`
          // again, which necessarily does pointless I/O.
          Services.prefs.setBoolPref(
            PREF_PRIVATE_BROWSING_SHORTCUT_CREATED,
            true
          );
        },
      },

      // Report whether Firefox is the default handler for various files types
      // and protocols, in particular, ".pdf" and "mailto"
      {
        name: "IsDefaultHandler",
        condition: AppConstants.platform == "win",
        task: () => {
          [".pdf", "mailto"].every(x => {
            Services.telemetry.keyedScalarSet(
              "os.environment.is_default_handler",
              x,
              lazy.ShellService.isDefaultHandlerFor(x)
            );
            return true;
          });
        },
      },

      // Install built-in themes. We already installed the active built-in
      // theme, if any, before UI startup.
      {
        name: "BuiltInThemes.ensureBuiltInThemes",
        task: async () => {
          await lazy.BuiltInThemes.ensureBuiltInThemes();
        },
      },

      {
        name: "WinTaskbarJumpList.startup",
        condition: AppConstants.platform == "win",
        task: () => {
          // For Windows 7, initialize the jump list module.
          const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
          if (
            WINTASKBAR_CONTRACTID in Cc &&
            Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available
          ) {
            const { WinTaskbarJumpList } = ChromeUtils.importESModule(
              "resource:///modules/WindowsJumpLists.sys.mjs"
            );
            WinTaskbarJumpList.startup();
          }
        },
      },

      // Report macOS Dock status
      {
        name: "MacDockSupport.isAppInDock",
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
            console.error(ex);
          }
        },
      },

      {
        name: "BrowserGlue._maybeShowDefaultBrowserPrompt",
        task: () => {
          this._maybeShowDefaultBrowserPrompt();
        },
      },

      {
        name: "BrowserGlue._monitorScreenshotsPref",
        task: () => {
          this._monitorScreenshotsPref();
        },
      },

      {
        name: "trackStartupCrashEndSetTimeout",
        task: () => {
          lazy.setTimeout(function () {
            Services.tm.idleDispatchToMainThread(
              Services.startup.trackStartupCrashEnd
            );
          }, STARTUP_CRASHES_END_DELAY_MS);
        },
      },

      {
        name: "handlerService.asyncInit",
        task: () => {
          let handlerService = Cc[
            "@mozilla.org/uriloader/handler-service;1"
          ].getService(Ci.nsIHandlerService);
          handlerService.asyncInit();
        },
      },

      {
        name: "webProtocolHandlerService.asyncInit",
        task: () => {
          lazy.WebProtocolHandlerRegistrar.prototype.init(true);
        },
      },

      {
        name: "RFPHelper.init",
        task: () => {
          lazy.RFPHelper.init();
        },
      },

      {
        name: "Blocklist.loadBlocklistAsync",
        task: () => {
          lazy.Blocklist.loadBlocklistAsync();
        },
      },

      {
        name: "TabUnloader.init",
        task: () => {
          lazy.TabUnloader.init();
        },
      },

      // Run TRR performance measurements for DoH.
      {
        name: "doh-rollout.trrRacer.run",
        task: () => {
          let enabledPref = "doh-rollout.trrRace.enabled";
          let completePref = "doh-rollout.trrRace.complete";

          if (Services.prefs.getBoolPref(enabledPref, false)) {
            if (!Services.prefs.getBoolPref(completePref, false)) {
              new lazy.TRRRacer().run(() => {
                Services.prefs.setBoolPref(completePref, true);
              });
            }
          } else {
            Services.prefs.addObserver(enabledPref, function observer() {
              if (Services.prefs.getBoolPref(enabledPref, false)) {
                Services.prefs.removeObserver(enabledPref, observer);

                if (!Services.prefs.getBoolPref(completePref, false)) {
                  new lazy.TRRRacer().run(() => {
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
        name: "initializeFOG",
        task: () => {
          Services.fog.initializeFOG();

          // Register Glean to listen for experiment updates releated to the
          // "gleanInternalSdk" feature defined in the t/c/nimbus/FeatureManifest.yaml
          // This feature is intended for internal Glean use only. For features wishing
          // to set a remote metric configuration, please use the "glean" feature for
          // the purpose of setting the data-control-plane features via Server Knobs.
          lazy.NimbusFeatures.gleanInternalSdk.onUpdate(() => {
            let cfg = lazy.NimbusFeatures.gleanInternalSdk.getVariable(
              "gleanMetricConfiguration"
            );
            Services.fog.applyServerKnobsConfig(JSON.stringify(cfg));
          });

          // Register Glean to listen for experiment updates releated to the
          // "glean" feature defined in the t/c/nimbus/FeatureManifest.yaml
          lazy.NimbusFeatures.glean.onUpdate(() => {
            let cfg = lazy.NimbusFeatures.glean.getVariable(
              "gleanMetricConfiguration"
            );
            Services.fog.applyServerKnobsConfig(JSON.stringify(cfg));
          });
        },
      },

      // Add the import button if this is the first startup.
      {
        name: "PlacesUIUtils.ImportButton",
        task: async () => {
          // First check if we've already added the import button, in which
          // case we should check for events indicating we can remove it.
          if (
            Services.prefs.getBoolPref(
              "browser.bookmarks.addedImportButton",
              false
            )
          ) {
            lazy.PlacesUIUtils.removeImportButtonWhenImportSucceeds();
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
            await lazy.PlacesUIUtils.maybeAddImportButton();
          }
        },
      },

      // Add the setup button if this is the first startup
      {
        name: "AWToolbarButton.SetupButton",
        task: async () => {
          if (
            // Not in automation: the button changes CUI state,
            // breaking tests. Check this first, so that the module
            // doesn't load if it doesn't have to.
            !Cu.isInAutomation &&
            lazy.AWToolbarButton.hasToolbarButtonEnabled
          ) {
            await lazy.AWToolbarButton.maybeAddSetupButton();
          }
        },
      },

      {
        name: "ASRouterNewTabHook.createInstance",
        task: () => {
          lazy.ASRouterNewTabHook.createInstance(lazy.ASRouterDefaultConfig());
        },
      },

      {
        name: "BackgroundUpdate",
        condition: AppConstants.MOZ_UPDATE_AGENT,
        task: async () => {
          // Never in automation!
          if (
            AppConstants.MOZ_UPDATER &&
            !lazy.UpdateServiceStub.updateDisabledForTesting
          ) {
            try {
              await lazy.BackgroundUpdate.scheduleFirefoxMessagingSystemTargetingSnapshotting();
            } catch (e) {
              console.error(
                "There was an error scheduling Firefox Messaging System targeting snapshotting: ",
                e
              );
            }
            await lazy.BackgroundUpdate.maybeScheduleBackgroundUpdateTask();
          }
        },
      },

      // Login detection service is used in fission to identify high value sites.
      {
        name: "LoginDetection.init",
        task: () => {
          let loginDetection = Cc[
            "@mozilla.org/login-detection-service;1"
          ].createInstance(Ci.nsILoginDetectionService);
          loginDetection.init();
        },
      },

      {
        name: "BrowserGlue._collectTelemetryPiPEnabled",
        task: () => {
          this._collectTelemetryPiPEnabled();
        },
      },
      // Schedule a sync (if enabled) after we've loaded
      {
        name: "WeaveService",
        task: async () => {
          if (lazy.WeaveService.enabled) {
            await lazy.WeaveService.whenLoaded();
            lazy.WeaveService.Weave.Service.scheduler.autoConnect();
          }
        },
      },

      {
        name: "unblock-untrusted-modules-thread",
        condition: AppConstants.platform == "win",
        task: () => {
          Services.obs.notifyObservers(
            null,
            "unblock-untrusted-modules-thread"
          );
        },
      },

      {
        name: "UpdateListener.maybeShowUnsupportedNotification",
        condition: AppConstants.MOZ_UPDATER,
        task: () => {
          lazy.UpdateListener.maybeShowUnsupportedNotification();
        },
      },

      {
        name: "QuickSuggest.init",
        task: () => {
          lazy.QuickSuggest.init();
        },
      },

      {
        name: "DAPTelemetrySender.startup",
        condition: lazy.TelemetryUtils.isTelemetryEnabled,
        task: async () => {
          await lazy.DAPTelemetrySender.startup();
        },
      },

      {
        name: "ShoppingUtils.init",
        task: () => {
          lazy.ShoppingUtils.init();
        },
      },

      {
        // Starts the JSOracle process for ORB JavaScript validation, if it hasn't started already.
        name: "start-orb-javascript-oracle",
        task: () => {
          ChromeUtils.ensureJSOracleStarted();
        },
      },

      {
        name: "SearchSERPCategorization.init",
        task: () => {
          lazy.SearchSERPCategorization.init();
        },
      },

      {
        name: "ContentRelevancyManager.init",
        task: () => {
          lazy.ContentRelevancyManager.init();
        },
      },

      {
        name: "BackupService initialization",
        condition: Services.prefs.getBoolPref("browser.backup.enabled", false),
        task: () => {
          lazy.BackupService.init();
        },
      },

      {
        name: "browser-startup-idle-tasks-finished",
        task: () => {
          // Use idleDispatch a second time to run this after the per-window
          // idle tasks.
          ChromeUtils.idleDispatch(() => {
            Services.obs.notifyObservers(
              null,
              "browser-startup-idle-tasks-finished"
            );
            BrowserInitState._resolveStartupIdleTask();
          });
        },
      },
      // Do NOT add anything after idle tasks finished.
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
              console.error(ex);
            } finally {
              ChromeUtils.addProfilerMarker(
                "startupIdleTask",
                startTime,
                task.name
              );
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
      function primaryPasswordTelemetry() {
        // Telemetry for primary-password - we do this after a delay as it
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

      function GMPInstallManagerSimpleCheckAndInstall() {
        let { GMPInstallManager } = ChromeUtils.importESModule(
          "resource://gre/modules/GMPInstallManager.sys.mjs"
        );
        this._gmpInstallManager = new GMPInstallManager();
        // We don't really care about the results, if someone is interested they
        // can check the log.
        this._gmpInstallManager.simpleCheckAndInstall().catch(() => {});
      }.bind(this),

      function RemoteSettingsInit() {
        lazy.RemoteSettings.init();
        this._addBreachesSyncHandler();
      }.bind(this),

      function PublicSuffixListInit() {
        lazy.PublicSuffixList.init();
      },

      function RemoteSecuritySettingsInit() {
        lazy.RemoteSecuritySettings.init();
      },

      function BrowserUsageTelemetryReportProfileCount() {
        lazy.BrowserUsageTelemetry.reportProfileCount();
      },

      function reportAllowedAppSources() {
        lazy.OsEnvironment.reportAllowedAppSources();
      },

      function searchBackgroundChecks() {
        Services.search.runBackgroundChecks();
      },

      function reportInstallationTelemetry() {
        lazy.BrowserUsageTelemetry.reportInstallationTelemetry();
      },

      function trustObjectTelemetry() {
        let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
          Ci.nsIX509CertDB
        );
        // countTrustObjects also logs the number of trust objects for telemetry purposes
        certdb.countTrustObjects();
      },
    ];

    for (let task of idleTasks) {
      ChromeUtils.idleDispatch(async () => {
        if (!Services.startup.shuttingDown) {
          let startTime = Cu.now();
          try {
            await task();
          } catch (ex) {
            console.error(ex);
          } finally {
            ChromeUtils.addProfilerMarker(
              "startupLateIdleTask",
              startTime,
              task.name
            );
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
      lazy
        .RemoteSettings(lazy.LoginBreaches.REMOTE_SETTINGS_COLLECTION)
        .on("sync", async event => {
          await lazy.LoginBreaches.update(event.data.current);
        });
    }
  },

  _addBreachAlertsPrefObserver() {
    const BREACH_ALERTS_PREF = "signon.management.page.breach-alerts.enabled";
    const clearVulnerablePasswordsIfBreachAlertsDisabled = async function () {
      if (!Services.prefs.getBoolPref(BREACH_ALERTS_PREF)) {
        await lazy.LoginBreaches.clearAllPotentiallyVulnerablePasswords();
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

    let windowcount = 0;
    let pagecount = 0;
    let pinnedcount = 0;
    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
      if (win.closed) {
        continue;
      }
      windowcount++;
      let tabbrowser = win.gBrowser;
      if (tabbrowser) {
        pinnedcount += tabbrowser._numPinnedTabs;
        pagecount += tabbrowser.visibleTabs.length - tabbrowser._numPinnedTabs;
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

    let win = lazy.BrowserWindowTracker.getTopWindow();

    // Our prompt for quitting is most important, so replace others.
    win.gDialogBox.replaceDialogIfOpen();

    let titleId, buttonLabelId;
    if (windowcount > 1) {
      // More than 1 window. Compose our own message.
      titleId = {
        id: "tabbrowser-confirm-close-windows-title",
        args: { windowCount: windowcount },
      };
      buttonLabelId = "tabbrowser-confirm-close-windows-button";
    } else if (shouldWarnForShortcut) {
      titleId = "tabbrowser-confirm-close-tabs-with-key-title";
      buttonLabelId = "tabbrowser-confirm-close-tabs-with-key-button";
    } else {
      titleId = {
        id: "tabbrowser-confirm-close-tabs-title",
        args: { tabCount: pagecount },
      };
      buttonLabelId = "tabbrowser-confirm-close-tabs-button";
    }

    // The checkbox label is different depending on whether the shortcut
    // was used to quit or not.
    let checkboxLabelId;
    if (shouldWarnForShortcut) {
      const quitKeyElement = win.document.getElementById("key_quitApplication");
      const quitKey = lazy.ShortcutUtils.prettifyShortcut(quitKeyElement);
      checkboxLabelId = {
        id: "tabbrowser-confirm-close-tabs-with-key-checkbox",
        args: { quitKey },
      };
    } else {
      checkboxLabelId = "tabbrowser-confirm-close-tabs-checkbox";
    }

    const [title, buttonLabel, checkboxLabel] =
      win.gBrowser.tabLocalization.formatMessagesSync([
        titleId,
        buttonLabelId,
        checkboxLabelId,
      ]);

    let warnOnClose = { value: true };
    let flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1;
    // buttonPressed will be 0 for closing, 1 for cancel (don't close/quit)
    let buttonPressed = Services.prompt.confirmEx(
      win,
      title.value,
      null,
      flags,
      buttonLabel.value,
      null,
      null,
      checkboxLabel.value,
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
    let dbStatus = lazy.PlacesUtils.history.databaseStatus;

    // Show a notification with a "more info" link for a locked places.sqlite.
    if (dbStatus == lazy.PlacesUtils.history.DATABASE_STATUS_LOCKED) {
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
      (dbStatus == lazy.PlacesUtils.history.DATABASE_STATUS_CREATE ||
        dbStatus == lazy.PlacesUtils.history.DATABASE_STATUS_CORRUPT);

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
      // Sqlite.sys.mjs and Places shutdown happen at profile-before-change, thus,
      // to be on the safe side, this should run earlier.
      lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
        "Places: export bookmarks.html",
        () =>
          lazy.BookmarkHTMLUtils.exportToFile(
            lazy.BookmarkHTMLUtils.defaultPath
          )
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
        let lastBackupFile = await lazy.PlacesBackups.getMostRecentBackup();
        if (lastBackupFile) {
          // restore from JSON backup
          await lazy.BookmarkJSONUtils.importFromFile(lastBackupFile, {
            replace: true,
            source: lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
          });
          importBookmarks = false;
        } else {
          // We have created a new database but we don't have any backup available
          importBookmarks = true;
          if (await IOUtils.exists(lazy.BookmarkHTMLUtils.defaultPath)) {
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
          console.error(e);
        }
      } else {
        // An import operation is about to run.
        let bookmarksUrl = null;
        if (restoreDefaultBookmarks) {
          // User wants to restore the default set of bookmarks shipped with the
          // browser, those that new profiles start with.
          bookmarksUrl = "chrome://browser/content/default-bookmarks.html";
        } else if (await IOUtils.exists(lazy.BookmarkHTMLUtils.defaultPath)) {
          bookmarksUrl = PathUtils.toFileURI(
            lazy.BookmarkHTMLUtils.defaultPath
          );
        }

        if (bookmarksUrl) {
          // Import from bookmarks.html file.
          try {
            if (
              Services.policies.isAllowed("defaultBookmarks") &&
              // Default bookmarks are imported after startup, and they may
              // influence the outcome of tests, thus it's possible to use
              // this test-only pref to skip the import.
              !(
                Cu.isInAutomation &&
                Services.prefs.getBoolPref(
                  "browser.bookmarks.testing.skipDefaultBookmarksImport",
                  false
                )
              )
            ) {
              await lazy.BookmarkHTMLUtils.importFromURL(bookmarksUrl, {
                replace: true,
                source: lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
              });
            }
          } catch (e) {
            console.error("Bookmarks.html file could be corrupt. ", e);
          }
          try {
            // Now apply distribution customized bookmarks.
            // This should always run after Places initialization.
            await this._distributionCustomizer.applyBookmarks();
          } catch (e) {
            console.error(e);
          }
        } else {
          console.error(new Error("Unable to find bookmarks.html file."));
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
      if (!(await lazy.PlacesBackups.hasRecentBackup())) {
        idleTime /= 2;
      }
      this._userIdleService.addIdleObserver(this, idleTime);
      this._bookmarksBackupIdleTime = idleTime;

      if (this._isNewProfile) {
        // New profiles may have existing bookmarks (imported from another browser or
        // copied into the profile) and we want to show the bookmark toolbar for them
        // in some cases.
        await lazy.PlacesUIUtils.maybeToggleBookmarkToolbarVisibility();
      }
    })()
      .catch(ex => {
        console.error(ex);
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
    return (async function () {
      let lastBackupFile = await lazy.PlacesBackups.getMostRecentBackup();
      // Should backup bookmarks if there are no backups or the maximum
      // interval between backups elapsed.
      if (
        !lastBackupFile ||
        new Date() - lazy.PlacesBackups.getDateForFile(lastBackupFile) >
          BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS * 86400000
      ) {
        let maxBackups = Services.prefs.getIntPref(
          "browser.bookmarks.max_backups"
        );
        await lazy.PlacesBackups.create(maxBackups);
      }
    })();
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  _showPlacesLockedNotificationBox:
    async function BG__showPlacesLockedNotificationBox() {
      var win = lazy.BrowserWindowTracker.getTopWindow();
      var buttons = [{ supportPage: "places-locked" }];

      var notifyBox = win.gBrowser.getNotificationBox();
      var notification = await notifyBox.appendNotification(
        "places-locked",
        {
          label: { "l10n-id": "places-locked-prompt" },
          priority: win.gNotificationBox.PRIORITY_CRITICAL_MEDIUM,
        },
        buttons
      );
      notification.persistence = -1; // Until user closes it
    },

  _onThisDeviceConnected() {
    const [title, body] = lazy.accountsL10n.formatValuesSync([
      "account-connection-title-2",
      "account-connection-connected",
    ]);

    let clickCallback = (subject, topic) => {
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
        let hashedId = lazy.PlacesUIUtils.obfuscateUrlForXulStore(id);
        Services.xulStore.setValue(docUrl, hashedId, "open", "true");
      });
  },

  // eslint-disable-next-line complexity
  _migrateUI() {
    // Use an increasing number to keep track of the current migration state.
    // Completely unrelated to the current Firefox release number.
    const UI_VERSION = 148;
    const BROWSER_DOCURL = AppConstants.BROWSER_CHROME_URL;

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
          const { ProfilerMenuButton } = ChromeUtils.importESModule(
            "resource://devtools/client/performance-new/popup/menu-button.sys.mjs"
          );
          if (!ProfilerMenuButton.isInNavbar()) {
            // The profiler menu button is not enabled. Turn it on now.
            const win = lazy.BrowserWindowTracker.getTopWindow();
            if (win && win.document) {
              ProfilerMenuButton.addToNavbar(win.document);
            }
          }
        }
      }

      let addonPromise;
      try {
        addonPromise = lazy.AddonManager.getAddonByID(
          "geckoprofiler@mozilla.com"
        );
      } catch (error) {
        console.error(
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
          .catch(console.error)
          .then(() => enableProfilerButton(wasAddonActive))
          .catch(console.error);
      }, console.error);
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
      const { CustomizableUI } = ChromeUtils.importESModule(
        "resource:///modules/CustomizableUI.sys.mjs"
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
      lazy.UrlbarPrefs.initializeShowSearchSuggestionsFirstPref();
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

    if (currentUIVersion < 122) {
      // Migrate xdg-desktop-portal pref from old to new prefs.
      try {
        const oldPref = "widget.use-xdg-desktop-portal";
        if (Services.prefs.getBoolPref(oldPref)) {
          Services.prefs.setIntPref(
            "widget.use-xdg-desktop-portal.file-picker",
            1
          );
          Services.prefs.setIntPref(
            "widget.use-xdg-desktop-portal.mime-handler",
            1
          );
        }
        Services.prefs.clearUserPref(oldPref);
      } catch (ex) {}
    }

    // Bug 1745248: Due to multiple backouts, do not use UI Version 123
    // as this version is most likely set for the Nightly channel

    if (currentUIVersion < 124) {
      // Migrate "extensions.formautofill.available" and
      // "extensions.formautofill.creditCards.available" from old to new prefs
      const oldFormAutofillModule = "extensions.formautofill.available";
      const oldCreditCardsAvailable =
        "extensions.formautofill.creditCards.available";
      const newCreditCardsAvailable =
        "extensions.formautofill.creditCards.supported";
      const newAddressesAvailable =
        "extensions.formautofill.addresses.supported";
      if (Services.prefs.prefHasUserValue(oldFormAutofillModule)) {
        let moduleAvailability = Services.prefs.getCharPref(
          oldFormAutofillModule
        );
        if (moduleAvailability == "on") {
          Services.prefs.setCharPref(newAddressesAvailable, moduleAvailability);
          Services.prefs.setCharPref(
            newCreditCardsAvailable,
            Services.prefs.getBoolPref(oldCreditCardsAvailable) ? "on" : "off"
          );
        }

        if (moduleAvailability == "off") {
          Services.prefs.setCharPref(
            newCreditCardsAvailable,
            moduleAvailability
          );
          Services.prefs.setCharPref(newAddressesAvailable, moduleAvailability);
        }
      }

      // after migrating, clear old prefs so we can remove them later.
      Services.prefs.clearUserPref(oldFormAutofillModule);
      Services.prefs.clearUserPref(oldCreditCardsAvailable);
    }

    if (currentUIVersion < 125) {
      // Bug 1756243 - Clear PiP cached coordinates since we changed their
      // coordinate space.
      const PIP_PLAYER_URI =
        "chrome://global/content/pictureinpicture/player.xhtml";
      try {
        for (let value of ["left", "top", "width", "height"]) {
          Services.xulStore.removeValue(
            PIP_PLAYER_URI,
            "picture-in-picture",
            value
          );
        }
      } catch (ex) {
        console.error("Failed to clear XULStore PiP values: ", ex);
      }
    }

    function migrateXULAttributeToStyle(url, id, attr) {
      try {
        let value = Services.xulStore.getValue(url, id, attr);
        if (value) {
          Services.xulStore.setValue(url, id, "style", `${attr}: ${value}px;`);
        }
      } catch (ex) {
        console.error(`Error migrating ${id}'s ${attr} value: `, ex);
      }
    }

    // Bug 1792748 used version 129 with a buggy variant of the sidebar width
    // migration. This version is already in use in the nightly channel, so it
    // shouldn't be used.

    // Bug 1793366: migrate sidebar persisted attribute from width to style.
    if (currentUIVersion < 130) {
      migrateXULAttributeToStyle(BROWSER_DOCURL, "sidebar-box", "width");
    }

    // Migration 131 was moved to 133 to allow for an uplift.

    if (currentUIVersion < 132) {
      // These attributes are no longer persisted, thus remove them from xulstore.
      for (let url of [
        "chrome://browser/content/places/bookmarkProperties.xhtml",
        "chrome://browser/content/places/bookmarkProperties2.xhtml",
      ]) {
        for (let attr of ["width", "screenX", "screenY"]) {
          xulStore.removeValue(url, "bookmarkproperties", attr);
        }
      }
    }

    if (currentUIVersion < 133) {
      xulStore.removeValue(BROWSER_DOCURL, "urlbar-container", "width");
    }

    // Migration 134 was removed because it was no longer necessary.

    if (currentUIVersion < 135 && AppConstants.platform == "linux") {
      // Avoid changing titlebar setting for users that used to had it off.
      try {
        if (!Services.prefs.prefHasUserValue("browser.tabs.inTitlebar")) {
          let de = Services.appinfo.desktopEnvironment;
          let oldDefault = de.includes("gnome") || de.includes("pantheon");
          if (!oldDefault) {
            Services.prefs.setIntPref("browser.tabs.inTitlebar", 0);
          }
        }
      } catch (e) {
        console.error("Error migrating tabsInTitlebar setting", e);
      }
    }

    if (currentUIVersion < 136) {
      migrateXULAttributeToStyle(
        "chrome://browser/content/places/places.xhtml",
        "placesList",
        "width"
      );
    }

    if (currentUIVersion < 137) {
      // The default value for enabling smooth scrolls is now false if the
      // user prefers reduced motion. If the value was previously set, do
      // not reset it, but if it was not explicitly set preserve the old
      // default value.
      if (
        !Services.prefs.prefHasUserValue("general.smoothScroll") &&
        Services.appinfo.prefersReducedMotion
      ) {
        Services.prefs.setBoolPref("general.smoothScroll", true);
      }
    }

    if (currentUIVersion < 138) {
      // Bug 1757297: Change scheme of all existing 'https-only-load-insecure'
      // permissions with https scheme to http scheme.
      try {
        Services.perms
          .getAllByTypes(["https-only-load-insecure"])
          .filter(permission => permission.principal.schemeIs("https"))
          .forEach(permission => {
            const capability = permission.capability;
            const uri = permission.principal.URI.mutate()
              .setScheme("http")
              .finalize();
            const principal =
              Services.scriptSecurityManager.createContentPrincipal(uri, {});
            Services.perms.removePermission(permission);
            Services.perms.addFromPrincipal(
              principal,
              "https-only-load-insecure",
              capability
            );
          });
      } catch (e) {
        console.error("Error migrating https-only-load-insecure permission", e);
      }
    }

    if (currentUIVersion < 139) {
      // Reset the default permissions to ALLOW_ACTION to rollback issues for
      // affected users, see Bug 1579517
      // originInfo in the format [origin, type]
      [
        ["https://www.mozilla.org", "uitour"],
        ["https://support.mozilla.org", "uitour"],
        ["about:home", "uitour"],
        ["about:newtab", "uitour"],
        ["https://addons.mozilla.org", "install"],
        ["https://support.mozilla.org", "remote-troubleshooting"],
        ["about:welcome", "autoplay-media"],
      ].forEach(originInfo => {
        // Reset permission on the condition that it is set to
        // UNKNOWN_ACTION, we want to prevent resetting user
        // manipulated permissions
        if (
          Services.perms.UNKNOWN_ACTION ==
          Services.perms.testPermissionFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              originInfo[0]
            ),
            originInfo[1]
          )
        ) {
          // Adding permissions which have default values does not create
          // new permissions, but rather remove the UNKNOWN_ACTION permission
          // overrides. User's not affected by Bug 1579517 will not be affected by this addition.
          Services.perms.addFromPrincipal(
            Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              originInfo[0]
            ),
            originInfo[1],
            Services.perms.ALLOW_ACTION
          );
        }
      });
    }

    if (currentUIVersion < 140) {
      // Remove browser.fixup.alternate.enabled pref in Bug 1850902.
      Services.prefs.clearUserPref("browser.fixup.alternate.enabled");
    }

    if (currentUIVersion < 141) {
      for (const filename of ["signons.sqlite", "signons.sqlite.corrupt"]) {
        const filePath = PathUtils.join(PathUtils.profileDir, filename);
        IOUtils.remove(filePath, { ignoreAbsent: true }).catch(console.error);
      }
    }

    if (currentUIVersion < 142) {
      // Bug 1860392 - Remove incorrectly persisted theming values from sidebar style.
      try {
        let value = xulStore.getValue(BROWSER_DOCURL, "sidebar-box", "style");
        if (value) {
          // Remove custom properties.
          value = value
            .split(";")
            .filter(v => !v.trim().startsWith("--"))
            .join(";");
          xulStore.setValue(BROWSER_DOCURL, "sidebar-box", "style", value);
        }
      } catch (ex) {
        console.error(ex);
      }
    }

    if (currentUIVersion < 143) {
      // Version 143 has been superseded by version 145 below.
    }

    if (currentUIVersion < 144) {
      // TerminatorTelemetry was removed in bug 1879136. Before it was removed,
      // the ShutdownDuration.json file would be written to disk at shutdown
      // so that the next launch of the browser could read it in and send
      // shutdown performance measurements.
      //
      // Unfortunately, this mechanism and its measurements were fairly
      // unreliable, so they were removed.
      for (const filename of [
        "ShutdownDuration.json",
        "ShutdownDuration.json.tmp",
      ]) {
        const filePath = PathUtils.join(PathUtils.profileDir, filename);
        IOUtils.remove(filePath, { ignoreAbsent: true }).catch(console.error);
      }
    }

    if (currentUIVersion < 145) {
      if (AppConstants.platform == "win") {
        // In Firefox 122, we enabled the firefox and firefox-private protocols.
        // We switched over to using firefox-bridge and firefox-private-bridge,
        // but we want to clean up the use of the other protocols.
        lazy.FirefoxBridgeExtensionUtils.maybeDeleteBridgeProtocolRegistryEntries(
          lazy.FirefoxBridgeExtensionUtils.OLD_PUBLIC_PROTOCOL,
          lazy.FirefoxBridgeExtensionUtils.OLD_PRIVATE_PROTOCOL
        );

        // Clean up the old user prefs from FX 122
        Services.prefs.clearUserPref(
          "network.protocol-handler.external.firefox"
        );
        Services.prefs.clearUserPref(
          "network.protocol-handler.external.firefox-private"
        );

        // In Firefox 126, we switched over to using native messaging so the
        // protocols are no longer necessary even in firefox-bridge and
        // firefox-private-bridge form
        lazy.FirefoxBridgeExtensionUtils.maybeDeleteBridgeProtocolRegistryEntries(
          lazy.FirefoxBridgeExtensionUtils.PUBLIC_PROTOCOL,
          lazy.FirefoxBridgeExtensionUtils.PRIVATE_PROTOCOL
        );
        Services.prefs.clearUserPref(
          "network.protocol-handler.external.firefox-bridge"
        );
        Services.prefs.clearUserPref(
          "network.protocol-handler.external.firefox-private-bridge"
        );
        Services.prefs.clearUserPref("browser.shell.customProtocolsRegistered");
      }
    }

    // Version 146 had a typo issue and thus it has been replaced by 147.

    if (currentUIVersion < 147) {
      // We're securing the boolean prefs for OS Authentication.
      // This is achieved by converting them into a string pref and encrypting the values
      // stored inside it.

      if (!AppConstants.NIGHTLY_BUILD) {
        const hasRunBetaMigration = Services.prefs
          .getCharPref("browser.startup.homepage_override.mstone", "")
          .startsWith("127.0");

        // Version 146 UI migration wrote to a wrong `creditcards` pref when
        // the feature was disabled, instead it should have used `creditCards`.
        // The correct pref name is in AUTOFILL_CREDITCARDS_REAUTH_PREF.
        // Note that we only wrote prefs if the feature was disabled.
        let ccTypoDisabled = !lazy.FormAutofillUtils.getOSAuthEnabled(
          "extensions.formautofill.creditcards.reauth.optout"
        );
        let ccCorrectPrefDisabled = !lazy.FormAutofillUtils.getOSAuthEnabled(
          lazy.FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF
        );
        let ccPrevReauthPrefValue = Services.prefs.getBoolPref(
          "extensions.formautofill.reauth.enabled",
          false
        );

        let userHadEnabledCreditCardReauth =
          // If we've run beta migration, and neither typo nor correct pref
          // indicate disablement, the user enabled the pref:
          (hasRunBetaMigration && !ccTypoDisabled && !ccCorrectPrefDisabled) ||
          // Or if we never ran beta migration and the bool pref is set:
          ccPrevReauthPrefValue;

        lazy.FormAutofillUtils.setOSAuthEnabled(
          lazy.FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
          userHadEnabledCreditCardReauth
        );

        if (!hasRunBetaMigration) {
          const passwordsPrevReauthPrefValue = Services.prefs.getBoolPref(
            "signon.management.page.os-auth.enabled",
            false
          );
          lazy.LoginHelper.setOSAuthEnabled(
            lazy.LoginHelper.OS_AUTH_FOR_PASSWORDS_PREF,
            passwordsPrevReauthPrefValue
          );
        }
      }

      Services.prefs.clearUserPref("extensions.formautofill.reauth.enabled");
      Services.prefs.clearUserPref("signon.management.page.os-auth.enabled");
      Services.prefs.clearUserPref(
        "extensions.formautofill.creditcards.reauth.optout"
      );
    }

    if (currentUIVersion < 148) {
      // The Firefox Translations addon is now a built-in Firefox feature.
      let addonPromise;
      try {
        addonPromise = lazy.AddonManager.getAddonByID(
          "firefox-translations-addon@mozilla.org"
        );
      } catch (error) {
        // This always throws in xpcshell as the AddonManager is not initialized.
        if (!Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
          console.error(
            "Could not access the AddonManager to upgrade the profile."
          );
        }
      }
      addonPromise?.then(addon => addon?.uninstall()).catch(console.error);
    }

    // Update the migration version.
    Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  },

  async _showUpgradeDialog() {
    const data = await lazy.OnboardingMessageProvider.getUpgradeMessage();
    const { gBrowser } = lazy.BrowserWindowTracker.getTopWindow();

    // We'll be adding a new tab open the tab-modal dialog in.
    let tab;

    const upgradeTabsProgressListener = {
      onLocationChange(aBrowser) {
        if (aBrowser === tab.linkedBrowser) {
          lazy.setTimeout(() => {
            // We're now far enough along in the load that we no longer have to
            // worry about a call to onLocationChange triggering SubDialog.abort,
            // so display the dialog
            const config = {
              type: "SHOW_SPOTLIGHT",
              data,
            };
            lazy.SpecialMessageActions.handleAction(config, tab.linkedBrowser);

            gBrowser.removeTabsProgressListener(upgradeTabsProgressListener);
          }, 0);
        }
      },
    };

    // Make sure we're ready to show the dialog once onLocationChange gets
    // called.
    gBrowser.addTabsProgressListener(upgradeTabsProgressListener);

    tab = gBrowser.addTrustedTab("about:home", {
      relatedToCurrent: true,
    });

    gBrowser.selectedTab = tab;
  },

  async _showAboutWelcomeModal() {
    const { gBrowser } = lazy.BrowserWindowTracker.getTopWindow();
    const data = await lazy.NimbusFeatures.aboutwelcome.getAllVariables();

    const config = {
      type: "SHOW_SPOTLIGHT",
      data: {
        content: {
          template: "multistage",
          id: data?.id || "ABOUT_WELCOME_MODAL",
          backdrop: data?.backdrop,
          screens: data?.screens,
          UTMTerm: data?.UTMTerm,
        },
      },
    };

    lazy.SpecialMessageActions.handleAction(config, gBrowser);
  },

  async _maybeShowDefaultBrowserPrompt() {
    // Highest priority is about:welcome window modal experiment
    // Second highest priority is the upgrade dialog, which can include a "primary
    // browser" request and is limited in various ways, e.g., major upgrades.
    if (
      lazy.BrowserHandler.firstRunProfile &&
      lazy.NimbusFeatures.aboutwelcome.getVariable("showModal")
    ) {
      this._showAboutWelcomeModal();
      return;
    }
    const dialogVersion = 106;
    const dialogVersionPref = "browser.startup.upgradeDialog.version";
    const dialogReason = await (async () => {
      if (!lazy.BrowserHandler.majorUpgrade) {
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
      if (!defaultPrefs.getBoolPref("browser.aboutwelcome.enabled", true)) {
        return "no-welcome";
      }
      if (!Services.policies.isAllowed("postUpdateCustomPage")) {
        return "disallow-postUpdate";
      }

      const showUpgradeDialog =
        lazy.NimbusFeatures.upgradeDialog.getVariable("enabled");

      return showUpgradeDialog ? "" : "disabled";
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
      this._showUpgradeDialog();
      return;
    }

    const willPrompt = await DefaultBrowserCheck.willCheckDefaultBrowser(
      /* isStartupCheck */ true
    );
    if (willPrompt) {
      let win = lazy.BrowserWindowTracker.getTopWindow();
      DefaultBrowserCheck.prompt(win);
    } else if (await lazy.QuickSuggest.maybeShowOnboardingDialog()) {
      return;
    }

    await lazy.ASRouter.waitForInitialized;
    lazy.ASRouter.sendTriggerMessage({
      browser:
        lazy.BrowserWindowTracker.getTopWindow()?.gBrowser.selectedBrowser,
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

    const win = lazy.BrowserWindowTracker.getTopWindow();
    // We've restarted at least once; we will show the notification if possible.
    // We can't do that if there's no session to restore, or this is a private window.
    if (
      !lazy.SessionStore.canRestoreLastSession ||
      lazy.PrivateBrowsingUtils.isWindowPrivate(win)
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
    const notification = await notifyBox.appendNotification(
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
    let chromeWindow = lazy.BrowserWindowTracker.getTopWindow();
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
      let win = lazy.BrowserWindowTracker.getTopWindow({ private: false });

      const openTab = async URI => {
        let tab;
        if (!win) {
          win = await this._openURLInNewWindow(URI.uri);
          let tabs = win.gBrowser.tabs;
          tab = tabs[tabs.length - 1];
        } else {
          tab = win.gBrowser.addWebTab(URI.uri);
        }
        tab.attention = true;
        return tab;
      };

      const firstTab = await openTab(URIs[0]);
      await Promise.all(URIs.slice(1).map(URI => openTab(URI)));

      const deviceName = URIs[0].sender && URIs[0].sender.name;
      let titleL10nId, body;
      if (URIs.length == 1) {
        // Due to bug 1305895, tabs from iOS may not have device information, so
        // we have separate strings to handle those cases. (See Also
        // unnamedTabsArrivingNotificationNoDevice.body below)
        titleL10nId = deviceName
          ? {
              id: "account-single-tab-arriving-from-device-title",
              args: { deviceName },
            }
          : { id: "account-single-tab-arriving-title" };
        // Use the page URL as the body. We strip the fragment and query (after
        // the `?` and `#` respectively) to reduce size, and also format it the
        // same way that the url bar would.
        let url = URIs[0].uri.replace(/([?#]).*$/, "$1");
        const wasTruncated = url.length < URIs[0].uri.length;
        url = lazy.BrowserUIUtils.trimURL(url);
        if (wasTruncated) {
          body = await lazy.accountsL10n.formatValue(
            "account-single-tab-arriving-truncated-url",
            { url }
          );
        } else {
          body = url;
        }
      } else {
        titleL10nId = { id: "account-multiple-tabs-arriving-title" };
        const allKnownSender = URIs.every(URI => URI.sender != null);
        const allSameDevice =
          allKnownSender &&
          URIs.every(URI => URI.sender.id == URIs[0].sender.id);
        let bodyL10nId;
        if (allSameDevice) {
          bodyL10nId = deviceName
            ? "account-multiple-tabs-arriving-from-single-device"
            : "account-multiple-tabs-arriving-from-unknown-device";
        } else {
          bodyL10nId = "account-multiple-tabs-arriving-from-multiple-devices";
        }

        body = await lazy.accountsL10n.formatValue(bodyL10nId, {
          deviceName,
          tabCount: URIs.length,
        });
      }
      const title = await lazy.accountsL10n.formatValue(titleL10nId);

      const clickCallback = (obsSubject, obsTopic) => {
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
      console.error("Error displaying tab(s) received by Sync: ", ex);
    }
  },

  async _onIncomingCloseTabCommand(data) {
    // The payload is wrapped weirdly because of how Sync does notifications.
    const wrappedObj = data.wrappedJSObject.object;
    let { urls } = wrappedObj[0];
    let urisToClose = [];
    urls.forEach(urlString => {
      try {
        urisToClose.push(Services.io.newURI(urlString));
      } catch (ex) {
        // The url was invalid so we ignore
        console.error(ex);
      }
    });
    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
      // Ensure we're operating on fully opened browser windows
      if (!win.gBrowser) {
        continue;
      }
      urisToClose = await win.gBrowser.closeTabsByURI(urisToClose);
      // If we've successfully closed all the tabs, break early
      if (!urisToClose.length) {
        break;
      }
    }
  },

  async _onVerifyLoginNotification({ body, title, url }) {
    let tab;
    let imageURL;
    if (AppConstants.platform == "win") {
      imageURL = "chrome://branding/content/icon64.png";
    }
    let win = lazy.BrowserWindowTracker.getTopWindow({ private: false });
    if (!win) {
      win = await this._openURLInNewWindow(url);
      let tabs = win.gBrowser.tabs;
      tab = tabs[tabs.length - 1];
    } else {
      tab = win.gBrowser.addWebTab(url);
    }
    tab.attention = true;
    let clickCallback = (subject, topic) => {
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
      console.error("Error notifying of a verify login event: ", ex);
    }
  },

  _onDeviceConnected(deviceName) {
    const [title, body] = lazy.accountsL10n.formatValuesSync([
      { id: "account-connection-title-2" },
      deviceName
        ? { id: "account-connection-connected-with", args: { deviceName } }
        : { id: "account-connection-connected-with-noname" },
    ]);

    let clickCallback = async (subject, topic) => {
      if (topic != "alertclickcallback") {
        return;
      }
      let url = await lazy.FxAccounts.config.promiseManageDevicesURI(
        "device-connected-notification"
      );
      let win = lazy.BrowserWindowTracker.getTopWindow({ private: false });
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
      console.error("Error notifying of a new Sync device: ", ex);
    }
  },

  _onDeviceDisconnected() {
    const [title, body] = lazy.accountsL10n.formatValuesSync([
      "account-connection-title-2",
      "account-connection-disconnected",
    ]);

    let clickCallback = (subject, topic) => {
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

  _updateFxaBadges(win) {
    let fxaButton = win.document.getElementById("fxa-toolbar-menu-button");
    let badge = fxaButton?.querySelector(".toolbarbutton-badge");

    let state = lazy.UIState.get();
    if (
      state.status == lazy.UIState.STATUS_LOGIN_FAILED ||
      state.status == lazy.UIState.STATUS_NOT_VERIFIED
    ) {
      // If the fxa toolbar button is in the toolbox, we display the notification
      // on the fxa button instead of the app menu.
      let navToolbox = win.document.getElementById("navigator-toolbox");
      let isFxAButtonShown = navToolbox.contains(fxaButton);
      if (isFxAButtonShown) {
        state.status == lazy.UIState.STATUS_LOGIN_FAILED
          ? fxaButton?.setAttribute("badge-status", state.status)
          : badge?.classList.add("feature-callout");
      } else {
        lazy.AppMenuNotifications.showBadgeOnlyNotification(
          "fxa-needs-authentication"
        );
      }
    } else {
      fxaButton?.removeAttribute("badge-status");
      badge?.classList.remove("feature-callout");
      lazy.AppMenuNotifications.removeNotification("fxa-needs-authentication");
    }
  },

  _collectTelemetryPiPEnabled() {
    Services.telemetry.setEventRecordingEnabled(
      "pictureinpicture.settings",
      true
    );
    Services.telemetry.setEventRecordingEnabled("pictureinpicture", true);

    const TOGGLE_ENABLED_PREF =
      "media.videocontrols.picture-in-picture.video-toggle.enabled";

    const observe = (subject, topic) => {
      const enabled = Services.prefs.getBoolPref(TOGGLE_ENABLED_PREF, false);
      Services.telemetry.scalarSet("pictureinpicture.toggle_enabled", enabled);

      // Record events when preferences change
      if (topic === "nsPref:changed") {
        if (enabled) {
          Services.telemetry.recordEvent(
            "pictureinpicture.settings",
            "enable",
            "settings"
          );
        }
      }
    };

    Services.prefs.addObserver(TOGGLE_ENABLED_PREF, observe);
    observe();
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
        "privacy.trackingprotection.emailtracking.enabled": null,
        "privacy.trackingprotection.emailtracking.pbmode.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation":
          null,
        "privacy.partition.network_state.ocsp_cache": null,
        "privacy.query_stripping.enabled": null,
        "privacy.query_stripping.enabled.pbmode": null,
        "privacy.fingerprintingProtection": null,
        "privacy.fingerprintingProtection.pbmode": null,
      },
      standard: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.trackingprotection.emailtracking.enabled": null,
        "privacy.trackingprotection.emailtracking.pbmode.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation":
          null,
        "privacy.partition.network_state.ocsp_cache": null,
        "privacy.query_stripping.enabled": null,
        "privacy.query_stripping.enabled.pbmode": null,
        "privacy.fingerprintingProtection": null,
        "privacy.fingerprintingProtection.pbmode": null,
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
        case "emailTP":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.enabled"
          ] = true;
          break;
        case "-emailTP":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.enabled"
          ] = false;
          break;
        case "emailTPPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.pbmode.enabled"
          ] = true;
          break;
        case "-emailTPPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.pbmode.enabled"
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
        case "rpTop":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation"
          ] = true;
          break;
        case "-rpTop":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation"
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
        case "qps":
          this.CATEGORY_PREFS[type]["privacy.query_stripping.enabled"] = true;
          break;
        case "-qps":
          this.CATEGORY_PREFS[type]["privacy.query_stripping.enabled"] = false;
          break;
        case "qpsPBM":
          this.CATEGORY_PREFS[type][
            "privacy.query_stripping.enabled.pbmode"
          ] = true;
          break;
        case "-qpsPBM":
          this.CATEGORY_PREFS[type][
            "privacy.query_stripping.enabled.pbmode"
          ] = false;
          break;
        case "fpp":
          this.CATEGORY_PREFS[type]["privacy.fingerprintingProtection"] = true;
          break;
        case "-fpp":
          this.CATEGORY_PREFS[type]["privacy.fingerprintingProtection"] = false;
          break;
        case "fppPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.fingerprintingProtection.pbmode"
          ] = true;
          break;
        case "-fppPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.fingerprintingProtection.pbmode"
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
          console.error(`Error: Unknown rule observed ${item}`);
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
 * This override ability is provided by Integration.sys.mjs. See
 * PermissionUI.sys.mjs for an example of how to provide a new prompt
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
   * @return {PermissionPrompt} (see PermissionUI.sys.mjs),
   *         or undefined if the type cannot be handled.
   */
  createPermissionPrompt(type, request) {
    switch (type) {
      case "geolocation": {
        return new lazy.PermissionUI.GeolocationPermissionPrompt(request);
      }
      case "xr": {
        return new lazy.PermissionUI.XRPermissionPrompt(request);
      }
      case "desktop-notification": {
        return new lazy.PermissionUI.DesktopNotificationPermissionPrompt(
          request
        );
      }
      case "persistent-storage": {
        return new lazy.PermissionUI.PersistentStoragePermissionPrompt(request);
      }
      case "midi": {
        return new lazy.PermissionUI.MIDIPermissionPrompt(request);
      }
      case "storage-access": {
        return new lazy.PermissionUI.StorageAccessPermissionPrompt(request);
      }
    }
    return undefined;
  },
};

export function ContentPermissionPrompt() {}

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
      let combinedIntegration = lazy.Integration.contentPermission.getCombined(
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
      console.error(ex);
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
        console.error(ex);
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

export var DefaultBrowserCheck = {
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
      try {
        await shellService.setAsDefault();
      } catch (e) {
        this.log.error("Failed to set the default browser", e);
      }

      shellService.pinToTaskbar();
    }
    if (checkboxState) {
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
    let win = lazy.BrowserWindowTracker.getTopWindow();
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
    await lazy.SessionStartup.onceInitialized;
    let willRecoverSession =
      lazy.SessionStartup.sessionType == lazy.SessionStartup.RECOVER_SESSION;

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

/**
 * AboutHomeStartupCache is responsible for reading and writing the
 * initial about:home document from the HTTP cache as a startup
 * performance optimization. It only works when the "privileged about
 * content process" is enabled and when ENABLED_PREF is set to true.
 *
 * See https://firefox-source-docs.mozilla.org/browser/components/newtab/docs/v2-system-addon/about_home_startup_cache.html
 * for further details.
 */
export var AboutHomeStartupCache = {
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

    this._enabled = Services.prefs.getBoolPref(
      "browser.startup.homepage.abouthome_cache.enabled"
    );

    if (!this._enabled) {
      this.recordResult(this.CACHE_RESULT_SCALARS.DISABLED);
      return;
    }

    this.log = console.createInstance({
      prefix: this.LOG_NAME,
      maxLogLevelPref: this.LOG_LEVEL_PREF,
    });

    this.log.trace("Initting.");

    // If the user is not configured to load about:home at startup, then
    // let's not bother with the cache - loading it needlessly is more likely
    // to hinder what we're actually trying to load.
    let willLoadAboutHome =
      !lazy.HomePage.overridden &&
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

    this._cacheTask = new lazy.DeferredTask(async () => {
      await this.cacheNow();
    }, this.CACHE_DEBOUNCE_RATE_MS);

    lazy.AsyncShutdown.quitApplicationGranted.addBlocker(
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
      const TIMED_OUT = Symbol();
      let timeoutID = 0;

      let timeoutPromise = new Promise(resolve => {
        timeoutID = lazy.setTimeout(
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
      lazy.clearTimeout(timeoutID);
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

    if (
      this._procManager.remoteType != lazy.E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE
    ) {
      this.log.error("Somehow got the wrong process type.");
      return { pageInputStream: null, scriptInputStream: null };
    }

    let state = lazy.AboutNewTab.activityStream.store.getState();
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
    lazy.NetUtil.asyncCopy(
      cachePageInputStream,
      this.pagePipe.outputStream,
      () => {
        this.log.info("Page stream connected to pipe.");
      }
    );

    let cacheScriptInputStream;
    try {
      this.log.trace("Connecting script stream to pipe.");
      cacheScriptInputStream =
        this._cacheEntry.openAlternativeInputStream("script");
      lazy.NetUtil.asyncCopy(
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
      lazy.NetUtil.asyncCopy(pageInputStream, pageOutputStream, pageResult => {
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
        lazy.NetUtil.asyncCopy(
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
    if (procManager.remoteType == lazy.E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE) {
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
    if (
      message.target.remoteType != lazy.E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE
    ) {
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

  onCacheEntryCheck() {
    return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
  },

  onCacheEntryAvailable(aEntry) {
    this.log.trace("Cache entry is available.");

    this._cacheEntry = aEntry;
    this.connectToPipes();
    this._cacheEntryResolver(this._cacheEntry);
  },
};
