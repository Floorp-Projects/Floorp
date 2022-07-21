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
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";

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
    this._currentSetupStateName = "";

    Services.obs.addObserver(this, lazy.UIState.ON_UPDATE);
    Services.obs.addObserver(this, "fxaccounts:device_connected");
    Services.obs.addObserver(this, "fxaccounts:device_disconnected");

    // this.syncTabsPrefEnabled will track the value of the tabs pref
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "syncTabsPrefEnabled",
      SYNC_TABS_PREF,
      false,
      () => {
        this.maybeUpdateUI();
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "lastTabFetch",
      RECENT_TABS_SYNC,
      false,
      () => {
        this.maybeUpdateUI();
      }
    );

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
      name: "connect-mobile-device",
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

    // attempting to resolve the fxa user is a proxy for readiness
    lazy.fxAccounts.getSignedInUser().then(() => {
      this.maybeUpdateUI();
    });
  }
  uninit() {
    Services.obs.removeObserver(this, lazy.UIState.ON_UPDATE);
    Services.obs.removeObserver(this, "fxaccounts:device_connected");
    Services.obs.removeObserver(this, "fxaccounts:device_disconnected");
  }
  get uiStateIndex() {
    const state =
      this._currentSetupStateName &&
      this.setupState.get(this._currentSetupStateName);
    return state ? state.uiStateIndex : -1;
  }
  get fxaSignedIn() {
    return lazy.UIState.get().status === lazy.UIState.STATUS_SIGNED_IN;
  }
  get secondaryDeviceConnected() {
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
  registerSetupState(state) {
    this.setupState.set(state.name, state);
  }

  async observe(subject, topic, data) {
    switch (topic) {
      case lazy.UIState.ON_UPDATE:
        this.maybeUpdateUI();
        break;
      case "fxaccounts:device_connected":
      case "fxaccounts:device_disconnected":
        await lazy.fxAccounts.device.refreshDeviceList();
        this.maybeUpdateUI();
        break;
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

    if (nextSetupStateName !== this._currentSetupStateName) {
      const state = this.setupState.get(nextSetupStateName);
      this._currentSetupStateName = nextSetupStateName;

      Services.obs.notifyObservers(
        null,
        TOPIC_SETUPSTATE_CHANGED,
        state.uiStateIndex
      );
      if ("function" == typeof state.enter) {
        state.enter();
      }
    }
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
