/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the TabsSetupFlowManager singleton, which manages the state and
 * diverse inputs which drive the Firefox View synced tabs setup flow
 */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "resource://gre/modules/Log.jsm",
  UIState: "resource://services-sync/UIState.jsm",
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});
XPCOMUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.import(
    "resource://gre/modules/FxAccounts.jsm"
  ).getFxAccountsSingleton();
});

const SYNC_TABS_PREF = "services.sync.engine.tabs";
const RECENT_TABS_SYNC = "services.sync.lastTabFetch";
const MOBILE_PROMO_DISMISSED_PREF =
  "browser.tabs.firefox-view.mobilePromo.dismissed";
const LOGGING_PREF = "browser.tabs.firefox-view.logLevel";
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";
const TOPIC_DEVICELIST_UPDATED = "fxaccounts:devicelist_updated";

function openTabInWindow(window, url) {
  const {
    switchToTabHavingURI,
  } = window.docShell.chromeEventHandler.ownerGlobal;
  switchToTabHavingURI(url, true, {});
}

export const TabsSetupFlowManager = new (class {
  constructor() {
    this.QueryInterface = ChromeUtils.generateQI(["nsIObserver"]);

    this.setupState = new Map();
    this.resetInternalState();

    this.registerSetupState({
      uiStateIndex: 0,
      name: "not-signed-in",
      exitConditions: () => {
        return this.fxaSignedIn;
      },
    });
    // TODO: handle offline, sync service not ready or available
    this.registerSetupState({
      uiStateIndex: 1,
      name: "connect-secondary-device",
      exitConditions: () => {
        return this.secondaryDeviceConnected;
      },
    });
    this.registerSetupState({
      uiStateIndex: 2,
      name: "disabled-tab-sync",
      exitConditions: () => {
        return this.syncTabsPrefEnabled;
      },
    });
    this.registerSetupState({
      uiStateIndex: 3,
      name: "synced-tabs-not-ready",
      enter: () => {
        if (!this.didRecentTabSync) {
          lazy.SyncedTabs.syncTabs();
        }
      },
      exitConditions: () => {
        return this.didRecentTabSync;
      },
    });
    this.registerSetupState({
      uiStateIndex: 4,
      name: "synced-tabs-loaded",
      exitConditions: () => {
        // This is the end state
        return false;
      },
    });

    Services.obs.addObserver(this, lazy.UIState.ON_UPDATE);
    Services.obs.addObserver(this, TOPIC_DEVICELIST_UPDATED);

    // this.syncTabsPrefEnabled will track the value of the tabs pref
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "syncTabsPrefEnabled",
      SYNC_TABS_PREF,
      false,
      () => {
        this._uiUpdateNeeded = true;
        this.maybeUpdateUI();
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "lastTabFetch",
      RECENT_TABS_SYNC,
      false,
      () => {
        this._uiUpdateNeeded = true;
        this.maybeUpdateUI();
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "mobilePromoDismissedPref",
      MOBILE_PROMO_DISMISSED_PREF,
      false,
      () => {
        this._uiUpdateNeeded = true;
        this.maybeUpdateUI();
      }
    );

    this._uiUpdateNeeded = true;
    if (this.fxaSignedIn) {
      this.refreshDevices();
    }
    this.maybeUpdateUI();
  }

  resetInternalState() {
    // assign initial values for all the managed internal properties
    this._currentSetupStateName = "not-signed-in";
    this._shouldShowSuccessConfirmation = false;
    this._didShowMobilePromo = false;
    this._uiUpdateNeeded = true;

    // keep track of what is connected so we can respond to changes
    this._deviceStateSnapshot = {
      mobileDeviceConnected: this.mobileDeviceConnected,
      secondaryDeviceConnected: this.secondaryDeviceConnected,
    };
  }
  uninit() {
    Services.obs.removeObserver(this, lazy.UIState.ON_UPDATE);
    Services.obs.removeObserver(this, TOPIC_DEVICELIST_UPDATED);
  }
  get currentSetupState() {
    return this.setupState.get(this._currentSetupStateName);
  }
  get uiStateIndex() {
    return this.currentSetupState.uiStateIndex;
  }
  get fxaSignedIn() {
    let { UIState } = lazy;
    return (
      UIState.isReady() && UIState.get().status === UIState.STATUS_SIGNED_IN
    );
  }
  get secondaryDeviceConnected() {
    if (!this.fxaSignedIn) {
      return false;
    }
    let recentDevices = lazy.fxAccounts.device?.recentDeviceList?.length;
    return recentDevices > 1;
  }
  get didRecentTabSync() {
    const nowSeconds = Math.floor(Date.now() / 1000);
    return (
      nowSeconds - this.lastTabFetch <
      lazy.SyncedTabs.TABS_FRESH_ENOUGH_INTERVAL_SECONDS
    );
  }
  get mobileDeviceConnected() {
    if (!this.fxaSignedIn) {
      return false;
    }
    let mobileClients = lazy.fxAccounts.device.recentDeviceList?.filter(
      device => device.type == "mobile"
    );
    return mobileClients?.length > 0;
  }
  get shouldShowMobilePromo() {
    return (
      this.currentSetupState.uiStateIndex >= 3 &&
      !this.mobileDeviceConnected &&
      !this.mobilePromoDismissedPref
    );
  }
  get shouldShowMobileConnectedSuccess() {
    return (
      this.currentSetupState.uiStateIndex >= 3 &&
      this._shouldShowSuccessConfirmation &&
      this.mobileDeviceConnected
    );
  }
  get logger() {
    if (!this._log) {
      let setupLog = lazy.Log.repository.getLogger("FirefoxView.TabsSetup");
      setupLog.manageLevelFromPref(LOGGING_PREF);
      setupLog.addAppender(
        new lazy.Log.ConsoleAppender(new lazy.Log.BasicFormatter())
      );
      this._log = setupLog;
    }
    return this._log;
  }

  registerSetupState(state) {
    this.setupState.set(state.name, state);
  }

  async observe(subject, topic, data) {
    switch (topic) {
      case lazy.UIState.ON_UPDATE:
        this.logger.debug("Handling UIState update");
        this.maybeUpdateUI();
        break;
      case TOPIC_DEVICELIST_UPDATED:
        this.logger.debug("Handling observer notification:", topic, data);
        this.refreshDevices();
        this.maybeUpdateUI();
        break;
    }
  }

  refreshDevices() {
    // compare new values to the previous values
    const mobileDeviceConnected = this.mobileDeviceConnected;
    const secondaryDeviceConnected = this.secondaryDeviceConnected;

    this.logger.debug(
      `refreshDevices, mobileDeviceConnected: ${mobileDeviceConnected}, `,
      `secondaryDeviceConnected: ${secondaryDeviceConnected}`
    );

    let didDeviceStateChange =
      this._deviceStateSnapshot.mobileDeviceConnected !=
        mobileDeviceConnected ||
      this._deviceStateSnapshot.secondaryDeviceConnected !=
        secondaryDeviceConnected;
    if (
      mobileDeviceConnected &&
      !this._deviceStateSnapshot.mobileDeviceConnected
    ) {
      // a mobile device was added, show success if we previously showed the promo
      this._shouldShowSuccessConfirmation = this._didShowMobilePromo;
    } else if (
      !mobileDeviceConnected &&
      this._deviceStateSnapshot.mobileDeviceConnected
    ) {
      // no mobile device connected now, reset
      Services.prefs.clearUserPref(MOBILE_PROMO_DISMISSED_PREF);
      this._shouldShowSuccessConfirmation = false;
    }
    this._deviceStateSnapshot = {
      mobileDeviceConnected,
      secondaryDeviceConnected,
    };
    if (didDeviceStateChange) {
      this.logger.debug(
        "refreshDevices: device state did change, call maybeUpdateUI"
      );
      this._uiUpdateNeeded = true;
    } else {
      this.logger.debug("refreshDevices: no device state change");
    }
  }

  maybeUpdateUI() {
    let nextSetupStateName = this._currentSetupStateName;

    // state transition conditions
    for (let state of this.setupState.values()) {
      nextSetupStateName = state.name;
      if (!state.exitConditions()) {
        break;
      }
    }

    let setupState = this.currentSetupState;
    if (nextSetupStateName != this._currentSetupStateName) {
      setupState = this.setupState.get(nextSetupStateName);
      this._currentSetupStateName = nextSetupStateName;
      this._uiUpdateNeeded = true;
    }
    this.logger.debug("maybeUpdateUI, _uiUpdateNeeded:", this._uiUpdateNeeded);
    if (this._uiUpdateNeeded) {
      this._uiUpdateNeeded = false;
      if (this.shouldShowMobilePromo) {
        this._didShowMobilePromo = true;
      }
      Services.obs.notifyObservers(null, TOPIC_SETUPSTATE_CHANGED);
    }
    if ("function" == typeof setupState.enter) {
      setupState.enter();
    }
  }

  dismissMobilePromo() {
    Services.prefs.setBoolPref(MOBILE_PROMO_DISMISSED_PREF, true);
  }

  dismissMobileConfirmation() {
    this._shouldShowSuccessConfirmation = false;
    this._didShowMobilePromo = false;
    this._uiUpdateNeeded = true;
    this.maybeUpdateUI();
  }

  async openFxASignup(window) {
    const url = await lazy.fxAccounts.constructor.config.promiseConnectAccountURI(
      "firefoxview"
    );
    openTabInWindow(window, url, true);
  }

  openSyncPreferences(window) {
    const url = "about:preferences?action=pair#sync";
    openTabInWindow(window, url, true);
  }

  syncOpenTabs(containerElem) {
    // Flip the pref on.
    // The observer should trigger re-evaluating state and advance to next step
    Services.prefs.setBoolPref(SYNC_TABS_PREF, true);
  }
})();
