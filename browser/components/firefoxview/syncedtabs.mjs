/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
});

const { SyncedTabsErrorHandler } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-synced-tabs-error-handler.sys.mjs"
);
const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";

const SYNCED_TABS_CHANGED = "services.sync.tabs.changed";
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";
const UI_OPEN_STATE = "browser.tabs.firefox-view.ui-state.tab-pickup.open";

class SyncedTabsInView extends ViewPage {
  constructor() {
    super();
    this.boundObserve = (...args) => this.observe(...args);
    this._currentSetupStateIndex = -1;
    this.errorState = null;
    this._id = Math.floor(Math.random() * 10e6);
    this.currentSyncedTabs = [];
    if (this.overview) {
      this.maxTabsLength = 5;
    } else {
      // Setting maxTabsLength to -1 for no max
      this.maxTabsLength = -1;
    }
    this.devices = [];
  }

  static properties = {
    ...ViewPage.properties,
    errorState: { type: Number },
    currentSyncedTabs: { type: Array },
    _currentSetupStateIndex: { type: Number },
    devices: { type: Array },
  };

  connectedCallback() {
    super.connectedCallback();
    this.addEventListener("click", this);
    this.ownerDocument.addEventListener("visibilitychange", this);
    Services.obs.addObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);
    Services.obs.addObserver(this.boundObserve, SYNCED_TABS_CHANGED);

