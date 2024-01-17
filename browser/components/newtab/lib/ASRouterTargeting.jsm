/* This Source Code Form is subject to the terms of the Mozilla PublicddonMa
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const FXA_ENABLED_PREF = "identity.fxaccounts.enabled";
const DISTRIBUTION_ID_PREF = "distribution.id";
const DISTRIBUTION_ID_CHINA_REPACK = "MozillaOnline";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { NewTabUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/NewTabUtils.sys.mjs"
);
const { ShellService } = ChromeUtils.importESModule(
  "resource:///modules/ShellService.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
  AttributionCode: "resource:///modules/AttributionCode.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.sys.mjs",
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  HomePage: "resource:///modules/HomePage.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  ProfileAge: "resource://gre/modules/ProfileAge.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
  TargetingContext: "resource://messaging-system/targeting/Targeting.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  TelemetrySession: "resource://gre/modules/TelemetrySession.sys.mjs",
  WindowsLaunchOnLogin: "resource://gre/modules/WindowsLaunchOnLogin.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ASRouterPreferences: "resource://activity-stream/lib/ASRouterPreferences.jsm",
});

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "cfrFeaturesUserPref",
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "cfrAddonsUserPref",
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isWhatsNewPanelEnabled",
  "browser.messaging-system.whatsNewPanel.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hasAccessedFxAPanel",
  "identity.fxaccounts.toolbar.accessed",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "clientsDevicesDesktop",
  "services.sync.clients.devices.desktop",
  0
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "clientsDevicesMobile",
  "services.sync.clients.devices.mobile",
  0
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "syncNumClients",
  "services.sync.numClients",
  0
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "devtoolsSelfXSSCount",
  "devtools.selfxss.count",
  0
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isFxAEnabled",
  FXA_ENABLED_PREF,
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isXPIInstallEnabled",
  "xpinstall.enabled",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hasMigratedBookmarks",
  "browser.migrate.interactions.bookmarks",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hasMigratedCSVPasswords",
  "browser.migrate.interactions.csvpasswords",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hasMigratedHistory",
  "browser.migrate.interactions.history",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hasMigratedPasswords",
  "browser.migrate.interactions.passwords",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "useEmbeddedMigrationWizard",
  "browser.migrate.content-modal.about-welcome-behavior",
  "default",
  null,
  behaviorString => {
    return behaviorString === "embedded";
  }
);

XPCOMUtils.defineLazyServiceGetters(lazy, {
  AUS: ["@mozilla.org/updates/update-service;1", "nsIApplicationUpdateService"],
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
  TrackingDBService: [
    "@mozilla.org/tracking-db-service;1",
    "nsITrackingDBService",
  ],
  UpdateCheckSvc: ["@mozilla.org/updates/update-checker;1", "nsIUpdateChecker"],
});

const FXA_USERNAME_PREF = "services.sync.username";

const { activityStreamProvider: asProvider } = NewTabUtils;

const FXA_ATTACHED_CLIENTS_UPDATE_INTERVAL = 4 * 60 * 60 * 1000; // Four hours
const FRECENT_SITES_UPDATE_INTERVAL = 6 * 60 * 60 * 1000; // Six hours
const FRECENT_SITES_IGNORE_BLOCKED = false;
const FRECENT_SITES_NUM_ITEMS = 25;
const FRECENT_SITES_MIN_FRECENCY = 100;

const CACHE_EXPIRATION = 5 * 60 * 1000;
const jexlEvaluationCache = new Map();

/**
 * CachedTargetingGetter
 * @param property {string} Name of the method
 * @param options {any=} Options passed to the method
 * @param updateInterval {number?} Update interval for query. Defaults to FRECENT_SITES_UPDATE_INTERVAL
 */
function CachedTargetingGetter(
  property,
  options = null,
  updateInterval = FRECENT_SITES_UPDATE_INTERVAL,
  getter = asProvider
) {
  return {
    _lastUpdated: 0,
    _value: null,
    // For testing
    expire() {
      this._lastUpdated = 0;
      this._value = null;
    },
    async get() {
      const now = Date.now();
      if (now - this._lastUpdated >= updateInterval) {
        this._value = await getter[property](options);
        this._lastUpdated = now;
      }
      return this._value;
    },
  };
}

