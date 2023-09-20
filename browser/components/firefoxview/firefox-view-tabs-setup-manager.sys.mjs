/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports the TabsSetupFlowManager singleton, which manages the state and
 * diverse inputs which drive the Firefox View synced tabs setup flow
 */

import { PromiseUtils } from "resource://gre/modules/PromiseUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "resource://gre/modules/Log.sys.mjs",
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
  SyncedTabsErrorHandler:
    "resource:///modules/firefox-view-synced-tabs-error-handler.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "syncUtils", () => {
  return ChromeUtils.importESModule("resource://services-sync/util.sys.mjs")
    .Utils;
});

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

const SYNC_TABS_PREF = "services.sync.engine.tabs";
const TOPIC_TABS_CHANGED = "services.sync.tabs.changed";
const MOBILE_PROMO_DISMISSED_PREF =
  "browser.tabs.firefox-view.mobilePromo.dismissed";
const LOGGING_PREF = "browser.tabs.firefox-view.logLevel";
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";
const TOPIC_DEVICESTATE_CHANGED = "firefox-view.devicestate.changed";
const TOPIC_DEVICELIST_UPDATED = "fxaccounts:devicelist_updated";
const NETWORK_STATUS_CHANGED = "network:offline-status-changed";
const SYNC_SERVICE_ERROR = "weave:service:sync:error";
const FXA_DEVICE_CONNECTED = "fxaccounts:device_connected";
const FXA_DEVICE_DISCONNECTED = "fxaccounts:device_disconnected";
const SYNC_SERVICE_FINISHED = "weave:service:sync:finish";
const PRIMARY_PASSWORD_UNLOCKED = "passwordmgr-crypto-login";
const TAB_PICKUP_OPEN_STATE_PREF =
  "browser.tabs.firefox-view.ui-state.tab-pickup.open";

function openTabInWindow(window, url) {
  const { switchToTabHavingURI } =
    window.docShell.chromeEventHandler.ownerGlobal;
  switchToTabHavingURI(url, true, {});
}

function isFirefoxViewNext(window) {
  return window.location.pathname === "firefoxview-next";
}