    this.updateStates();
    this.onVisibilityChange();
  }

  cleanup() {
    TabsSetupFlowManager.updateViewVisibility(this._id, "unloaded");
    this.ownerDocument?.removeEventListener("visibilitychange", this);
    Services.obs.removeObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);
    Services.obs.removeObserver(this.boundObserve, SYNCED_TABS_CHANGED);
  }

  disconnectedCallback() {
    this.cleanup();
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
          break;
        }
        case "add-device": {
          TabsSetupFlowManager.openFxAPairDevice(event.target.ownerGlobal);
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
    if (event.type == "change") {
      TabsSetupFlowManager.syncOpenTabs(event.target);
    }

    // Returning to fxview seems like a likely time for a device check
    if (event.type == "visibilitychange") {
      this.onVisibilityChange();
    }
  }
  onVisibilityChange() {
    const isVisible = document.visibilityState == "visible";
    const isOpen = this.open;
    if (isVisible && isOpen) {
      this.update();
      TabsSetupFlowManager.updateViewVisibility(this._id, "visible");
    } else {
      TabsSetupFlowManager.updateViewVisibility(
        this._id,
        isVisible ? "closed" : "hidden"
      );
    }
  }

  async observe(subject, topic, errorState) {
    if (topic == TOPIC_SETUPSTATE_CHANGED) {
      this.updateStates({ errorState });
    }
    if (topic == SYNCED_TABS_CHANGED) {
      this.getSyncedTabData();
    }
  }

  updateStates({
    stateIndex = TabsSetupFlowManager.uiStateIndex,
    errorState = SyncedTabsErrorHandler.getErrorType(),
  } = {}) {
    if (stateIndex == 4 && this._currentSetupStateIndex !== stateIndex) {
      // trigger an initial request for the synced tabs list
      this.getSyncedTabData();
    }

    this._currentSetupStateIndex = stateIndex;
    this.errorState = errorState;
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
      checkboxLabel: "firefoxview-syncedtabs-synctabs-checkbox",
    },
  };

  generateMessageCard({ error = false, action, errorState }) {
    errorState = errorState || this.errorState;
    let header,
      description,
      descriptionLink,
      buttonLabel,
      checkboxLabel,
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
      checkboxLabel = this.actionMappings[action].checkboxLabel;
      descriptionLink = this.actionMappings[action].descriptionLink;
      mainImageUrl =
        "chrome://browser/content/firefoxview/synced-tabs-error.svg";
      descriptionArray = [description];
    }

    return html`
      <fxview-empty-state
        headerLabel=${header}
        .descriptionLabels=${descriptionArray}
        .descriptionLink=${ifDefined(descriptionLink)}
        class="empty-state synced-tabs"
        ?isSelectedTab=${this.selectedTab}
        mainImageUrl="${ifDefined(mainImageUrl)}"
        headerIconUrl="${ifDefined(headerIconUrl)}"
      >
        <button
          class="primary"
          slot="primary-action"
          ?hidden=${!buttonLabel}
          data-l10n-id="${ifDefined(buttonLabel)}"
          data-action="${action}"
          @click=${this.handleEvent}
        ></button>
        <div slot="primary-action"
          ?hidden=${!checkboxLabel} >
          <label>
            <input type="checkbox" @change=${this.handleEvent}></input>
            <span data-l10n-id="${ifDefined(checkboxLabel)}"></span>
          </label>
        </div>
      </fxview-empty-state>
    `;
  }

  onOpenLink(event) {
    let currentWindow = this.getWindow();
    if (currentWindow.openTrustedLinkIn) {
      let where = lazy.BrowserUtils.whereToOpenLink(
        event.detail.originalEvent,
        false,
        true
      );
      if (where == "current") {
        where = "tab";
      }
      currentWindow.openTrustedLinkIn(event.originalTarget.url, where);
    }
  }

  onContextMenu(e) {
    this.triggerNode = e.originalTarget;
    e.target.querySelector("panel-list").toggle(e.detail.originalEvent);
  }

  panelListTemplate() {
    return html`
      <panel-list slot="menu">
        <panel-item
          @click=${this.openInNewWindow}
          data-l10n-id="fxviewtabrow-open-in-window"
          data-l10n-attrs="accesskey"
        ></panel-item>
        <panel-item
          @click=${this.openInNewPrivateWindow}
          data-l10n-id="fxviewtabrow-open-in-private-window"
          data-l10n-attrs="accesskey"
        ></panel-item>
        <hr />
        <panel-item
          @click=${this.copyLink}
          data-l10n-id="fxviewtabrow-copy-link"
          data-l10n-attrs="accesskey"
        ></panel-item>
      </panel-list>
    `;
  }

  noDeviceTabsTemplate(deviceName, deviceType) {
    return html`<card-container>
      <h2 slot="header">
        <div class="icon ${deviceType}" role="presentation"></div>
        ${deviceName}
      </h2>
      <div slot="main" class="blackbox notabs">No tabs open on this device</div>
    </card-container>`;
  }

  generateTabList() {
    let renderArray = [];
    let renderInfo = {};
    for (let tab of this.currentSyncedTabs) {
      if (!(tab.device in renderInfo)) {
        renderInfo[tab.device] = {
          deviceType: tab.deviceType,
          tabs: [],
        };
      }
      renderInfo[tab.device].tabs.push(tab);
    }
    // Add devices without tabs
    for (let device of this.devices) {
      if (!(device.name in renderInfo)) {
        renderInfo[device.name] = {
          deviceType: device.type,
          tabs: [],
        };
      }
    }
    for (let deviceName in renderInfo) {
      if (renderInfo[deviceName].tabs.length) {
        renderArray.push(html`<card-container>
          <h2 slot="header">
            <div
              class="icon ${renderInfo[deviceName].deviceType}"
              role="presentation"
            ></div>
            ${deviceName}
          </h2>
          <fxview-tab-list
            slot="main"
            class="syncedtabs"
            hasPopup="menu"
            .tabItems=${ifDefined(
              this.getTabItems(renderInfo[deviceName].tabs)
            )}
            maxTabsLength=${this.maxTabsLength}
            @fxview-tab-list-primary-action=${this.onOpenLink}
            @fxview-tab-list-secondary-action=${this.onContextMenu}
          >
            ${this.panelListTemplate()}
          </fxview-tab-list>
        </card-container>`);
      } else {
        renderArray.push(
          this.noDeviceTabsTemplate(
            deviceName,
            renderInfo[deviceName].deviceType
          )
        );
      }
    }
    return renderArray;
  }
  render() {
    const stateIndex = this._currentSetupStateIndex;

    this.open =
      !TabsSetupFlowManager.isTabSyncSetupComplete ||
      Services.prefs.getBoolPref(UI_OPEN_STATE, true);

    let renderArray = [];
    renderArray.push(html` <link
      rel="stylesheet"
      href="chrome://browser/content/firefoxview/view-syncedtabs.css"
    />`);
    renderArray.push(html` <link
      rel="stylesheet"
      href="chrome://browser/content/firefoxview/firefoxview-next.css"
    />`);
    if (!this.overview) {
      renderArray.push(html`<div class="sticky-container bottom-fade">
        <h2
          class="page-header"
          data-l10n-id="firefoxview-synced-tabs-header"
        ></h2>
      </div>`);
    }

    switch (stateIndex) {
      case 0 /* error-state */:
        if (this.errorState) {
          renderArray.push(this.generateMessageCard({ error: true }));
        }
        break;
      case 1 /* not-signed-in */:
        if (Services.prefs.prefHasUserValue("services.sync.lastversion")) {
          // If this pref is set, the user has signed out of sync.
          // This path is also taken if we are disconnected from sync. See bug 1784055
          renderArray.push(
            this.generateMessageCard({ error: true, errorState: "signed-out" })
          );
        } else {
          renderArray.push(this.generateMessageCard({ action: "sign-in" }));
        }
        break;
      case 2 /* connect-secondary-device*/:
        renderArray.push(this.generateMessageCard({ action: "add-device" }));
        break;
      case 3 /* disabled-tab-sync */:
        renderArray.push(
          this.generateMessageCard({ action: "sync-tabs-disabled" })
        );
        break;
      case 4 /* synced-tabs-loaded*/:
        renderArray = renderArray.concat(this.generateTabList());
        break;
    }
    return renderArray;
  }

  async onReload() {
    await TabsSetupFlowManager.syncOnPageReload();
  }

  getTabItems(tabs) {
    tabs = tabs || this.tabs;
    return tabs?.map(tab => ({
      icon: tab.icon,
      title: tab.title,
      time: tab.lastUsed * 1000,
      url: tab.url,
      primaryL10nId: "firefoxview-tabs-list-tab-button",
      primaryL10nArgs: JSON.stringify({ targetURI: tab.url }),
      secondaryL10nId: "firefoxview-close-button",
    }));
  }

  updateTabsList(syncedTabs) {
    if (!syncedTabs.length) {
      this.currentSyncedTabs = syncedTabs;
      this.sendTabTelemetry(0);
    }

    const tabsToRender = syncedTabs;

    // Return early if new tabs are the same as previous ones
    if (
      JSON.stringify(tabsToRender) == JSON.stringify(this.currentSyncedTabs)
    ) {
      return;
    }

    this.currentSyncedTabs = tabsToRender;
    // Record the full tab count
    this.sendTabTelemetry(syncedTabs.length);
  }

  async getSyncedTabData() {
    this.devices = await lazy.SyncedTabs.getTabClients();
    let tabs = await lazy.SyncedTabs.getRecentTabs(50, {
      removeAllDupes: false,
      removeDeviceDupes: true,
    });

    this.updateTabsList(tabs);
  }

  sendTabTelemetry(numTabs) {
    /*
    Services.telemetry.recordEvent(
      "firefoxview-next",
      "synced_tabs",
      "tabs",
      null,
      {
        count: numTabs.toString(),
      }
    );
*/
  }
}
customElements.define("view-syncedtabs", SyncedTabsInView);