function CacheListAttachedOAuthClients() {
  return {
    _lastUpdated: 0,
    _value: null,
    expire() {
      this._lastUpdated = 0;
      this._value = null;
    },
    get() {
      const now = Date.now();
      if (now - this._lastUpdated >= FXA_ATTACHED_CLIENTS_UPDATE_INTERVAL) {
        this._value = new Promise(resolve => {
          lazy.fxAccounts
            .listAttachedOAuthClients()
            .then(clients => {
              resolve(clients);
            })
            .catch(() => resolve([]));
        });
        this._lastUpdated = now;
      }
      return this._value;
    },
  };
}

function CheckBrowserNeedsUpdate(
  updateInterval = FRECENT_SITES_UPDATE_INTERVAL
) {
  const checker = {
    _lastUpdated: 0,
    _value: null,
    // For testing. Avoid update check network call.
    setUp(value) {
      this._lastUpdated = Date.now();
      this._value = value;
    },
    expire() {
      this._lastUpdated = 0;
      this._value = null;
    },
    async get() {
      const now = Date.now();
      if (
        !AppConstants.MOZ_UPDATER ||
        now - this._lastUpdated < updateInterval
      ) {
        return this._value;
      }
      if (!lazy.AUS.canCheckForUpdates) {
        return false;
      }
      this._lastUpdated = now;
      let check = lazy.UpdateCheckSvc.checkForUpdates(
        lazy.UpdateCheckSvc.FOREGROUND_CHECK
      );
      let result = await check.result;
      if (!result.succeeded) {
        lazy.ASRouterPreferences.console.error(
          "CheckBrowserNeedsUpdate failed :>> ",
          result.request
        );
        return false;
      }
      checker._value = !!result.updates.length;
      return checker._value;
    },
  };

  return checker;
}

const QueryCache = {
  expireAll() {
    Object.keys(this.queries).forEach(query => {
      this.queries[query].expire();
    });
    Object.keys(this.getters).forEach(key => {
      this.getters[key].expire();
    });
  },
  queries: {
    TopFrecentSites: new CachedTargetingGetter("getTopFrecentSites", {
      ignoreBlocked: FRECENT_SITES_IGNORE_BLOCKED,
      numItems: FRECENT_SITES_NUM_ITEMS,
      topsiteFrecency: FRECENT_SITES_MIN_FRECENCY,
      onePerDomain: true,
      includeFavicon: false,
    }),
    TotalBookmarksCount: new CachedTargetingGetter("getTotalBookmarksCount"),
    CheckBrowserNeedsUpdate: new CheckBrowserNeedsUpdate(),
    RecentBookmarks: new CachedTargetingGetter("getRecentBookmarks"),
    ListAttachedOAuthClients: new CacheListAttachedOAuthClients(),
    UserMonthlyActivity: new CachedTargetingGetter("getUserMonthlyActivity"),
  },
  getters: {
    doesAppNeedPin: new CachedTargetingGetter(
      "doesAppNeedPin",
      null,
      FRECENT_SITES_UPDATE_INTERVAL,
      ShellService
    ),
    doesAppNeedPrivatePin: new CachedTargetingGetter(
      "doesAppNeedPin",
      true,
      FRECENT_SITES_UPDATE_INTERVAL,
      ShellService
    ),
    isDefaultBrowser: new CachedTargetingGetter(
      "isDefaultBrowser",
      null,
      FRECENT_SITES_UPDATE_INTERVAL,
      ShellService
    ),
    currentThemes: new CachedTargetingGetter(
      "getAddonsByTypes",
      ["theme"],
      FRECENT_SITES_UPDATE_INTERVAL,
      lazy.AddonManager // eslint-disable-line mozilla/valid-lazy
    ),
    isDefaultHTMLHandler: new CachedTargetingGetter(
      "isDefaultHandlerFor",
      [".html"],
      FRECENT_SITES_UPDATE_INTERVAL,
      ShellService
    ),
    isDefaultPDFHandler: new CachedTargetingGetter(
      "isDefaultHandlerFor",
      [".pdf"],
      FRECENT_SITES_UPDATE_INTERVAL,
      ShellService
    ),
    defaultPDFHandler: new CachedTargetingGetter(
      "getDefaultPDFHandler",
      null,
      FRECENT_SITES_UPDATE_INTERVAL,
      ShellService
    ),
  },
};

