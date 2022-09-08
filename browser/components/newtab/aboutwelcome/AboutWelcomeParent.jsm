/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeParent"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  BuiltInThemes: "resource:///modules/BuiltInThemes.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.jsm",
  AboutWelcomeTelemetry:
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm",
  AboutWelcomeDefaults:
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeDefaults.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Region: "resource://gre/modules/Region.jsm",
  ShellService: "resource:///modules/ShellService.jsm",
  LangPackMatcher: "resource://gre/modules/LangPackMatcher.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.import(
    "resource://messaging-system/lib/Logger.jsm"
  );
  return new Logger("AboutWelcomeParent");
});

XPCOMUtils.defineLazyGetter(
  lazy,
  "Telemetry",
  () => new lazy.AboutWelcomeTelemetry()
);

const DID_SEE_ABOUT_WELCOME_PREF = "trailhead.firstrun.didSeeAboutWelcome";
const AWTerminate = {
  WINDOW_CLOSED: "welcome-window-closed",
  TAB_CLOSED: "welcome-tab-closed",
  APP_SHUT_DOWN: "app-shut-down",
  ADDRESS_BAR_NAVIGATED: "address-bar-navigated",
};
const LIGHT_WEIGHT_THEMES = {
  AUTOMATIC: "default-theme@mozilla.org",
  DARK: "firefox-compact-dark@mozilla.org",
  LIGHT: "firefox-compact-light@mozilla.org",
  ALPENGLOW: "firefox-alpenglow@mozilla.org",
  "PLAYMAKER-SOFT": "playmaker-soft-colorway@mozilla.org",
  "PLAYMAKER-BALANCED": "playmaker-balanced-colorway@mozilla.org",
  "PLAYMAKER-BOLD": "playmaker-bold-colorway@mozilla.org",
  "EXPRESSIONIST-SOFT": "expressionist-soft-colorway@mozilla.org",
  "EXPRESSIONIST-BALANCED": "expressionist-balanced-colorway@mozilla.org",
  "EXPRESSIONIST-BOLD": "expressionist-bold-colorway@mozilla.org",
  "VISIONARY-SOFT": "visionary-soft-colorway@mozilla.org",
  "VISIONARY-BALANCED": "visionary-balanced-colorway@mozilla.org",
  "VISIONARY-BOLD": "visionary-bold-colorway@mozilla.org",
  "ACTIVIST-SOFT": "activist-soft-colorway@mozilla.org",
  "ACTIVIST-BALANCED": "activist-balanced-colorway@mozilla.org",
  "ACTIVIST-BOLD": "activist-bold-colorway@mozilla.org",
  "DREAMER-SOFT": "dreamer-soft-colorway@mozilla.org",
  "DREAMER-BALANCED": "dreamer-balanced-colorway@mozilla.org",
  "DREAMER-BOLD": "dreamer-bold-colorway@mozilla.org",
  "INNOVATOR-SOFT": "innovator-soft-colorway@mozilla.org",
  "INNOVATOR-BALANCED": "innovator-balanced-colorway@mozilla.org",
  "INNOVATOR-BOLD": "innovator-bold-colorway@mozilla.org",
};

async function getImportableSites() {
  const sites = [];

  // Just handle these chromium-based browsers for now
  for (const browserId of ["chrome", "chromium-edge", "chromium"]) {
    // Skip if there's no profile data.
    const migrator = await lazy.MigrationUtils.getMigrator(browserId);
    if (!migrator) {
      continue;
    }

    // Check each profile for top sites
    const dataPath = await migrator.wrappedJSObject._getChromeUserDataPathIfExists();
    for (const profile of await migrator.getSourceProfiles()) {
      let path = PathUtils.join(dataPath, profile.id, "Top Sites");
      // Skip if top sites data is missing
      if (!(await IOUtils.exists(path))) {
        Cu.reportError(`Missing file at ${path}`);
        continue;
      }

      try {
        for (const row of await lazy.MigrationUtils.getRowsFromDBWithoutLocks(
          path,
          `Importable ${browserId} top sites`,
          `SELECT url
           FROM top_sites
           ORDER BY url_rank`
        )) {
          sites.push(row.getString(0));
        }
      } catch (ex) {
        Cu.reportError(
          `Failed to get importable top sites from ${browserId} ${ex}`
        );
      }
    }
  }
  return sites;
}

