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
  Weave: "resource://services-sync/main.js",
});

XPCOMUtils.defineLazyGetter(lazy, "syncUtils", () => {
  return ChromeUtils.import("resource://services-sync/util.js").Utils;
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
const TOPIC_TABS_CHANGED = "services.sync.tabs.changed";
const MOBILE_PROMO_DISMISSED_PREF =
  "browser.tabs.firefox-view.mobilePromo.dismissed";
const LOGGING_PREF = "browser.tabs.firefox-view.logLevel";
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";
const TOPIC_DEVICELIST_UPDATED = "fxaccounts:devicelist_updated";
const NETWORK_STATUS_CHANGED = "network:offline-status-changed";
const SYNC_SERVICE_ERROR = "weave:service:sync:error";
const FXA_ENABLED = "identity.fxaccounts.enabled";
const SYNC_SERVICE_FINISHED = "weave:service:sync:finish";
const PRIMARY_PASSWORD_UNLOCKED = "passwordmgr-crypto-login";

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
          (this.syncIsConnected || !this.fxaSignedIn) &&
          // We treat a locked primary password as an error if we are signed-in.
          // If the user dismisses the prompt to unlock, they can use the "Try again" button to prompt again
          (!this.isPrimaryPasswordLocked || !this.fxaSignedIn)
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
    Services.obs.addObserver(this, TOPIC_TABS_CHANGED);
    Services.obs.addObserver(this, PRIMARY_PASSWORD_UNLOCKED);

    // this.syncTabsPrefEnabled will track the value of the tabs pref
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "syncTabsPrefEnabled",
      SYNC_TABS_PREF,
      false,
      () => {
        this.maybeUpdateUI(true);
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "mobilePromoDismissedPref",
      MOBILE_PROMO_DISMISSED_PREF,
      false,
      () => {
        this.maybeUpdateUI(true);
      }
    );

    this._lastFxASignedIn = this.fxaSignedIn;
    this.logger.debug(
      "TabsSetupFlowManager constructor, fxaSignedIn:",
      this._lastFxASignedIn
    );
    this.onSignedInChange();
  }

  resetInternalState() {
    // assign initial values for all the managed internal properties
    delete this._lastFxASignedIn;
    this._currentSetupStateName = "not-signed-in";
    this._shouldShowSuccessConfirmation = false;
    this._didShowMobilePromo = false;
    this._waitingForTabs = false;

    this.syncHasWorked = false;

    // keep track of what is connected so we can respond to changes
    this._deviceStateSnapshot = {
      mobileDeviceConnected: this.mobileDeviceConnected,
      secondaryDeviceConnected: this.secondaryDeviceConnected,
    };
  }

  get isPrimaryPasswordLocked() {
    return lazy.syncUtils.mpLocked();
  }

  getErrorType() {
    // this ordering is important for dealing with multiple errors at once
    const errorStates = {
      "network-offline": !this.networkIsOnline,
      "sync-error":
        (!this.syncIsWorking && !this.syncHasWorked) ||
        this.isPrimaryPasswordLocked,
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
    Services.obs.removeObserver(this, TOPIC_TABS_CHANGED);
    Services.obs.removeObserver(this, PRIMARY_PASSWORD_UNLOCKED);
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
    let syncState = UIState.get();
    return (
      UIState.isReady() &&
      syncState.status === UIState.STATUS_SIGNED_IN &&
      // we actually mostly care that the user is signed into sync
      // syncEnabled just checks the "services.sync.username" pref has a value
      syncState.syncEnabled
    );
  }
  get secondaryDeviceConnected() {
    if (!this.fxaSignedIn) {
      return false;
    }
    let recentDevices = lazy.fxAccounts.device?.recentDeviceList?.length;
    return recentDevices > 1;
  }
  get mobileDeviceConnected() {
    if (!this.fxaSignedIn) {
      return false;
    }
    let mobileClients = lazy.fxAccounts.device.recentDeviceList?.filter(
      device => device.type == "mobile" || device.type == "tablet"
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
        if (this._lastFxASignedIn !== this.fxaSignedIn) {
          this.onSignedInChange();
        } else {
          this.maybeUpdateUI();
        }
        this._lastFxASignedIn = this.fxaSignedIn;
        break;
      case TOPIC_DEVICELIST_UPDATED:
        this.logger.debug("Handling observer notification:", topic, data);
        if (this.refreshDevices()) {
          this.logger.debug(
            "refreshDevices made changes, calling maybeUpdateUI"
          );
          this.maybeUpdateUI(true);
        }
        break;
      case "fxaccounts:device_connected":
      case "fxaccounts:device_disconnected":
        await lazy.fxAccounts.device.refreshDeviceList();
        this.maybeUpdateUI(true);
        break;
      case SYNC_SERVICE_ERROR:
        this.logger.debug(`Handling ${SYNC_SERVICE_ERROR}`);
        this._waitingForTabs = false;
        this.syncIsWorking = false;
        this.maybeUpdateUI(true);
        break;
      case NETWORK_STATUS_CHANGED:
        this.networkIsOnline = data == "online";
        this._waitingForTabs = false;
        this.maybeUpdateUI(true);
        break;
      case SYNC_SERVICE_FINISHED:
        this.logger.debug(`Handling ${SYNC_SERVICE_FINISHED}`);
        this._waitingForTabs = false;
        if (!this.syncIsWorking) {
          this.syncIsWorking = true;
          this.syncHasWorked = true;
        }
        this.maybeUpdateUI(true);
        break;
      case TOPIC_TABS_CHANGED:
        this.stopWaitingForTabs();
        break;
      case PRIMARY_PASSWORD_UNLOCKED:
        this.logger.debug(`Handling ${PRIMARY_PASSWORD_UNLOCKED}`);
        this.tryToClearError();
        break;
    }
  }

  get waitingForTabs() {
    return (
      // signed in & at least 1 other device is sycning indicates there's something to wait for
      this.secondaryDeviceConnected &&
      // last recent tabs request came back empty and we've not had a sync finish (or error) yet
      this._waitingForTabs
    );
  }

  startWaitingForTabs() {
    if (!this._waitingForTabs) {
      this._waitingForTabs = true;
      Services.obs.notifyObservers(null, TOPIC_SETUPSTATE_CHANGED);
    }
  }

  stopWaitingForTabs() {
    if (this._waitingForTabs) {
      this._waitingForTabs = false;
      Services.obs.notifyObservers(null, TOPIC_SETUPSTATE_CHANGED);
    }
  }

  async onSignedInChange() {
    this.logger.debug("onSignedInChange, fxaSignedIn:", this.fxaSignedIn);
    // update UI to make the state change
    this.maybeUpdateUI(true);
    if (!this.fxaSignedIn) {
      // As we just signed out, ensure the waiting flag is reset for next time around
      this._waitingForTabs = false;
      return;
    }

    // Now we need to figure out if we have recently synced tabs to show
    // Or, if we are going to need to trigger a tab sync for them
    const recentTabs = await lazy.SyncedTabs.getRecentTabs(50);

    if (!this.fxaSignedIn) {
      // We got signed-out in the meantime. We should get an ON_UPDATE which will put us
      // back in the right state, so we just do nothing here
      return;
    }

    // When SyncedTabs has resolved the getRecentTabs promise,
    // we also know we can update devices-related internal state
    if (this.refreshDevices()) {
      this.logger.debug(
        "onSignedInChange, after refreshDevices, calling maybeUpdateUI"
      );
      // give the UI an opportunity to update as secondaryDeviceConnected or
      // mobileDeviceConnected have changed value
      this.maybeUpdateUI(true);
    }

    // If we can't get recent tabs, we need to trigger a request for them
    const tabSyncNeeded = !recentTabs?.length;
    this.logger.debug("onSignedInChange, tabSyncNeeded:", tabSyncNeeded);

    if (tabSyncNeeded) {
      this.startWaitingForTabs();
      this.logger.debug(
        "isPrimaryPasswordLocked:",
        this.isPrimaryPasswordLocked
      );
      this.logger.debug("onSignedInChange, no recentTabs, calling syncTabs");
      // If the syncTabs call rejects or resolves false we need to clear the waiting
      // flag and update UI
      lazy.SyncedTabs.syncTabs()
        .catch(ex => {
          this.logger.debug("onSignedInChange, syncTabs rejected:", ex);
          this.stopWaitingForTabs();
        })
        .then(willSync => {
          if (!willSync) {
            this.logger.debug("onSignedInChange, no tab sync expected");
            this.stopWaitingForTabs();
          }
        });
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
      this.logger.debug("refreshDevices: device state did change");
      if (!secondaryDeviceConnected) {
        this.logger.debug(
          "We lost a device, now claim sync hasn't worked before."
        );
        this.syncHasWorked = false;
      }
    } else {
      this.logger.debug("refreshDevices: no device state change");
    }
    return didDeviceStateChange;
  }

  maybeUpdateUI(forceUpdate = false) {
    let nextSetupStateName = this._currentSetupStateName;
    let errorState = null;
    let stateChanged = false;

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
      stateChanged = true;
    }
    this.logger.debug(
      "maybeUpdateUI, will notify update?:",
      stateChanged,
      forceUpdate
    );
    if (stateChanged || forceUpdate) {
      if (this.shouldShowMobilePromo) {
        this._didShowMobilePromo = true;
      }
      if (uiStateIndex == 0) {
        errorState = this.getErrorType();
        this.logger.debug("maybeUpdateUI, in error state:", errorState);
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
    this.maybeUpdateUI(true);
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

  tryToClearError() {
    if (lazy.UIState.isReady() && this.fxaSignedIn) {
      this.startWaitingForTabs();
      Services.tm.dispatchToMainThread(() => {
        this.logger.debug("tryToClearError: triggering new tab sync");
        lazy.Weave.Service.sync({ why: "tabs", engines: ["tabs"] });
      });
    } else {
      this.logger.debug(
        `tryToClearError: unable to sync, isReady: ${lazy.UIState.isReady()}, fxaSignedIn: ${
          this.fxaSignedIn
        }`
      );
    }
  }
})();