export const TabsSetupFlowManager = new (class {
  constructor() {
    this.QueryInterface = ChromeUtils.generateQI(["nsIObserver"]);

    this.setupState = new Map();
    this.resetInternalState();
    this._currentSetupStateName = "";
    this.syncIsConnected = lazy.UIState.get().syncEnabled;
    this.didFxaTabOpen = false;

    this.registerSetupState({
      uiStateIndex: 0,
      name: "error-state",
      exitConditions: () => {
        return lazy.SyncedTabsErrorHandler.isSyncReady();
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
    Services.obs.addObserver(this, FXA_DEVICE_CONNECTED);
    Services.obs.addObserver(this, FXA_DEVICE_DISCONNECTED);

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
    this.abortWaitingForTabs();

    Services.obs.notifyObservers(null, TOPIC_DEVICESTATE_CHANGED);

    // keep track of what is connected so we can respond to changes
    this._deviceStateSnapshot = {
      mobileDeviceConnected: this.mobileDeviceConnected,
      secondaryDeviceConnected: this.secondaryDeviceConnected,
    };
    // keep track of tab-pickup-container instance visibilities
    this._viewVisibilityStates = new Map();
  }

  get isPrimaryPasswordLocked() {
    return lazy.syncUtils.mpLocked();
  }

  uninit() {
    Services.obs.removeObserver(this, lazy.UIState.ON_UPDATE);
    Services.obs.removeObserver(this, TOPIC_DEVICELIST_UPDATED);
    Services.obs.removeObserver(this, NETWORK_STATUS_CHANGED);
    Services.obs.removeObserver(this, SYNC_SERVICE_ERROR);
    Services.obs.removeObserver(this, SYNC_SERVICE_FINISHED);
    Services.obs.removeObserver(this, TOPIC_TABS_CHANGED);
    Services.obs.removeObserver(this, PRIMARY_PASSWORD_UNLOCKED);
    Services.obs.removeObserver(this, FXA_DEVICE_CONNECTED);
    Services.obs.removeObserver(this, FXA_DEVICE_DISCONNECTED);
  }
  get hasVisibleViews() {
    return Array.from(this._viewVisibilityStates.values()).reduce(
      (hasVisible, visibility) => {
        return hasVisible || visibility == "visible";
      },
      false
    );
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
          await this.maybeUpdateUI();
        }
        this._lastFxASignedIn = this.fxaSignedIn;
        break;
      case TOPIC_DEVICELIST_UPDATED:
        this.logger.debug("Handling observer notification:", topic, data);
        const { deviceStateChanged, deviceAdded } = await this.refreshDevices();
        if (deviceStateChanged) {
          await this.maybeUpdateUI(true);
        }
        if (deviceAdded && this.secondaryDeviceConnected) {
          this.logger.debug("device was added");
          this._deviceAddedResultsNeverSeen = true;
          if (this.hasVisibleViews) {
            this.startWaitingForNewDeviceTabs();
          }
        }
        break;
      case FXA_DEVICE_CONNECTED:
      case FXA_DEVICE_DISCONNECTED:
        await lazy.fxAccounts.device.refreshDeviceList({ ignoreCached: true });
        await this.maybeUpdateUI(true);
        break;
      case SYNC_SERVICE_ERROR:
        this.logger.debug(`Handling ${SYNC_SERVICE_ERROR}`);
        if (lazy.UIState.get().status == lazy.UIState.STATUS_SIGNED_IN) {
          this.abortWaitingForTabs();
          await this.maybeUpdateUI(true);
        }
        break;
      case NETWORK_STATUS_CHANGED:
        this.abortWaitingForTabs();
        await this.maybeUpdateUI(true);
        break;
      case SYNC_SERVICE_FINISHED:
        this.logger.debug(`Handling ${SYNC_SERVICE_FINISHED}`);
        // We intentionally leave any empty-tabs timestamp
        // as we may be still waiting for a sync that delivers some tabs
        this._waitingForNextTabSync = false;
        await this.maybeUpdateUI(true);
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

  updateViewVisibility(instanceId, visibility) {
    const wasVisible = this.hasVisibleViews;
    this.logger.debug(
      `updateViewVisibility for instance: ${instanceId}, visibility: ${visibility}`
    );
    if (visibility == "unloaded") {
      this._viewVisibilityStates.delete(instanceId);
    } else {
      this._viewVisibilityStates.set(instanceId, visibility);
    }
    const isVisible = this.hasVisibleViews;
    if (isVisible && !wasVisible) {
      // If we're already timing waiting for tabs from a newly-added device
      // we might be able to stop
      if (this._noTabsVisibleFromAddedDeviceTimestamp) {
        return this.stopWaitingForNewDeviceTabs();
      }
      if (this._deviceAddedResultsNeverSeen) {
        // If this is the first time a view has been visible since a device was added
        // we may want to start the empty-tabs visible timer
        return this.startWaitingForNewDeviceTabs();
      }
    }
    if (!isVisible) {
      this.logger.debug(
        "Resetting timestamp and tabs pending flags as there are no visible views"
      );
      // if there's no view visible, we're not really waiting anymore
      this.abortWaitingForTabs();
    }
    return null;
  }

  get waitingForTabs() {
    return (
      // signed in & at least 1 other device is syncing indicates there's something to wait for
      this.secondaryDeviceConnected && this._waitingForNextTabSync
    );
  }

  abortWaitingForTabs() {
    this._waitingForNextTabSync = false;
    // also clear out the device-added / tabs pending flags
    this._noTabsVisibleFromAddedDeviceTimestamp = 0;
    this._deviceAddedResultsNeverSeen = false;
  }

  startWaitingForTabs() {
    if (!this._waitingForNextTabSync) {
      this._waitingForNextTabSync = true;
      Services.obs.notifyObservers(null, TOPIC_SETUPSTATE_CHANGED);
    }
  }

  async stopWaitingForTabs() {
    const wasWaiting = this.waitingForTabs;
    if (this.hasVisibleViews && this._deviceAddedResultsNeverSeen) {
      await this.stopWaitingForNewDeviceTabs();
    }
    this._waitingForNextTabSync = false;
    if (wasWaiting) {
      Services.obs.notifyObservers(null, TOPIC_SETUPSTATE_CHANGED);
    }
  }

  async onSignedInChange() {
    this.logger.debug("onSignedInChange, fxaSignedIn:", this.fxaSignedIn);
    // update UI to make the state change
    await this.maybeUpdateUI(true);
    if (!this.fxaSignedIn) {
      // As we just signed out, ensure the waiting flag is reset for next time around
      this.abortWaitingForTabs();
      return;
    }

    // Set Tab pickup open state pref to true when signing in
    Services.prefs.setBoolPref(TAB_PICKUP_OPEN_STATE_PREF, true);

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
    const { deviceStateChanged } = await this.refreshDevices();
    if (deviceStateChanged) {
      this.logger.debug(
        "onSignedInChange, after refreshDevices, calling maybeUpdateUI"
      );
      // give the UI an opportunity to update as secondaryDeviceConnected or
      // mobileDeviceConnected have changed value
      await this.maybeUpdateUI(true);
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
      this.syncTabs()
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

  async startWaitingForNewDeviceTabs() {
    // if we're already waiting for tabs, don't reset
    if (this._noTabsVisibleFromAddedDeviceTimestamp) {
      return;
    }

    // take a timestamp whenever the latest device is added and we have 0 tabs to show,
    // allowing us to track how long we show an empty list after a new device is added
    const hasRecentTabs = (await lazy.SyncedTabs.getRecentTabs(1)).length;
    if (this.hasVisibleViews && !hasRecentTabs) {
      this._noTabsVisibleFromAddedDeviceTimestamp = Date.now();
      this.logger.debug(
        "New device added with 0 synced tabs to show, storing timestamp:",
        this._noTabsVisibleFromAddedDeviceTimestamp
      );
    }
  }

  async stopWaitingForNewDeviceTabs() {
    if (!this._noTabsVisibleFromAddedDeviceTimestamp) {
      return;
    }
    const recentTabs = await lazy.SyncedTabs.getRecentTabs(1);
    if (recentTabs.length) {
      // We have been waiting for > 0 tabs after a newly-added device, record
      // the time elapsed
      const elapsed = Date.now() - this._noTabsVisibleFromAddedDeviceTimestamp;
      this.logger.debug(
        "stopWaitingForTabs, resetting _noTabsVisibleFromAddedDeviceTimestamp and recording telemetry:",
        Math.round(elapsed / 1000)
      );
      this._noTabsVisibleFromAddedDeviceTimestamp = 0;
      this._deviceAddedResultsNeverSeen = false;
      Services.telemetry.recordEvent(
        "firefoxview",
        "synced_tabs_empty",
        "since_device_added",
        Math.round(elapsed / 1000).toString()
      );
    } else {
      // we are still waiting for some tabs to show...
      this.logger.debug(
        "stopWaitingForTabs: Still no recent tabs, we are still waiting"
      );
    }
  }

  async refreshDevices() {
    // If current device not found in recent device list, refresh device list
    if (
      !lazy.fxAccounts.device.recentDeviceList?.some(
        device => device.isCurrentDevice
      )
    ) {
      await lazy.fxAccounts.device.refreshDeviceList({ ignoreCached: true });
    }

    // compare new values to the previous values
    const mobileDeviceConnected = this.mobileDeviceConnected;
    const secondaryDeviceConnected = this.secondaryDeviceConnected;
    const oldDevicesCount = this._deviceStateSnapshot?.devicesCount ?? 0;
    const devicesCount = lazy.fxAccounts.device?.recentDeviceList?.length ?? 0;

    this.logger.debug(
      `refreshDevices, mobileDeviceConnected: ${mobileDeviceConnected}, `,
      `secondaryDeviceConnected: ${secondaryDeviceConnected}`
    );

    let deviceStateChanged =
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
      devicesCount,
    };
    if (deviceStateChanged) {
      this.logger.debug("refreshDevices: device state did change");
      if (!secondaryDeviceConnected) {
        this.logger.debug(
          "We lost a device, now claim sync hasn't worked before."
        );
        Services.obs.notifyObservers(null, TOPIC_DEVICESTATE_CHANGED);
      }
    } else {
      this.logger.debug("refreshDevices: no device state change");
    }
    return {
      deviceStateChanged,
      deviceAdded: oldDevicesCount < devicesCount,
    };
  }

  async maybeUpdateUI(forceUpdate = false) {
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
        // Use idleDispatch() to give observers a chance to resolve before
        // determining the new state.
        errorState = await PromiseUtils.idleDispatch(() =>
          lazy.SyncedTabsErrorHandler.getErrorType()
        );
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
    if (!(await lazy.fxAccounts.constructor.canConnectAccount())) {
      return;
    }
    const url =
      await lazy.fxAccounts.constructor.config.promiseConnectAccountURI(
        "fx-view"
      );
    this.didFxaTabOpen = true;
    openTabInWindow(window, url, true);
    const category = isFirefoxViewNext(window)
      ? "firefoxview_next"
      : "firefoxview";
    Services.telemetry.recordEvent(category, "fxa_continue", "sync", null);
  }

  async openFxAPairDevice(window) {
    const url = await lazy.fxAccounts.constructor.config.promisePairingURI({
      entrypoint: "fx-view",
    });
    this.didFxaTabOpen = true;
    openTabInWindow(window, url, true);
    const category = isFirefoxViewNext(window)
      ? "firefoxview_next"
      : "firefoxview";
    Services.telemetry.recordEvent(category, "fxa_mobile", "sync", null, {
      has_devices: this.secondaryDeviceConnected.toString(),
    });
  }

  syncOpenTabs(containerElem) {
    // Flip the pref on.
    // The observer should trigger re-evaluating state and advance to next step
    Services.prefs.setBoolPref(SYNC_TABS_PREF, true);
  }

  async syncOnPageReload() {
    if (lazy.UIState.isReady() && this.fxaSignedIn) {
      this.startWaitingForTabs();
      await this.syncTabs(true);
    }
  }

  tryToClearError() {
    if (lazy.UIState.isReady() && this.fxaSignedIn) {
      this.startWaitingForTabs();
      if (this.isPrimaryPasswordLocked) {
        lazy.syncUtils.ensureMPUnlocked();
      }
      this.logger.debug("tryToClearError: triggering new tab sync");
      this.syncTabs();
      Services.tm.dispatchToMainThread(() => {});
    } else {
      this.logger.debug(
        `tryToClearError: unable to sync, isReady: ${lazy.UIState.isReady()}, fxaSignedIn: ${
          this.fxaSignedIn
        }`
      );
    }
  }
  // For easy overriding in tests
  syncTabs(force = false) {
    return lazy.SyncedTabs.syncTabs(force);
  }
})();