/**
 * sortMessagesByWeightedRank
 *
 * Each message has an associated weight, which is guaranteed to be strictly
 * positive. Sort the messages so that higher weighted messages are more likely
 * to come first.
 *
 * Specifically, sort them so that the probability of message x_1 with weight
 * w_1 appearing before message x_2 with weight w_2 is (w_1 / (w_1 + w_2)).
 *
 * This is equivalent to requiring that x_1 appearing before x_2 is (w_1 / w_2)
 * "times" as likely as x_2 appearing before x_1.
 *
 * See Bug 1484996, Comment 2 for a justification of the method.
 *
 * @param {Array} messages - A non-empty array of messages to sort, all with
 *                           strictly positive weights
 * @returns the sorted array
 */
function sortMessagesByWeightedRank(messages) {
  return messages
    .map(message => ({
      message,
      rank: Math.pow(Math.random(), 1 / message.weight),
    }))
    .sort((a, b) => b.rank - a.rank)
    .map(({ message }) => message);
}

/**
 * getSortedMessages - Given an array of Messages, applies sorting and filtering rules
 *                     in expected order.
 *
 * @param {Array<Message>} messages
 * @param {{}} options
 * @param {boolean} options.ordered - Should .order be used instead of random weighted sorting?
 * @returns {Array<Message>}
 */
function getSortedMessages(messages, options = {}) {
  let { ordered } = { ordered: false, ...options };
  let result = messages;

  if (!ordered) {
    result = sortMessagesByWeightedRank(result);
  }

  result.sort((a, b) => {
    // Next, sort by priority
    if (a.priority > b.priority || (!isNaN(a.priority) && isNaN(b.priority))) {
      return -1;
    }
    if (a.priority < b.priority || (isNaN(a.priority) && !isNaN(b.priority))) {
      return 1;
    }

    // Sort messages with targeting expressions higher than those with none
    if (a.targeting && !b.targeting) {
      return -1;
    }
    if (!a.targeting && b.targeting) {
      return 1;
    }

    // Next, sort by order *ascending* if ordered = true
    if (ordered) {
      if (a.order > b.order || (!isNaN(a.order) && isNaN(b.order))) {
        return 1;
      }
      if (a.order < b.order || (isNaN(a.order) && !isNaN(b.order))) {
        return -1;
      }
    }

    return 0;
  });

  return result;
}

/**
 * parseAboutPageURL - Parse a URL string retrieved from about:home and about:new, returns
 *                    its type (web extenstion or custom url) and the parsed url(s)
 *
 * @param {string} url - A URL string for home page or newtab page
 * @returns {Object} {
 *   isWebExt: boolean,
 *   isCustomUrl: boolean,
 *   urls: Array<{url: string, host: string}>
 * }
 */
function parseAboutPageURL(url) {
  let ret = {
    isWebExt: false,
    isCustomUrl: false,
    urls: [],
  };
  if (url.startsWith("moz-extension://")) {
    ret.isWebExt = true;
    ret.urls.push({ url, host: "" });
  } else {
    // The home page URL could be either a single URL or a list of "|" separated URLs.
    // Note that it should work with "about:home" and "about:blank", in which case the
    // "host" is set as an empty string.
    for (const _url of url.split("|")) {
      if (!["about:home", "about:newtab", "about:blank"].includes(_url)) {
        ret.isCustomUrl = true;
      }
      try {
        const parsedURL = new URL(_url);
        const host = parsedURL.hostname.replace(/^www\./i, "");
        ret.urls.push({ url: _url, host });
      } catch (e) {}
    }
    // If URL parsing failed, just return the given url with an empty host
    if (!ret.urls.length) {
      ret.urls.push({ url, host: "" });
    }
  }

  return ret;
}

/**
 * Get the number of records in autofill storage, e.g. credit cards/addresses.
 *
 * @param  {Object} [data]
 * @param  {string} [data.collectionName]
 *         The name used to specify which collection to retrieve records.
 * @param  {string} [data.searchString]
 *         The typed string for filtering out the matched records.
 * @param  {string} [data.info]
 *         The input autocomplete property's information.
 * @returns {Promise<number>} The number of matched records.
 * @see FormAutofillParent._getRecords
 */
async function getAutofillRecords(data) {
  let actor;
  try {
    const win = Services.wm.getMostRecentBrowserWindow();
    actor =
      win.gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
        "FormAutofill"
      );
  } catch (error) {
    // If the actor is not available, we can't get the records. We could import
    // the records directly from FormAutofillStorage to avoid the messiness of
    // JSActors, but that would import a lot of code for a targeting attribute.
    return 0;
  }
  let records = await actor?.receiveMessage({
    name: "FormAutofill:GetRecords",
    data,
  });
  return records?.records?.length ?? 0;
}

