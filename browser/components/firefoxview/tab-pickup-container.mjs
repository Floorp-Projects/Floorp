/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { onToggleContainer } from "./helpers.mjs";

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";
const UI_OPEN_STATE = "browser.tabs.firefox-view.ui-state.tab-pickup.open";

class TabPickupContainer extends HTMLDetailsElement {
  constructor() {
    super();
    this.boundObserve = (...args) => this.observe(...args);
    this._currentSetupStateIndex = -1;
    this.errorState = null;
  }
  get setupContainerElem() {
    return this.querySelector(".sync-setup-container");
  }

  get tabsContainerElem() {
    return this.querySelector(".synced-tabs-container");
  }

  getWindow() {
    return this.ownerGlobal.browsingContext.embedderWindowGlobal.browsingContext
      .window;
  }

  connectedCallback() {
    this.addEventListener("click", this);
    this.addEventListener("toggle", this);
    this.addEventListener("visibilitychange", this);
    Services.obs.addObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);
    this.update();
  }

  cleanup() {
    Services.obs.removeObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);
  }

  disconnectedCallback() {
    this.cleanup();
  }

  handleEvent(event) {
    if (event.type == "toggle") {
      onToggleContainer(this);
      return;
    }
    if (event.type == "click" && event.target.dataset.action) {
      switch (event.target.dataset.action) {
        case "view0-sync-error-action":
        case "view0-network-offline-action": {
          TabsSetupFlowManager.forceSyncTabs();
          break;
        }
        case "view1-primary-action": {
          TabsSetupFlowManager.openFxASignup(event.target.ownerGlobal);
          break;
        }
        case "view2-primary-action":
        case "mobile-promo-primary-action": {
          TabsSetupFlowManager.openFxAPairDevice(event.target.ownerGlobal);
          break;
        }
        case "view3-primary-action": {
          TabsSetupFlowManager.syncOpenTabs(event.target);
          break;
        }
        case "mobile-promo-dismiss": {
          TabsSetupFlowManager.dismissMobilePromo(event.target);
          break;
        }
        case "mobile-confirmation-dismiss": {
          TabsSetupFlowManager.dismissMobileConfirmation(event.target);
          break;
        }
        case "view0-sync-disconnected-action": {
          const window = event.target.ownerGlobal;
          const {
            switchToTabHavingURI,
          } = window.docShell.chromeEventHandler.ownerGlobal;
          switchToTabHavingURI(
            "about:preferences?action=choose-what-to-sync#sync",
            true,
            {}
          );
          break;
        }
      }
    }
    // Returning to fxview seems like a likely time for a device check
    if (
      event.type == "visibilitychange" &&
      document.visibilityState === "visible"
    ) {
      this.update();
    }
  }

  async observe(subject, topic, errorState) {
    if (topic == TOPIC_SETUPSTATE_CHANGED) {
      this.update({ errorState });
    }
  }

  get mobilePromoElem() {
    return this.querySelector(".promo-box");
  }
  get mobileSuccessElem() {
    return this.querySelector(".confirmation-message-box");
  }

  insertTemplatedElement(templateId, elementId, replaceNode) {
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
          Services.urlFormatter.formatURLPref("app.support.baseURL") +
          elem.dataset.supportUrl;
      }
    }
    if (replaceNode) {
      if (typeof replaceNode == "string") {
        replaceNode = document.getElementById(replaceNode);
      }
      this.replaceChild(cloned, replaceNode);
    } else {
      this.appendChild(cloned);
    }
  }

  update({
    stateIndex = TabsSetupFlowManager.uiStateIndex,
    showMobilePromo = TabsSetupFlowManager.shouldShowMobilePromo,
    showMobilePairSuccess = TabsSetupFlowManager.shouldShowMobileConnectedSuccess,
    errorState = TabsSetupFlowManager.getErrorType(),
  } = {}) {
    let needsRender = false;
    if (showMobilePromo !== this._showMobilePromo) {
      this._showMobilePromo = showMobilePromo;
      needsRender = true;
    }
    if (showMobilePairSuccess !== this._showMobilePairSuccess) {
      this._showMobilePairSuccess = showMobilePairSuccess;
      needsRender = true;
    }
    if (stateIndex !== this._currentSetupStateIndex || stateIndex == 0) {
      this._currentSetupStateIndex = stateIndex;
      needsRender = true;
      this.errorState = errorState;
    }
    needsRender && this.render();
  }

  generateErrorMessage() {
    // We map the error state strings to Fluent string IDs so that it's easier
    // to change strings in the future without having to update all of the
    // error state strings.
    const errorStateStringMappings = {
      "sync-error": {
        header: "firefoxview-tabpickup-sync-error-header",
        description: "firefoxview-tabpickup-generic-sync-error-description",
        buttonLabel: "firefoxview-tabpickup-sync-error-primarybutton",
      },

      "fxa-admin-disabled": {
        header: "firefoxview-tabpickup-fxa-admin-disabled-header",
        description: "firefoxview-tabpickup-fxa-admin-disabled-description",
        // The button is hidden for this errorState, so we don't include the
        // buttonLabel property.
      },

      "network-offline": {
        header: "firefoxview-tabpickup-network-offline-header",
        description: "firefoxview-tabpickup-network-offline-description",
        buttonLabel: "firefoxview-tabpickup-network-offline-primarybutton",
      },

      "sync-disconnected": {
        header: "firefoxview-tabpickup-sync-disconnected-header",
        description: "firefoxview-tabpickup-sync-disconnected-description",
        buttonLabel: "firefoxview-tabpickup-sync-disconnected-primarybutton",
      },
    };

    const errorStateHeader = this.querySelector(
      "#tabpickup-steps-view0-header"
    );
    const errorStateDescription = this.querySelector(
      "#error-state-description"
    );
    const errorStateButton = this.querySelector("#error-state-button");

    document.l10n.setAttributes(
      errorStateHeader,
      errorStateStringMappings[this.errorState].header
    );
    document.l10n.setAttributes(
      errorStateDescription,
      errorStateStringMappings[this.errorState].description
    );

    errorStateButton.hidden = this.errorState == "fxa-admin-disabled";

    if (this.errorState != "fxa-admin-disabled") {
      document.l10n.setAttributes(
        errorStateButton,
        errorStateStringMappings[this.errorState].buttonLabel
      );
      errorStateButton.setAttribute(
        "data-action",
        `view0-${this.errorState}-action`
      );
    }
  }

  render() {
    if (!this.isConnected) {
      return;
    }

    let setupElem = this.setupContainerElem;
    let tabsElem = this.tabsContainerElem;
    let mobilePromoElem = this.mobilePromoElem;
    let mobileSuccessElem = this.mobileSuccessElem;

    const stateIndex = this._currentSetupStateIndex;
    const isLoading = stateIndex == 4;

    mobilePromoElem.hidden = !this._showMobilePromo;
    mobileSuccessElem.hidden = !this._showMobilePairSuccess;

    this.open =
      !TabsSetupFlowManager.isTabSyncSetupComplete ||
      Services.prefs.getBoolPref(UI_OPEN_STATE, true);

    // show/hide either the setup or tab list containers, creating each as necessary
    if (stateIndex < 4) {
      if (!setupElem) {
        this.insertTemplatedElement(
          "sync-setup-template",
          "tabpickup-steps",
          "sync-setup-placeholder"
        );
        setupElem = this.setupContainerElem;
      }
      if (tabsElem) {
        tabsElem.hidden = true;
      }
      setupElem.hidden = false;
      setupElem.selectedViewName = `sync-setup-view${stateIndex}`;

      if (stateIndex == 0 && this.errorState) {
        this.generateErrorMessage();
      }
      return;
    }

    if (!tabsElem) {
      this.insertTemplatedElement(
        "synced-tabs-template",
        "tabpickup-tabs-container",
        "synced-tabs-placeholder"
      );
      tabsElem = this.tabsContainerElem;
    }
    if (setupElem) {
      setupElem.hidden = true;
    }
    tabsElem.hidden = false;
    tabsElem.classList.toggle("loading", isLoading);
  }
}
customElements.define("tab-pickup-container", TabPickupContainer, {
  extends: "details",
});

export { TabPickupContainer };
