/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeParent"];
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
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
});

XPCOMUtils.defineLazyGetter(this, "log", () => {
  const { Logger } = ChromeUtils.import(
    "resource://messaging-system/lib/Logger.jsm"
  );
  return new Logger("AboutWelcomeParent");
});

XPCOMUtils.defineLazyGetter(
  this,
  "Telemetry",
  () => new AboutWelcomeTelemetry()
);

const DID_SEE_ABOUT_WELCOME_PREF = "trailhead.firstrun.didSeeAboutWelcome";
const AWTerminate = {
  WINDOW_CLOSED: "welcome-window-closed",
  TAB_CLOSED: "welcome-tab-closed",
  APP_SHUT_DOWN: "app-shut-down",
  ADDRESS_BAR_NAVIGATED: "address-bar-navigated",
};
const LIGHT_WEIGHT_THEMES = {
  DARK: "firefox-compact-dark@mozilla.org",
  LIGHT: "firefox-compact-light@mozilla.org",
  AUTOMATIC: "default-theme@mozilla.org",
  ALPENGLOW: "firefox-alpenglow@mozilla.org",
  "ABSTRACT-SOFT": "abstract-soft-colorway@mozilla.org",
  "ABSTRACT-BALANCED": "abstract-balanced-colorway@mozilla.org",
  "ABSTRACT-BOLD": "abstract-bold-colorway@mozilla.org",
  "CHEERS-SOFT": "cheers-soft-colorway@mozilla.org",
  "CHEERS-BALANCED": "cheers-balanced-colorway@mozilla.org",
  "CHEERS-BOLD": "cheers-bold-colorway@mozilla.org",
  "ELEMENTAL-SOFT": "elemental-soft-colorway@mozilla.org",
  "ELEMENTAL-BALANCED": "elemental-balanced-colorway@mozilla.org",
  "ELEMENTAL-BOLD": "elemental-bold-colorway@mozilla.org",
  "FOTO-SOFT": "foto-soft-colorway@mozilla.org",
  "FOTO-BALANCED": "foto-balanced-colorway@mozilla.org",
  "FOTO-BOLD": "foto-bold-colorway@mozilla.org",
  "GRAFFITI-SOFT": "graffiti-soft-colorway@mozilla.org",
  "GRAFFITI-BALANCED": "graffiti-balanced-colorway@mozilla.org",
  "GRAFFITI-BOLD": "graffiti-bold-colorway@mozilla.org",
  "LUSH-SOFT": "lush-soft-colorway@mozilla.org",
  "LUSH-BALANCED": "lush-balanced-colorway@mozilla.org",
  "LUSH-BOLD": "lush-bold-colorway@mozilla.org",
};

async function getImportableSites() {
  const sites = [];

  // Just handle these chromium-based browsers for now
  for (const browserId of ["chrome", "chromium-edge", "chromium"]) {
    // Skip if there's no profile data.
    const migrator = await MigrationUtils.getMigrator(browserId);
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
        for (const row of await MigrationUtils.getRowsFromDBWithoutLocks(
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
    log.debug(`Terminate reason is ${this.terminateReason}`);
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
      case Region.REGION_TOPIC:
        Services.obs.removeObserver(this, Region.REGION_TOPIC);
        this.regionHomeDeferred.resolve(Region.home);
        this.regionHomeDeferred = null;
        break;
    }
  }

  promiseRegionHome() {
    // Add observer and create promise that should be resolved
    // with region or rejected inside didDestroy if user exits
    // before region is available
    if (!this.regionHomeDeferred) {
      Services.obs.addObserver(this, Region.REGION_TOPIC);
      this.regionHomeDeferred = PromiseUtils.defer();
    }
    return this.regionHomeDeferred.promise;
  }

  stop() {
    if (this.regionHomeDeferred) {
      Services.obs.removeObserver(this, Region.REGION_TOPIC);
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
    return ShellService.doesAppNeedPin();
  }

  static isDefaultBrowser() {
    return ShellService.isDefaultBrowser();
  }

  didDestroy() {
    if (this.AboutWelcomeObserver) {
      this.AboutWelcomeObserver.stop();
    }
    this.RegionHomeObserver?.stop();

    Telemetry.sendTelemetry({
      event: "SESSION_END",
      event_context: {
        reason: this.AboutWelcomeObserver.terminateReason,
        page: "about:welcome",
      },
      message_id: this.AWMessageId,
      id: "ABOUT_WELCOME",
    });
  }

  /**
   * Handle messages from AboutWelcomeChild.jsm
   *
   * @param {string} type
   * @param {any=} data
   * @param {Browser} browser
   * @param {Window} window
   */
  async onContentMessage(type, data, browser, window) {
    log.debug(`Received content event: ${type}`);
    switch (type) {
      case "AWPage:SET_WELCOME_MESSAGE_SEEN":
        this.AWMessageId = data;
        try {
          Services.prefs.setBoolPref(DID_SEE_ABOUT_WELCOME_PREF, true);
        } catch (e) {
          log.debug(`Fails to set ${DID_SEE_ABOUT_WELCOME_PREF}.`);
        }
        break;
      case "AWPage:SPECIAL_ACTION":
        SpecialMessageActions.handleAction(data, browser);
        break;
      case "AWPage:FXA_METRICS_FLOW_URI":
        return FxAccounts.config.promiseMetricsFlowURI("aboutwelcome");
      case "AWPage:IMPORTABLE_SITES":
        return getImportableSites();
      case "AWPage:TELEMETRY_EVENT":
        Telemetry.sendTelemetry(data);
        break;
      case "AWPage:GET_ATTRIBUTION_DATA":
        let attributionData = await AboutWelcomeDefaults.getAttributionContent();
        return attributionData;
      case "AWPage:SELECT_THEME":
        await BuiltInThemes.ensureBuiltInThemes();
        return AddonManager.getAddonByID(
          LIGHT_WEIGHT_THEMES[data]
        ).then(addon => addon.enable());
      case "AWPage:GET_SELECTED_THEME":
        let themes = await AddonManager.getAddonsByTypes(["theme"]);
        let activeTheme = themes.find(addon => addon.isActive);

        // convert this to the short form name that the front end code
        // expects
        let themeShortName = Object.keys(LIGHT_WEIGHT_THEMES).find(
          key => LIGHT_WEIGHT_THEMES[key] === activeTheme?.id
        );
        return themeShortName?.toLowerCase();
      case "AWPage:GET_REGION":
        if (Region.home !== null) {
          return Region.home;
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
      default:
        log.debug(`Unexpected event ${type} was not handled.`);
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
    let window;

    if (this.manager.rootFrameLoader) {
      browser = this.manager.rootFrameLoader.ownerElement;
      window = browser.ownerGlobal;
      return this.onContentMessage(name, data, browser, window);
    }

    log.warn(`Not handling ${name} because the browser doesn't exist.`);
    return null;
  }
}