class AboutWelcomeObserver {
  constructor() {
    Services.obs.addObserver(this, "quit-application");

    this.win = Services.focus.activeWindow;
    if (!this.win) {
      return;
    }

    this.terminateReason = AWTerminate.ADDRESS_BAR_NAVIGATED;

    this.onWindowClose = () => {
      this.terminateReason = AWTerminate.WINDOW_CLOSED;
    };

    this.onTabClose = () => {
      this.terminateReason = AWTerminate.TAB_CLOSED;
    };

    this.win.addEventListener("TabClose", this.onTabClose, { once: true });
    this.win.addEventListener("unload", this.onWindowClose, { once: true });
  }

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application":
        this.terminateReason = AWTerminate.APP_SHUT_DOWN;
        break;
    }
  }

  // Added for testing
  get AWTerminate() {
    return AWTerminate;
  }

  stop() {
    lazy.log.debug(`Terminate reason is ${this.terminateReason}`);
    Services.obs.removeObserver(this, "quit-application");
    if (!this.win) {
      return;
    }
    this.win.removeEventListener("TabClose", this.onTabClose);
    this.win.removeEventListener("unload", this.onWindowClose);
    this.win = null;
  }
}

class RegionHomeObserver {
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case lazy.Region.REGION_TOPIC:
        Services.obs.removeObserver(this, lazy.Region.REGION_TOPIC);
        this.regionHomeDeferred.resolve(lazy.Region.home);
        this.regionHomeDeferred = null;
        break;
    }
  }

  promiseRegionHome() {
    // Add observer and create promise that should be resolved
    // with region or rejected inside didDestroy if user exits
    // before region is available
    if (!this.regionHomeDeferred) {
      Services.obs.addObserver(this, lazy.Region.REGION_TOPIC);
      this.regionHomeDeferred = lazy.PromiseUtils.defer();
    }
    return this.regionHomeDeferred.promise;
  }

  stop() {
    if (this.regionHomeDeferred) {
      Services.obs.removeObserver(this, lazy.Region.REGION_TOPIC);
      // Reject unresolved deferred promise on exit
      this.regionHomeDeferred.reject(
        new Error("Unresolved region home promise")
      );
      this.regionHomeDeferred = null;
    }
  }
}

class AboutWelcomeParent extends JSWindowActorParent {
  constructor() {
    super();
    this.AboutWelcomeObserver = new AboutWelcomeObserver(this);
  }

  // Static methods that calls into ShellService to check
  // if Firefox is pinned or already default
  static doesAppNeedPin() {
    return lazy.ShellService.doesAppNeedPin();
  }

  static isDefaultBrowser() {
    return lazy.ShellService.isDefaultBrowser();
  }

  didDestroy() {
    if (this.AboutWelcomeObserver) {
      this.AboutWelcomeObserver.stop();
    }
    this.RegionHomeObserver?.stop();

    lazy.Telemetry.sendTelemetry({
      event: "SESSION_END",
      event_context: {
        reason: this.AboutWelcomeObserver.terminateReason,
        page: "about:welcome",
      },
      message_id: this.AWMessageId,
    });
  }

