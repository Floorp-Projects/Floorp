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

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

const SYNC_TABS_PREF = "services.sync.engine.tabs";
const RECENT_TABS_SYNC = "services.sync.lastTabFetch";
const MOBILE_PROMO_DISMISSED_PREF =
  "browser.tabs.firefox-view.mobilePromo.dismissed";
const LOGGING_PREF = "browser.tabs.firefox-view.logLevel";
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";
const TOPIC_DEVICELIST_UPDATED = "fxaccounts:devicelist_updated";
const NETWORK_STATUS_CHANGED = "network:offline-status-changed";
const SYNC_SERVICE_ERROR = "weave:service:sync:error";
const FXA_ENABLED = "identity.fxaccounts.enabled";
const SYNC_SERVICE_FINISHED = "weave:service:sync:finish";

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
    this._currentSetupStateName = "";
    this.networkIsOnline =
      lazy.gNetworkLinkService.linkStatusKnown &&
      lazy.gNetworkLinkService.isLinkUp;
    this.syncIsWorking = true;
    this.syncIsConnected = lazy.UIState.get().syncEnabled;

    this.registerSetupState({
      uiStateIndex: 0,
      name: "error-state",
      exitConditions: () => {
        return (
          this.networkIsOnline &&
          (this.syncIsWorking || this.syncHasWorked) &&
          !Services.prefs.prefIsLocked(FXA_ENABLED) &&
          // its only an error for sync to not be connected if we are signed-in.
          (this.syncIsConnected || !this.fxaSignedIn)
        );
      },
    });
    this.registerSetupState({
      uiStateIndex: 1,
      name: "not-signed-in",
      exitConditions: () => {
        return this.fxaSignedIn;
      },
    });
    this.registerSetupState({
      uiStateIndex: 2,
      name: "connect-secondary-device",
      exitConditions: () => {
        return this.secondaryDeviceConnected;
      },
    });
    this.registerSetupState({
      uiStateIndex: 3,
      name: "disabled-tab-sync",
      exitConditions: () => {
        return this.syncTabsPrefEnabled;
      },
    });
    this.registerSetupState({
      uiStateIndex: 4,
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
      uiStateIndex: 5,
      name: "synced-tabs-loaded",
      exitConditions: () => {
        // This is the end state
        return false;
      },
    });

    Services.obs.addObserver(this, lazy.UIState.ON_UPDATE);
    Services.obs.addObserver(this, TOPIC_DEVICELIST_UPDATED);
    Services.obs.addObserver(this, NETWORK_STATUS_CHANGED);
    Services.obs.addObserver(this, SYNC_SERVICE_ERROR);
    Services.obs.addObserver(this, SYNC_SERVICE_FINISHED);

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
      0,
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

    this.syncHasWorked = false;

    // keep track of what is connected so we can respond to changes
    this._deviceStateSnapshot = {
      mobileDeviceConnected: this.mobileDeviceConnected,
      secondaryDeviceConnected: this.secondaryDeviceConnected,
    };
  }

  getErrorType() {
    // this ordering is important for dealing with multiple errors at once
    const errorStates = {
      "network-offline": !this.networkIsOnline,
      "sync-error": !this.syncIsWorking && !this.syncHasWorked,
      "fxa-admin-disabled": Services.prefs.prefIsLocked(FXA_ENABLED),
      "sync-disconnected": !this.syncIsConnected,
    };

    for (let [type, value] of Object.entries(errorStates)) {
      if (value) {
        return type;
      }
    }
    return null;
  }

  uninit() {
    Services.obs.removeObserver(this, lazy.UIState.ON_UPDATE);
    Services.obs.removeObserver(this, TOPIC_DEVICELIST_UPDATED);
    Services.obs.removeObserver(this, NETWORK_STATUS_CHANGED);
    Services.obs.removeObserver(this, SYNC_SERVICE_ERROR);
    Services.obs.removeObserver(this, SYNC_SERVICE_FINISHED);
  }
  get currentSetupState() {
    return this.setupState.get(this._currentSetupStateName);
  }
  get isTabSyncSetupComplete() {
    return this.currentSetupState.uiStateIndex >= 4;
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
      this.syncIsConnected &&
      this.fxaSignedIn &&
      this.currentSetupState.uiStateIndex >= 4 &&
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
        this.syncIsConnected = lazy.UIState.get().syncEnabled;
        this.maybeUpdateUI();
        break;
      case TOPIC_DEVICELIST_UPDATED:
        this.logger.debug("Handling observer notification:", topic, data);
        this.refreshDevices();
        this.maybeUpdateUI();
        break;
      case "fxaccounts:device_connected":
      case "fxaccounts:device_disconnected":
        await lazy.fxAccounts.device.refreshDeviceList();
        this.maybeUpdateUI();
        break;
      case SYNC_SERVICE_ERROR:
        this.syncIsWorking = false;
        this.maybeUpdateUI();
        break;
      case NETWORK_STATUS_CHANGED:
        this.networkIsOnline = data == "online";
        this.maybeUpdateUI();
        break;
      case SYNC_SERVICE_FINISHED:
        if (!this.syncIsWorking) {
          this.syncIsWorking = true;
          this.syncHasWorked = true;
          this.maybeUpdateUI();
        }
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
      if (!secondaryDeviceConnected) {
        this.logger.debug(
          "We lost a device, now claim sync hasn't worked before."
        );
        this.syncHasWorked = false;
      }
      this._uiUpdateNeeded = true;
    } else {
      this.logger.debug("refreshDevices: no device state change");
    }
  }

  maybeUpdateUI() {
    let nextSetupStateName = this._currentSetupStateName;
    let errorState = null;

    // state transition conditions
    for (let state of this.setupState.values()) {
      nextSetupStateName = state.name;
      if (!state.exitConditions()) {
        this.logger.debug(
          "maybeUpdateUI, conditions not met to exit state: ",
          nextSetupStateName
        );
        break;
      }
    }

    let setupState = this.currentSetupState;
    const state = this.setupState.get(nextSetupStateName);
    const uiStateIndex = state.uiStateIndex;

    if (
      uiStateIndex == 0 ||
      nextSetupStateName != this._currentSetupStateName
    ) {
      setupState = state;
      this._currentSetupStateName = nextSetupStateName;
      this._uiUpdateNeeded = true;
    }
    this.logger.debug("maybeUpdateUI, _uiUpdateNeeded:", this._uiUpdateNeeded);
    if (this._uiUpdateNeeded) {
      this._uiUpdateNeeded = false;
      if (this.shouldShowMobilePromo) {
        this._didShowMobilePromo = true;
      }
      if (uiStateIndex == 0) {
        errorState = this.getErrorType();
      }
      Services.obs.notifyObservers(null, TOPIC_SETUPSTATE_CHANGED, errorState);
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
    Services.telemetry.recordEvent("firefoxview", "fxa_continue", "sync", null);
  }

  async openFxAPairDevice(window) {
    const url = await lazy.fxAccounts.constructor.config.promisePairingURI({
      entrypoint: "fx-view",
    });
    openTabInWindow(window, url, true);
    Services.telemetry.recordEvent("firefoxview", "fxa_mobile", "sync", null, {
      has_devices: this.secondaryDeviceConnected.toString(),
    });
  }

  syncOpenTabs(containerElem) {
    // Flip the pref on.
    // The observer should trigger re-evaluating state and advance to next step
    Services.prefs.setBoolPref(SYNC_TABS_PREF, true);
  }

  forceSyncTabs() {
    lazy.SyncedTabs.syncTabs(true);
  }
})();
