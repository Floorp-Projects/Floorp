/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
});

import { SyncedTabsErrorHandler } from "resource:///modules/firefox-view-synced-tabs-error-handler.sys.mjs";
import { TabsSetupFlowManager } from "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs";
import { searchTabList } from "chrome://browser/content/firefoxview/search-helpers.mjs";

const SYNCED_TABS_CHANGED = "services.sync.tabs.changed";
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";

/**
 * The controller for synced tabs components.
 *
 * @implements {ReactiveController}
 */
export class SyncedTabsController {
  /**
   * @type {boolean}
   */
  contextMenu;
  currentSetupStateIndex = -1;
  currentSyncedTabs = [];
  devices = [];
  /**
   * The current error state as determined by `SyncedTabsErrorHandler`.
   *
   * @type {number}
   */
  errorState = null;
  /**
   * Component associated with this controller.
   *
   * @type {ReactiveControllerHost}
   */
  host;
  /**
   * @type {Function}
   */
  pairDeviceCallback;
  searchQuery = "";
  /**
   * @type {Function}
   */
  signupCallback;

  /**
   * Construct a new SyncedTabsController.
   *
   * @param {ReactiveControllerHost} host
   * @param {object} options
   * @param {boolean} [options.contextMenu]
   *   Whether synced tab items have a secondary context menu.
   * @param {Function} [options.pairDeviceCallback]
   *   The function to call when the pair device window is opened.
   * @param {Function} [options.signupCallback]
   *   The function to call when the signup window is opened.
   */
  constructor(host, { contextMenu, pairDeviceCallback, signupCallback } = {}) {
    this.contextMenu = contextMenu;
    this.pairDeviceCallback = pairDeviceCallback;
    this.signupCallback = signupCallback;
    this.observe = this.observe.bind(this);
    this.host = host;
    this.host.addController(this);
  }

  hostConnected() {
    this.host.addEventListener("click", this);
  }

  hostDisconnected() {
    this.host.removeEventListener("click", this);
  }

  addSyncObservers() {
    Services.obs.addObserver(this.observe, SYNCED_TABS_CHANGED);
    Services.obs.addObserver(this.observe, TOPIC_SETUPSTATE_CHANGED);
  }

  removeSyncObservers() {
    Services.obs.removeObserver(this.observe, SYNCED_TABS_CHANGED);
    Services.obs.removeObserver(this.observe, TOPIC_SETUPSTATE_CHANGED);
  }

  handleEvent(event) {
    if (event.type == "click" && event.target.dataset.action) {
      const { ErrorType } = SyncedTabsErrorHandler;
      switch (event.target.dataset.action) {
        case `${ErrorType.SYNC_ERROR}`:
        case `${ErrorType.NETWORK_OFFLINE}`:
        case `${ErrorType.PASSWORD_LOCKED}`: {
          TabsSetupFlowManager.tryToClearError();
          break;
        }
        case `${ErrorType.SIGNED_OUT}`:
        case "sign-in": {
          TabsSetupFlowManager.openFxASignup(event.target.ownerGlobal);
          this.signupCallback?.();
          break;
        }
        case "add-device": {
          TabsSetupFlowManager.openFxAPairDevice(event.target.ownerGlobal);
          this.pairDeviceCallback?.();
          break;
        }
        case "sync-tabs-disabled": {
          TabsSetupFlowManager.syncOpenTabs(event.target);
          break;
        }
        case `${ErrorType.SYNC_DISCONNECTED}`: {
          const win = event.target.ownerGlobal;
          const { switchToTabHavingURI } =
            win.docShell.chromeEventHandler.ownerGlobal;
          switchToTabHavingURI(
            "about:preferences?action=choose-what-to-sync#sync",
            true,
            {}
          );
          break;
        }
      }
    }
  }

  async observe(_, topic, errorState) {
    if (topic == TOPIC_SETUPSTATE_CHANGED) {
      await this.updateStates(errorState);
    }
    if (topic == SYNCED_TABS_CHANGED) {
      await this.getSyncedTabData();
    }
  }

  async updateStates(errorState) {
    let stateIndex = TabsSetupFlowManager.uiStateIndex;
    errorState = errorState || SyncedTabsErrorHandler.getErrorType();

    if (stateIndex == 4 && this.currentSetupStateIndex !== stateIndex) {
      // trigger an initial request for the synced tabs list
      await this.getSyncedTabData();
    }

    this.currentSetupStateIndex = stateIndex;
    this.errorState = errorState;
    this.host.requestUpdate();
  }

  actionMappings = {
    "sign-in": {
      header: "firefoxview-syncedtabs-signin-header",
      description: "firefoxview-syncedtabs-signin-description",
      buttonLabel: "firefoxview-syncedtabs-signin-primarybutton",
    },
    "add-device": {
      header: "firefoxview-syncedtabs-adddevice-header",
      description: "firefoxview-syncedtabs-adddevice-description",
      buttonLabel: "firefoxview-syncedtabs-adddevice-primarybutton",
      descriptionLink: {
        name: "url",
        url: "https://support.mozilla.org/kb/how-do-i-set-sync-my-computer#w_connect-additional-devices-to-sync",
      },
    },
    "sync-tabs-disabled": {
      header: "firefoxview-syncedtabs-synctabs-header",
      description: "firefoxview-syncedtabs-synctabs-description",
      buttonLabel: "firefoxview-tabpickup-synctabs-primarybutton",
    },
    loading: {
      header: "firefoxview-syncedtabs-loading-header",
      description: "firefoxview-syncedtabs-loading-description",
    },
  };