// Attribution data can be encoded multiple times so we need this function to
// get a cleartext value.
function decodeAttributionValue(value) {
  if (!value) {
    return null;
  }

  let decodedValue = value;

  while (decodedValue.includes("%")) {
    try {
      const result = decodeURIComponent(decodedValue);
      if (result === decodedValue) {
        break;
      }
      decodedValue = result;
    } catch (e) {
      break;
    }
  }

  return decodedValue;
}

const TargetingGetters = {
  get locale() {
    return Services.locale.appLocaleAsBCP47;
  },
  get localeLanguageCode() {
    return (
      Services.locale.appLocaleAsBCP47 &&
      Services.locale.appLocaleAsBCP47.substr(0, 2)
    );
  },
  get browserSettings() {
    const { settings } = lazy.TelemetryEnvironment.currentEnvironment;
    return {
      update: settings.update,
    };
  },
  get attributionData() {
    // Attribution is determined at startup - so we can use the cached attribution at this point
    return lazy.AttributionCode.getCachedAttributionData();
  },
  get currentDate() {
    return new Date();
  },
  get profileAgeCreated() {
    return lazy.ProfileAge().then(times => times.created);
  },
  get profileAgeReset() {
    return lazy.ProfileAge().then(times => times.reset);
  },
  get usesFirefoxSync() {
    return Services.prefs.prefHasUserValue(FXA_USERNAME_PREF);
  },
  get isFxAEnabled() {
    return lazy.isFxAEnabled;
  },
  get isFxASignedIn() {
    return new Promise(resolve => {
      if (!lazy.isFxAEnabled) {
        resolve(false);
      }
      if (Services.prefs.getStringPref(FXA_USERNAME_PREF, "")) {
        resolve(true);
      }
      lazy.fxAccounts
        .getSignedInUser()
        .then(data => resolve(!!data))
        .catch(e => resolve(false));
    });
  },
  get sync() {
    return {
      desktopDevices: lazy.clientsDevicesDesktop,
      mobileDevices: lazy.clientsDevicesMobile,
      totalDevices: lazy.syncNumClients,
    };
  },
  get xpinstallEnabled() {
    // This is needed for all add-on recommendations, to know if we allow xpi installs in the first place
    return lazy.isXPIInstallEnabled;
  },
  get addonsInfo() {
    let bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
      Ci.nsIBackgroundTasks
    );
    if (bts?.isBackgroundTaskMode) {
      return { addons: {}, isFullData: true };
    }

    return lazy.AddonManager.getActiveAddons(["extension", "service"]).then(
      ({ addons, fullData }) => {
        const info = {};
        for (const addon of addons) {
          info[addon.id] = {
            version: addon.version,
            type: addon.type,
            isSystem: addon.isSystem,
            isWebExtension: addon.isWebExtension,
          };
          if (fullData) {
            Object.assign(info[addon.id], {
              name: addon.name,
              userDisabled: addon.userDisabled,
              installDate: addon.installDate,
            });
          }
        }
        return { addons: info, isFullData: fullData };
      }
    );
  },
  get searchEngines() {
    const NONE = { installed: [], current: "" };
    let bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
      Ci.nsIBackgroundTasks
    );
    if (bts?.isBackgroundTaskMode) {
      return Promise.resolve(NONE);
    }
    return new Promise(resolve => {
      // Note: calling init ensures this code is only executed after Search has been initialized
      Services.search
        .getAppProvidedEngines()
        .then(engines => {
          resolve({
            current: Services.search.defaultEngine.identifier,
            installed: engines.map(engine => engine.identifier),
          });
        })
        .catch(() => resolve(NONE));
    });
  },
  get isDefaultBrowser() {
    return QueryCache.getters.isDefaultBrowser.get().catch(() => null);
  },
  get devToolsOpenedCount() {
    return lazy.devtoolsSelfXSSCount;
  },
  get topFrecentSites() {
    return QueryCache.queries.TopFrecentSites.get().then(sites =>
      sites.map(site => ({
        url: site.url,
        host: new URL(site.url).hostname,
        frecency: site.frecency,
        lastVisitDate: site.lastVisitDate,
      }))
    );
  },
  get recentBookmarks() {
    return QueryCache.queries.RecentBookmarks.get();
  },
  get pinnedSites() {
    return NewTabUtils.pinnedLinks.links.map(site =>
      site
        ? {
            url: site.url,
            host: new URL(site.url).hostname,
            searchTopSite: site.searchTopSite,
          }
        : {}
    );
  },
  get providerCohorts() {
    return lazy.ASRouterPreferences.providers.reduce((prev, current) => {
      prev[current.id] = current.cohort || "";
      return prev;
    }, {});
  },
  get totalBookmarksCount() {
    return QueryCache.queries.TotalBookmarksCount.get();
  },
  get firefoxVersion() {
    return parseInt(AppConstants.MOZ_APP_VERSION.match(/\d+/), 10);
  },
  get region() {
    return lazy.Region.home || "";
  },
  get needsUpdate() {
    return QueryCache.queries.CheckBrowserNeedsUpdate.get();
  },
  get hasPinnedTabs() {
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      if (win.closed || !win.ownerGlobal.gBrowser) {
        continue;
      }
      if (win.ownerGlobal.gBrowser.visibleTabs.filter(t => t.pinned).length) {
        return true;
      }
    }

    return false;
  },
  get hasAccessedFxAPanel() {
    return lazy.hasAccessedFxAPanel;
  },
  get isWhatsNewPanelEnabled() {
    return lazy.isWhatsNewPanelEnabled;
  },
  get userPrefs() {
    return {
      cfrFeatures: lazy.cfrFeaturesUserPref,
      cfrAddons: lazy.cfrAddonsUserPref,
    };
  },
  get totalBlockedCount() {
    return lazy.TrackingDBService.sumAllEvents();
  },
  get blockedCountByType() {
    const idToTextMap = new Map([
      [Ci.nsITrackingDBService.TRACKERS_ID, "trackerCount"],
      [Ci.nsITrackingDBService.TRACKING_COOKIES_ID, "cookieCount"],
      [Ci.nsITrackingDBService.CRYPTOMINERS_ID, "cryptominerCount"],
      [Ci.nsITrackingDBService.FINGERPRINTERS_ID, "fingerprinterCount"],
      [Ci.nsITrackingDBService.SOCIAL_ID, "socialCount"],
    ]);

    const dateTo = new Date();
    const dateFrom = new Date(dateTo.getTime() - 42 * 24 * 60 * 60 * 1000);
    return lazy.TrackingDBService.getEventsByDateRange(dateFrom, dateTo).then(
      eventsByDate => {
        let totalEvents = {};
        for (let blockedType of idToTextMap.values()) {
          totalEvents[blockedType] = 0;
        }

        return eventsByDate.reduce((acc, day) => {
          const type = day.getResultByName("type");
          const count = day.getResultByName("count");
          acc[idToTextMap.get(type)] = acc[idToTextMap.get(type)] + count;
          return acc;
        }, totalEvents);
      }
    );
  },
  get attachedFxAOAuthClients() {
    return this.usesFirefoxSync
      ? QueryCache.queries.ListAttachedOAuthClients.get()
      : [];
  },
  get platformName() {
    return AppConstants.platform;
  },
  get isChinaRepack() {
    return (
      Services.prefs
        .getDefaultBranch(null)
        .getCharPref(DISTRIBUTION_ID_PREF, "default") ===
      DISTRIBUTION_ID_CHINA_REPACK
    );
  },
  get userId() {
    return lazy.ClientEnvironment.userId;
  },
  get profileRestartCount() {
    let bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
      Ci.nsIBackgroundTasks
    );
    if (bts?.isBackgroundTaskMode) {
      return 0;
    }
    // Counter starts at 1 when a profile is created, substract 1 so the value
    // returned matches expectations
    return (
      lazy.TelemetrySession.getMetadata("targeting").profileSubsessionCounter -
      1
    );
  },
  get homePageSettings() {
    const url = lazy.HomePage.get();
    const { isWebExt, isCustomUrl, urls } = parseAboutPageURL(url);

    return {
      isWebExt,
      isCustomUrl,
      urls,
      isDefault: lazy.HomePage.isDefault,
      isLocked: lazy.HomePage.locked,
    };
  },
  get newtabSettings() {
    const url = lazy.AboutNewTab.newTabURL;
    const { isWebExt, isCustomUrl, urls } = parseAboutPageURL(url);

    return {
      isWebExt,
      isCustomUrl,
      isDefault: lazy.AboutNewTab.activityStreamEnabled,
      url: urls[0].url,
      host: urls[0].host,
    };
  },
  get activeNotifications() {
    let bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
      Ci.nsIBackgroundTasks
    );
    if (bts?.isBackgroundTaskMode) {
      // This might need to hook into the alert service to enumerate relevant
      // persistent native notifications.
      return false;
    }

    let window = lazy.BrowserWindowTracker.getTopWindow();

    // Technically this doesn't mean we have active notifications,
    // but because we use !activeNotifications to check for conflicts, this should return true
    if (!window) {
      return true;
    }

    if (
      window.gURLBar?.view.isOpen ||
      window.gNotificationBox?.currentNotification ||
      window.gBrowser.getNotificationBox()?.currentNotification
    ) {
      return true;
    }

    return false;
  },

  get isMajorUpgrade() {
    return lazy.BrowserHandler.majorUpgrade;
  },

  get hasActiveEnterprisePolicies() {
    return Services.policies.status === Services.policies.ACTIVE;
  },

  get userMonthlyActivity() {
    return QueryCache.queries.UserMonthlyActivity.get();
  },

  get doesAppNeedPin() {
    return QueryCache.getters.doesAppNeedPin.get();
  },

  get doesAppNeedPrivatePin() {
    return QueryCache.getters.doesAppNeedPrivatePin.get();
  },

  get launchOnLoginEnabled() {
    if (AppConstants.platform !== "win") {
      return false;
    }
    return lazy.WindowsLaunchOnLogin.getLaunchOnLoginEnabled();
  },

  /**
   * Is this invocation running in background task mode?
   *
   * @return {boolean} `true` if running in background task mode.
   */
  get isBackgroundTaskMode() {
    let bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
      Ci.nsIBackgroundTasks
    );
    return !!bts?.isBackgroundTaskMode;
  },

  /**
   * A non-empty task name if this invocation is running in background
   * task mode, or `null` if this invocation is not running in
   * background task mode.
   *
   * @return {string|null} background task name or `null`.
   */
  get backgroundTaskName() {
    let bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
      Ci.nsIBackgroundTasks
    );
    return bts?.backgroundTaskName();
  },

  get userPrefersReducedMotion() {
    let window = Services.appShell.hiddenDOMWindow;
    return window?.matchMedia("(prefers-reduced-motion: reduce)")?.matches;
  },

  /**
   * Whether or not the user is in the Major Release 2022 holdback study.
   */
  get inMr2022Holdback() {
    return (
      lazy.NimbusFeatures.majorRelease2022.getVariable("onboarding") === false
    );
  },

  /**
   * The distribution id, if any.
   * @return {string}
   */
  get distributionId() {
    return Services.prefs
      .getDefaultBranch(null)
      .getCharPref("distribution.id", "");
  },

  /** Where the Firefox View button is shown, if at all.
   * @return {string} container of the button if it is shown in the toolbar/overflow menu
   * @return {string} `null` if the button has been removed
   */
  get fxViewButtonAreaType() {
    let button = lazy.CustomizableUI.getWidget("firefox-view-button");
    return button.areaType;
  },

  isDefaultHandler: {
    get html() {
      return QueryCache.getters.isDefaultHTMLHandler.get();
    },
    get pdf() {
      return QueryCache.getters.isDefaultPDFHandler.get();
    },
  },

  get defaultPDFHandler() {
    return QueryCache.getters.defaultPDFHandler.get();
  },

  get creditCardsSaved() {
    return getAutofillRecords({ collectionName: "creditCards" });
  },

  get addressesSaved() {
    return getAutofillRecords({ collectionName: "addresses" });
  },

  /**
   * Has the user ever used the Migration Wizard to migrate bookmarks?
   * @return {boolean} `true` if bookmark migration has occurred.
   */
  get hasMigratedBookmarks() {
    return lazy.hasMigratedBookmarks;
  },

  /**
   * Has the user ever used the Migration Wizard to migrate passwords from
   * a CSV file?
   * @return {boolean} `true` if CSV passwords have been imported via the
   *   migration wizard.
   */
  get hasMigratedCSVPasswords() {
    return lazy.hasMigratedCSVPasswords;
  },

  /**
   * Has the user ever used the Migration Wizard to migrate history?
   * @return {boolean} `true` if history migration has occurred.
   */
  get hasMigratedHistory() {
    return lazy.hasMigratedHistory;
  },

  /**
   * Has the user ever used the Migration Wizard to migrate passwords?
   * @return {boolean} `true` if password migration has occurred.
   */
  get hasMigratedPasswords() {
    return lazy.hasMigratedPasswords;
  },

  /**
   * Returns true if the user is configured to use the embedded migration
   * wizard in about:welcome by having
   * "browser.migrate.content-modal.about-welcome-behavior" be equal to
   * "embedded".
   * @return {boolean} `true` if the embedded migration wizard is enabled.
   */
  get useEmbeddedMigrationWizard() {
    return lazy.useEmbeddedMigrationWizard;
  },

  /**
   * Whether the user installed Firefox via the RTAMO flow.
   * @return {boolean} `true` when RTAMO has been used to download Firefox,
   * `false` otherwise.
   */
  get isRTAMO() {
    const { attributionData } = this;

    return (
      attributionData?.source === "addons.mozilla.org" &&
      !!decodeAttributionValue(attributionData?.content)?.startsWith("rta:")
    );
  },

  /**
   * Whether the user installed via the device migration flow.
   * @return {boolean} `true` when the link to download the browser was part
   * of guidance for device migration. `false` otherwise.
   */
  get isDeviceMigration() {
    const { attributionData } = this;

    return attributionData?.campaign === "migration";
  },

  /**
   * The values of the height and width available to the browser to display
   * web content. The available height and width are each calculated taking
   * into account the presence of menu bars, docks, and other similar OS elements
   * @returns {Object} resolution The resolution object containing width and height
   * @returns {string} resolution.width The available width of the primary monitor
   * @returns {string} resolution.height The available height of the primary monitor
   */
  get primaryResolution() {
    // Using hidden dom window ensures that we have a window object
    // to grab a screen from in certain edge cases such as targeting evaluation
    // during first startup before the browser is available, and in MacOS
    let window = Services.appShell.hiddenDOMWindow;
    return {
      width: window?.screen.availWidth,
      height: window?.screen.availHeight,
    };
  },

  get archBits() {
    let bits = null;
    try {
      bits = Services.sysinfo.getProperty("archbits", null);
    } catch (_e) {
      // getProperty can throw if the memsize does not exist
    }
    if (bits) {
      bits = Number(bits);
    }
    return bits;
  },

  get memoryMB() {
    let memory = null;
    try {
      memory = Services.sysinfo.getProperty("memsize", null);
    } catch (_e) {
      // getProperty can throw if the memsize does not exist
    }
    if (memory) {
      memory = Number(memory) / 1024 / 1024;
    }
    return memory;
  },
};