  /**
   * Handle messages from AboutWelcomeChild.jsm
   *
   * @param {string} type
   * @param {any=} data
   * @param {Browser} the xul:browser rendering the page
   */
  async onContentMessage(type, data, browser) {
    lazy.log.debug(`Received content event: ${type}`);
    switch (type) {
      case "AWPage:SET_WELCOME_MESSAGE_SEEN":
        this.AWMessageId = data;
        try {
          Services.prefs.setBoolPref(DID_SEE_ABOUT_WELCOME_PREF, true);
        } catch (e) {
          lazy.log.debug(`Fails to set ${DID_SEE_ABOUT_WELCOME_PREF}.`);
        }
        break;
      case "AWPage:SPECIAL_ACTION":
        lazy.SpecialMessageActions.handleAction(data, browser);
        break;
      case "AWPage:FXA_METRICS_FLOW_URI":
        return lazy.FxAccounts.config.promiseMetricsFlowURI("aboutwelcome");
      case "AWPage:IMPORTABLE_SITES":
        return getImportableSites();
      case "AWPage:TELEMETRY_EVENT":
        lazy.Telemetry.sendTelemetry(data);
        break;
      case "AWPage:GET_ATTRIBUTION_DATA":
        let attributionData = await lazy.AboutWelcomeDefaults.getAttributionContent();
        return attributionData;
      case "AWPage:SELECT_THEME":
        await lazy.BuiltInThemes.ensureBuiltInThemes();
        return lazy.AddonManager.getAddonByID(
          LIGHT_WEIGHT_THEMES[data]
        ).then(addon => addon.enable());
      case "AWPage:GET_SELECTED_THEME":
        let themes = await lazy.AddonManager.getAddonsByTypes(["theme"]);
        let activeTheme = themes.find(addon => addon.isActive);
        // Store the current theme ID so user can restore their previous theme.
        if (activeTheme?.id) {
          LIGHT_WEIGHT_THEMES.AUTOMATIC = activeTheme.id;
        }
        // convert this to the short form name that the front end code
        // expects
        let themeShortName = Object.keys(LIGHT_WEIGHT_THEMES).find(
          key => LIGHT_WEIGHT_THEMES[key] === activeTheme?.id
        );
        return themeShortName?.toLowerCase();
      case "AWPage:GET_REGION":
        if (lazy.Region.home !== null) {
          return lazy.Region.home;
        }
        if (!this.RegionHomeObserver) {
          this.RegionHomeObserver = new RegionHomeObserver(this);
        }
        return this.RegionHomeObserver.promiseRegionHome();
      case "AWPage:DOES_APP_NEED_PIN":
        return AboutWelcomeParent.doesAppNeedPin();
      case "AWPage:NEED_DEFAULT":
        // Only need to set default if we're supposed to check and not default.
        return (
          Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser") &&
          !AboutWelcomeParent.isDefaultBrowser()
        );
      case "AWPage:WAIT_FOR_MIGRATION_CLOSE":
        return new Promise(resolve =>
          Services.ww.registerNotification(function observer(subject, topic) {
            if (
              topic === "domwindowclosed" &&
              subject.document.documentURI ===
                "chrome://browser/content/migration/migration.xhtml"
            ) {
              Services.ww.unregisterNotification(observer);
              resolve();
            }
          })
        );
      case "AWPage:GET_APP_AND_SYSTEM_LOCALE_INFO":
        return lazy.LangPackMatcher.getAppAndSystemLocaleInfo();
      case "AWPage:NEGOTIATE_LANGPACK":
        return lazy.LangPackMatcher.negotiateLangPackForLanguageMismatch(data);
      case "AWPage:ENSURE_LANG_PACK_INSTALLED":
        return lazy.LangPackMatcher.ensureLangPackInstalled(data);
      case "AWPage:SET_REQUESTED_LOCALES":
        return lazy.LangPackMatcher.setRequestedAppLocales(data);
      case "AWPage:SEND_TO_DEVICE_EMAILS_SUPPORTED": {
        return lazy.BrowserUtils.sendToDeviceEmailsSupported();
      }
      default:
        lazy.log.debug(`Unexpected event ${type} was not handled.`);
    }

    return undefined;
  }

  /**
   * @param {{name: string, data?: any}} message
   * @override
   */
  receiveMessage(message) {
    const { name, data } = message;
    let browser;

    if (this.manager.rootFrameLoader) {
      browser = this.manager.rootFrameLoader.ownerElement;
      return this.onContentMessage(name, data, browser);
    }

    lazy.log.warn(`Not handling ${name} because the browser doesn't exist.`);
    return null;
  }
}
