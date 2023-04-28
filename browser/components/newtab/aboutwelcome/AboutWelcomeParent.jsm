/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeParent"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  BuiltInThemes: "resource:///modules/BuiltInThemes.sys.mjs",
  FxAccounts: "resource://gre/modules/FxAccounts.sys.mjs",
  ShellService: "resource:///modules/ShellService.sys.mjs",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",

  AboutWelcomeTelemetry:
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm",

  AboutWelcomeDefaults:
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeDefaults.jsm",

  LangPackMatcher: "resource://gre/modules/LangPackMatcher.jsm",
  AWScreenUtils: "resource://activity-stream/lib/AWScreenUtils.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
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
      case "AWPage:DOES_APP_NEED_PIN":
        return AboutWelcomeParent.doesAppNeedPin();
      case "AWPage:NEED_DEFAULT":
        // Only need to set default if we're supposed to check and not default.
        return (
          Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser") &&
          !AboutWelcomeParent.isDefaultBrowser()
        );
      case "AWPage:WAIT_FOR_MIGRATION_CLOSE":
        // Support multiples types of migration: 1) content modal 2) old
        // migration modal 3) standalone content modal
        return new Promise(resolve => {
          const topics = [
            "MigrationWizard:Closed",
            "MigrationWizard:Destroyed",
          ];
          const observer = () => {
            topics.forEach(t => Services.obs.removeObserver(observer, t));
            resolve();
          };
          topics.forEach(t => Services.obs.addObserver(observer, t));
        });
      case "AWPage:GET_APP_AND_SYSTEM_LOCALE_INFO":
        return lazy.LangPackMatcher.getAppAndSystemLocaleInfo();
      case "AWPage:EVALUATE_SCREEN_TARGETING":
        return lazy.AWScreenUtils.evaluateTargetingAndRemoveScreens(data);
      case "AWPage:NEGOTIATE_LANGPACK":
        return lazy.LangPackMatcher.negotiateLangPackForLanguageMismatch(data);
      case "AWPage:ENSURE_LANG_PACK_INSTALLED":
        return lazy.LangPackMatcher.ensureLangPackInstalled(data);
      case "AWPage:SET_REQUESTED_LOCALES":
        return lazy.LangPackMatcher.setRequestedAppLocales(data);
      case "AWPage:SEND_TO_DEVICE_EMAILS_SUPPORTED": {
        return lazy.BrowserUtils.sendToDeviceEmailsSupported();
      }
      case "AWPage:FXA_SIGNIN_TAB_FLOW": {
        return this.fxaSignInTabFlow(data, browser);
      }
      default:
        lazy.log.debug(`Unexpected event ${type} was not handled.`);
    }

    return undefined;
  }

  /**
   * Open an FxA sign-in page and automatically close it once sign-in
   * completes.
   *
   * @param {any=} data
   * @param {Browser} the xul:browser rendering the page
   */
  async fxaSignInTabFlow(data, browser) {
    if (!(await lazy.FxAccounts.canConnectAccount())) {
      return false;
    }
    const url = await lazy.FxAccounts.config.promiseConnectAccountURI(
      data?.entrypoint || "activity-stream-firstrun",
      data?.extraParams || {}
    );

    let window = browser.ownerGlobal;

    let fxaBrowser = await new Promise(resolve => {
      window.openLinkIn(url, "tab", {
        private: false,
        triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
          {}
        ),
        csp: null,
        resolveOnNewTabCreated: resolve,
        forceForeground: true,
      });
    });

    let gBrowser = fxaBrowser.getTabBrowser();
    let fxaTab = gBrowser.getTabForBrowser(fxaBrowser);

    let didSignIn = await new Promise(resolve => {
      // We're going to be setting up a listener and an observer for this
      // mechanism.
      //
      // 1. An event listener for the TabClose event, to detect if the user
      //    closes the tab before completing sign-in
      // 2. An nsIObserver that listens for the UIState for FxA to reach
      //    STATUS_SIGNED_IN.
      //
      // We want to clean up both the listener and observer when all of this
      // is done.
      //
      // We use an AbortController to make it easier to manage the cleanup.
      let controller = new AbortController();
      let { signal } = controller;

      // This nsIObserver will listen for the UIState status to change to
      // STATUS_SIGNED_IN as our definitive signal that FxA sign-in has
      // completed. It will then resolve the outer Promise to `true`.
      let fxaObserver = {
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIObserver,
          Ci.nsISupportsWeakReference,
        ]),

        observe(aSubject, aTopic, aData) {
          let state = lazy.UIState.get();
          if (state.status === lazy.UIState.STATUS_SIGNED_IN) {
            // We completed sign-in, so tear down our listener / observer and resolve
            // didSignIn to true.
            controller.abort();
            resolve(true);
          }
        },
      };

      // The TabClose event listener _can_ accept the AbortController signal,
      // which will then remove the event listener after controller.abort is
      // called.
      fxaTab.addEventListener(
        "TabClose",
        () => {
          // If the TabClose event was fired before the event handler was
          // removed, this means that the tab was closed and sign-in was
          // not completed, which means we should resolve didSignIn to false.
          controller.abort();
          resolve(false);
        },
        { once: true, signal }
      );

      Services.obs.addObserver(fxaObserver, lazy.UIState.ON_UPDATE);

      // Unfortunately, nsIObserverService.addObserver does not accept an
      // AbortController signal as a parameter, so instead we listen for the
      // abort event on the signal to remove the observer.
      signal.addEventListener(
        "abort",
        () => {
          Services.obs.removeObserver(fxaObserver, lazy.UIState.ON_UPDATE);
        },
        { once: true }
      );
    });

    // If the user completed sign-in, we'll close the fxaBrowser tab for
    // them to bring them back to the about:welcome flow.
    if (didSignIn && data?.autoClose !== false) {
      gBrowser.removeTab(fxaTab);
    }

    return didSignIn;
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