const ASRouterTargeting = {
  Environment: TargetingGetters,

  /**
   * Snapshot the current targeting environment.
   *
   * Asynchronous getters are handled.  Getters that throw or reject
   * are ignored.
   *
   * @param {object} target - the environment to snapshot.
   * @return {object} snapshot of target with `environment` object and `version`
   * integer.
   */
  async getEnvironmentSnapshot(target = ASRouterTargeting.Environment) {
    async function resolve(object) {
      if (typeof object === "object" && object !== null) {
        if (Array.isArray(object)) {
          return Promise.all(object.map(async item => resolve(await item)));
        }

        if (object instanceof Date) {
          return object;
        }

        // One promise for each named property. Label promises with property name.
        const promises = Object.keys(object).map(async key => {
          // Each promise needs to check if we're shutting down when it is evaluated.
          if (Services.startup.shuttingDown) {
            throw new Error(
              "shutting down, so not querying targeting environment"
            );
          }

          const value = await resolve(await object[key]);

          return [key, value];
        });

        const resolved = {};
        for (const result of await Promise.allSettled(promises)) {
          // Ignore properties that are rejected.
          if (result.status === "fulfilled") {
            const [key, value] = result.value;
            resolved[key] = value;
          }
        }

        return resolved;
      }

      return object;
    }

    const environment = await resolve(target);

    // Should we need to migrate in the future.
    const snapshot = { environment, version: 1 };

    return snapshot;
  },

  isTriggerMatch(trigger = {}, candidateMessageTrigger = {}) {
    if (trigger.id !== candidateMessageTrigger.id) {
      return false;
    } else if (
      !candidateMessageTrigger.params &&
      !candidateMessageTrigger.patterns
    ) {
      return true;
    }

    if (!trigger.param) {
      return false;
    }

    return (
      (candidateMessageTrigger.params &&
        trigger.param.host &&
        candidateMessageTrigger.params.includes(trigger.param.host)) ||
      (candidateMessageTrigger.params &&
        trigger.param.type &&
        candidateMessageTrigger.params.filter(t => t === trigger.param.type)
          .length) ||
      (candidateMessageTrigger.params &&
        trigger.param.type &&
        candidateMessageTrigger.params.filter(
          t => (t & trigger.param.type) === t
        ).length) ||
      (candidateMessageTrigger.patterns &&
        trigger.param.url &&
        new MatchPatternSet(candidateMessageTrigger.patterns).matches(
          trigger.param.url
        ))
    );
  },

  /**
   * getCachedEvaluation - Return a cached jexl evaluation if available
   *
   * @param {string} targeting JEXL expression to lookup
   * @returns {obj|null} Object with value result or null if not available
   */
  getCachedEvaluation(targeting) {
    if (jexlEvaluationCache.has(targeting)) {
      const { timestamp, value } = jexlEvaluationCache.get(targeting);
      if (Date.now() - timestamp <= CACHE_EXPIRATION) {
        return { value };
      }
      jexlEvaluationCache.delete(targeting);
    }

    return null;
  },

  /**
   * checkMessageTargeting - Checks is a message's targeting parameters are satisfied
   *
   * @param {*} message An AS router message
   * @param {obj} targetingContext a TargetingContext instance complete with eval environment
   * @param {func} onError A function to handle errors (takes two params; error, message)
   * @param {boolean} shouldCache Should the JEXL evaluations be cached and reused.
   * @returns
   */
  async checkMessageTargeting(message, targetingContext, onError, shouldCache) {
    lazy.ASRouterPreferences.console.debug(
      "in checkMessageTargeting, arguments = ",
      Array.from(arguments) // eslint-disable-line prefer-rest-params
    );

    // If no targeting is specified,
    if (!message.targeting) {
      return true;
    }
    let result;
    try {
      if (shouldCache) {
        result = this.getCachedEvaluation(message.targeting);
        if (result) {
          return result.value;
        }
      }
      // Used to report the source of the targeting error in the case of
      // undesired events
      targetingContext.setTelemetrySource(message.id);
      result = await targetingContext.evalWithDefault(message.targeting);
      if (shouldCache) {
        jexlEvaluationCache.set(message.targeting, {
          timestamp: Date.now(),
          value: result,
        });
      }
    } catch (error) {
      if (onError) {
        onError(error, message);
      }
      console.error(error);
      result = false;
    }
    return result;
  },

  _isMessageMatch(
    message,
    trigger,
    targetingContext,
    onError,
    shouldCache = false
  ) {
    return (
      message &&
      (trigger
        ? this.isTriggerMatch(trigger, message.trigger)
        : !message.trigger) &&
      // If a trigger expression was passed to this function, the message should match it.
      // Otherwise, we should choose a message with no trigger property (i.e. a message that can show up at any time)
      this.checkMessageTargeting(
        message,
        targetingContext,
        onError,
        shouldCache
      )
    );
  },

  /**
   * findMatchingMessage - Given an array of messages, returns one message
   *                       whos targeting expression evaluates to true
   *
   * @param {Array<Message>} messages An array of AS router messages
   * @param {trigger} string A trigger expression if a message for that trigger is desired
   * @param {obj|null} context A FilterExpression context. Defaults to TargetingGetters above.
   * @param {func} onError A function to handle errors (takes two params; error, message)
   * @param {func} ordered An optional param when true sort message by order specified in message
   * @param {boolean} shouldCache Should the JEXL evaluations be cached and reused.
   * @param {boolean} returnAll Should we return all matching messages, not just the first one found.
   * @returns {obj|Array<Message>} If returnAll is false, a single message. If returnAll is true, an array of messages.
   */
  async findMatchingMessage({
    messages,
    trigger = {},
    context = {},
    onError,
    ordered = false,
    shouldCache = false,
    returnAll = false,
  }) {
    const sortedMessages = getSortedMessages(messages, { ordered });
    lazy.ASRouterPreferences.console.debug(
      "in findMatchingMessage, sortedMessages = ",
      sortedMessages
    );
    const matching = returnAll ? [] : null;
    const targetingContext = new lazy.TargetingContext(
      lazy.TargetingContext.combineContexts(
        context,
        this.Environment,
        trigger.context || {}
      )
    );

    const isMatch = candidate =>
      this._isMessageMatch(
        candidate,
        trigger,
        targetingContext,
        onError,
        shouldCache
      );

    for (const candidate of sortedMessages) {
      if (await isMatch(candidate)) {
        // If not returnAll, we should return the first message we find that matches.
        if (!returnAll) {
          return candidate;
        }

        matching.push(candidate);
      }
    }
    return matching;
  },
};

const EXPORTED_SYMBOLS = [
  "ASRouterTargeting",
  "QueryCache",
  "CachedTargetingGetter",
  "getSortedMessages",
];
