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
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  AboutWelcomeTelemetry:
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm",
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
  UNKNOWN: "unknown",
  WINDOW_CLOSED: "welcome-window-closed",
  TAB_CLOSED: "welcome-tab-closed",
  APP_SHUT_DOWN: "app-shut-down",
  ADDRESS_BAR_NAVIGATED: "address-bar-navigated",
};

class AboutWelcomeObserver {
  constructor() {
    Services.obs.addObserver(this, "quit-application");

    this.win = Services.focus.activeWindow;
    if (!this.win) {
      return;
    }

    this.terminateReason = AWTerminate.UNKNOWN;

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

class AboutWelcomeParent extends JSWindowActorParent {
  constructor() {
    super();
    this.AboutWelcomeObserver = new AboutWelcomeObserver(this);
  }

  didDestroy() {
    if (this.AboutWelcomeObserver) {
      this.AboutWelcomeObserver.stop();
    }

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
  onContentMessage(type, data, browser, window) {
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
      case "AWPage:OPEN_AWESOME_BAR":
        window.gURLBar.search("");
        break;
      case "AWPage:OPEN_PRIVATE_BROWSER_WINDOW":
        window.OpenBrowserWindow({ private: true });
        break;
      case "AWPage:SHOW_MIGRATION_WIZARD":
        MigrationUtils.showMigrationWizard(window, [
          MigrationUtils.MIGRATION_ENTRYPOINT_NEWTAB,
        ]);
        break;
      case "AWPage:FXA_METRICS_FLOW_URI":
        return FxAccounts.config.promiseMetricsFlowURI("aboutwelcome");
      case "AWPage:TELEMETRY_EVENT":
        Telemetry.sendTelemetry(data);
        break;
      case "AWPage:LOCATION_CHANGED":
        this.AboutWelcomeObserver.terminateReason =
          AWTerminate.ADDRESS_BAR_NAVIGATED;
        break;
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
