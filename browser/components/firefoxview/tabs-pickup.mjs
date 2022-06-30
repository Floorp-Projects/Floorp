/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const { switchToTabHavingURI } = window.docShell.chromeEventHandler.ownerGlobal;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const SYNC_TABS_PREF = "services.sync.engine.tabs";
const RECENT_TABS_SYNC = "services.sync.lastTabFetch";

const tabsSetupFlowManager = new (class {
  constructor() {
    this.QueryInterface = ChromeUtils.generateQI(["nsIObserver"]);

    this.setupState = new Map();
    this._currentSetupStateName = "";
    this.sync = {};

    XPCOMUtils.defineLazyModuleGetters(this, {
      Services: "resource://gre/modules/Services.jsm",
      SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
    });
    XPCOMUtils.defineLazyGetter(this, "fxAccounts", () => {
      return ChromeUtils.import(
        "resource://gre/modules/FxAccounts.jsm"
      ).getFxAccountsSingleton();
    });
    ChromeUtils.defineModuleGetter(
      this.sync,
      "UIState",
      "resource://services-sync/UIState.jsm"
    );

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
          this.SyncedTabs.syncTabs();
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
  }
  async initialize(elem) {
    this.elem = elem;
    this.elem.addEventListener("click", this);
    this.Services.obs.addObserver(this, this.sync.UIState.ON_UPDATE);
    this.Services.obs.addObserver(this, "fxaccounts:device_connected");
    this.Services.obs.addObserver(this, "fxaccounts:device_disconnected");

    await this.fxAccounts.getSignedInUser();
    this.maybeUpdateUI();
  }
  uninit() {
    this.Services.obs.removeObserver(this, this.sync.UIState.ON_UPDATE);
    this.Services.obs.removeObserver(this, "fxaccounts:device_connected");
    this.Services.obs.removeObserver(this, "fxaccounts:device_disconnected");
  }
  get fxaSignedIn() {
    return (
      this.sync.UIState.get().status === this.sync.UIState.STATUS_SIGNED_IN
    );
  }
  get secondaryDeviceConnected() {
    let recentDevices = this.fxAccounts.device?.recentDeviceList?.length;
    return recentDevices > 1;
  }
  get didRecentTabSync() {
    const nowSeconds = Math.floor(Date.now() / 1000);
    return (
      nowSeconds - this.lastTabFetch <
      this.SyncedTabs.TABS_FRESH_ENOUGH_INTERVAL_SECONDS
    );
  }
  registerSetupState(state) {
    this.setupState.set(state.name, state);
  }

  async observe(subject, topic, data) {
    switch (topic) {
      case this.sync.UIState.ON_UPDATE:
        this.maybeUpdateUI();
        break;
      case "fxaccounts:device_connected":
      case "fxaccounts:device_disconnected":
        await this.fxAccounts.device.refreshDeviceList();
        this.maybeUpdateUI();
        break;
    }
  }

  handleEvent(event) {
    if (event.type == "click" && event.target.dataset.action) {
      switch (event.target.dataset.action) {
        case "view0-primary-action": {
          this.openFxASignup(event.target);
          break;
        }
        case "view1-primary-action": {
          this.openSyncPreferences(event.target);
          break;
        }
        case "view2-primary-action": {
          this.syncOpenTabs(event.target);
          break;
        }
      }
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
      this.elem.updateSetupState(state.uiStateIndex);
      this._currentSetupStateName = nextSetupStateName;
      if ("function" == typeof state.enter) {
        state.enter();
      }
    }
  }

  async openFxASignup() {
    const url = await this.fxAccounts.constructor.config.promiseConnectAccountURI(
      "firefoxview"
    );
    switchToTabHavingURI(url, true);
  }
  openSyncPreferences(containerElem) {
    const url = "about:preferences?action=pair#sync";
    switchToTabHavingURI(url, true);
  }
  syncOpenTabs(containerElem) {
    // Flip the pref on.
    // The observer should trigger re-evaluating state and advance to next step
    this.Services.prefs.setBoolPref(SYNC_TABS_PREF, true);
  }
})();

class TabsPickupContainer extends HTMLElement {
  constructor() {
    super();
    this.manager = null;
    this._currentSetupStateIndex = -1;
    XPCOMUtils.defineLazyModuleGetters(this, {
      Services: "resource://gre/modules/Services.jsm",
    });
  }
  get setupContainerElem() {
    return this.querySelector(".sync-setup-container");
  }
  get tabsContainerElem() {
    return this.querySelector(".synced-tabs-container");
  }
  appendTemplatedElement(templateId, elementId) {
    const template = document.getElementById(templateId);
    const templateContent = template.content;
    const cloned = templateContent.cloneNode(true);
    if (elementId) {
      // populate id-prefixed attributes on elements that need them
      for (let elem of cloned.querySelectorAll("[data-prefix]")) {
        let [name, value] = elem.dataset.prefix
          .split(":")
          .map(str => str.trim());
        elem.setAttribute(name, elementId + value);
        delete elem.dataset.prefix;
      }
      for (let elem of cloned.querySelectorAll("a[data-support-url]")) {
        elem.href =
          this.Services.urlFormatter.formatURLPref("app.support.baseURL") +
          elem.dataset.supportUrl;
      }
    }
    this.appendChild(cloned);
  }
  updateSetupState(stateIndex) {
    const currStateIndex = this._currentSetupStateIndex;
    if (stateIndex === undefined) {
      stateIndex = currStateIndex;
    }
    if (stateIndex === this._currentSetupStateIndex) {
      return;
    }
    this._currentSetupStateIndex = stateIndex;
    this.render();
  }
  render() {
    if (!this.isConnected) {
      return;
    }
    let setupElem = this.setupContainerElem;
    let tabsElem = this.tabsContainerElem;
    const stateIndex = this._currentSetupStateIndex;

    // show/hide either the setup or tab list containers, creating each as necessary
    if (stateIndex < 3) {
      if (!setupElem) {
        this.appendTemplatedElement("sync-setup-template", "tabpickup-steps");
        setupElem = this.setupContainerElem;
      }
      if (tabsElem) {
        tabsElem.hidden = true;
      }
      setupElem.hidden = false;
      setupElem.selectedViewName = `sync-setup-view${stateIndex}`;
      return;
    }

    if (!tabsElem) {
      this.appendTemplatedElement(
        "synced-tabs-template",
        "tabpickup-tabs-container"
      );
      tabsElem = this.tabsContainerElem;
      tabsElem.classList.toggle("loading", stateIndex == 3);
    }
    if (setupElem) {
      setupElem.hidden = true;
    }
    tabsElem.hidden = false;

    if (stateIndex == 4) {
      tabsElem.classList.toggle("loading", false);
    }
  }
}
customElements.define("tabs-pickup-container", TabsPickupContainer);

export { tabsSetupFlowManager };