  #getMessageCardForState({ error = false, action, errorState }) {
    errorState = errorState || this.errorState;
    let header,
      description,
      descriptionLink,
      buttonLabel,
      headerIconUrl,
      mainImageUrl;
    let descriptionArray;
    if (error) {
      let link;
      ({ header, description, link, buttonLabel } =
        SyncedTabsErrorHandler.getFluentStringsForErrorType(errorState));
      action = `${errorState}`;
      headerIconUrl = "chrome://global/skin/icons/info-filled.svg";
      mainImageUrl =
        "chrome://browser/content/firefoxview/synced-tabs-error.svg";
      descriptionArray = [description];
      if (errorState == "password-locked") {
        descriptionLink = {};
        // This is ugly, but we need to special case this link so we can
        // coexist with the old view.
        descriptionArray.push("firefoxview-syncedtab-password-locked-link");
        descriptionLink.name = "syncedtab-password-locked-link";
        descriptionLink.url = link.href;
      }
    } else {
      header = this.actionMappings[action].header;
      description = this.actionMappings[action].description;
      buttonLabel = this.actionMappings[action].buttonLabel;
      descriptionLink = this.actionMappings[action].descriptionLink;
      mainImageUrl =
        "chrome://browser/content/firefoxview/synced-tabs-error.svg";
      descriptionArray = [description];
    }
    return {
      action,
      buttonLabel,
      descriptionArray,
      descriptionLink,
      error,
      header,
      headerIconUrl,
      mainImageUrl,
    };
  }

  getRenderInfo() {
    let renderInfo = {};
    for (let tab of this.currentSyncedTabs) {
      if (!(tab.client in renderInfo)) {
        renderInfo[tab.client] = {
          name: tab.device,
          deviceType: tab.deviceType,
          tabs: [],
        };
      }
      renderInfo[tab.client].tabs.push(tab);
    }

    // Add devices without tabs
    for (let device of this.devices) {
      if (!(device.id in renderInfo)) {
        renderInfo[device.id] = {
          name: device.name,
          deviceType: device.clientType,
          tabs: [],
        };
      }
    }

    for (let id in renderInfo) {
      renderInfo[id].tabItems = this.searchQuery
        ? searchTabList(this.searchQuery, this.getTabItems(renderInfo[id].tabs))
        : this.getTabItems(renderInfo[id].tabs);
    }
    return renderInfo;
  }

  getMessageCard() {
    switch (this.currentSetupStateIndex) {
      case 0 /* error-state */:
        if (this.errorState) {
          return this.#getMessageCardForState({ error: true });
        }
        return this.#getMessageCardForState({ action: "loading" });
      case 1 /* not-signed-in */:
        if (Services.prefs.prefHasUserValue("services.sync.lastversion")) {
          // If this pref is set, the user has signed out of sync.
          // This path is also taken if we are disconnected from sync. See bug 1784055
          return this.#getMessageCardForState({
            error: true,
            errorState: "signed-out",
          });
        }
        return this.#getMessageCardForState({ action: "sign-in" });
      case 2 /* connect-secondary-device*/:
        return this.#getMessageCardForState({ action: "add-device" });
      case 3 /* disabled-tab-sync */:
        return this.#getMessageCardForState({ action: "sync-tabs-disabled" });
      case 4 /* synced-tabs-loaded*/:
        // There seems to be an edge case where sync says everything worked
        // fine but we have no devices.
        if (!this.devices.length) {
          return this.#getMessageCardForState({ action: "add-device" });
        }
    }
    return null;
  }

  getTabItems(tabs) {
    return tabs?.map(tab => ({
      icon: tab.icon,
      title: tab.title,
      time: tab.lastUsed * 1000,
      url: tab.url,
      primaryL10nId: "firefoxview-tabs-list-tab-button",
      primaryL10nArgs: JSON.stringify({ targetURI: tab.url }),
      secondaryL10nId: this.contextMenu
        ? "fxviewtabrow-options-menu-button"
        : undefined,
      secondaryL10nArgs: this.contextMenu
        ? JSON.stringify({ tabTitle: tab.title })
        : undefined,
    }));
  }

  updateTabsList(syncedTabs) {
    if (!syncedTabs.length) {
      this.currentSyncedTabs = syncedTabs;
    }

    const tabsToRender = syncedTabs;

    // Return early if new tabs are the same as previous ones
    if (lazy.ObjectUtils.deepEqual(tabsToRender, this.currentSyncedTabs)) {
      return;
    }

    this.currentSyncedTabs = tabsToRender;
    this.host.requestUpdate();
  }

  async getSyncedTabData() {
    this.devices = await lazy.SyncedTabs.getTabClients();
    let tabs = await lazy.SyncedTabs.createRecentTabsList(this.devices, 50, {
      removeAllDupes: false,
      removeDeviceDupes: true,
    });

    this.updateTabsList(tabs);
  }
}
