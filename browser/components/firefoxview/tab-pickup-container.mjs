/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { onToggleContainer } from "./helpers.mjs";

const { SyncedTabsErrorHandler } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-synced-tabs-error-handler.sys.mjs"
);
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
    this.tabListAdded = null;
    this._id = Math.floor(Math.random() * 10e6);
  }
  get setupContainerElem() {
    return this.querySelector(".sync-setup-container");
  }

  get tabsContainerElem() {
    return this.querySelector(".synced-tabs-container");
  }

  get tabPickupListElem() {
    return this.querySelector(".synced-tabs-container tab-pickup-list");
  }

  getWindow() {
    return this.ownerGlobal.browsingContext.embedderWindowGlobal.browsingContext
      .window;
  }

  connectedCallback() {
    this.addEventListener("click", this);
    this.addEventListener("toggle", this);
    this.ownerDocument.addEventListener("visibilitychange", this);
    Services.obs.addObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);

    for (let elem of this.querySelectorAll("a[data-support-url]")) {
      elem.href =
        Services.urlFormatter.formatURLPref("app.support.baseURL") +
        elem.dataset.supportUrl;
    }

    // we wait until the list shows up before trying to populate it,
    // when its safe to assume the custom-element's methods will be available
    this.tabListAdded = this.promiseChildAdded();
    this.update();
    this.onVisibilityChange();
  }

  promiseChildAdded() {
    return new Promise(resolve => {
      if (typeof this.tabPickupListElem?.getSyncedTabData == "function") {
        resolve();
        return;
      }
      this.addEventListener(
        "list-ready",
        event => {
          resolve();
        },
        { once: true }
      );
    });
  }

  cleanup() {
    TabsSetupFlowManager.updateViewVisibility(this._id, "unloaded");
    this.ownerDocument?.removeEventListener("visibilitychange", this);
    Services.obs.removeObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);
  }

  disconnectedCallback() {
    this.cleanup();
  }

  handleEvent(event) {
    if (event.type == "toggle") {
      onToggleContainer(this);
      this.onVisibilityChange();
      return;
    }
    if (event.type == "click" && event.target.dataset.action) {
      const { ErrorType } = SyncedTabsErrorHandler;
      switch (event.target.dataset.action) {
        case `view0-${ErrorType.SYNC_ERROR}-action`:
        case `view0-${ErrorType.NETWORK_OFFLINE}-action`:
        case `view0-${ErrorType.PASSWORD_LOCKED}-action`: {
          TabsSetupFlowManager.tryToClearError();
          break;
        }
        case `view0-${ErrorType.SIGNED_OUT}-action`:
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
        case `view0-${ErrorType.SYNC_DISCONNECTED}-action`: {
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
      this.update({ errorState });
    }
  }

  get mobilePromoElem() {
    return this.querySelector(".promo-box");
  }
  get mobileSuccessElem() {
    return this.querySelector(".confirmation-message-box");
  }

  update({
    stateIndex = TabsSetupFlowManager.uiStateIndex,
    showMobilePromo = TabsSetupFlowManager.shouldShowMobilePromo,
    showMobilePairSuccess = TabsSetupFlowManager.shouldShowMobileConnectedSuccess,
    errorState = SyncedTabsErrorHandler.getErrorType(),
    waitingForTabs = TabsSetupFlowManager.waitingForTabs,
  } = {}) {
    let needsRender = false;
    if (waitingForTabs !== this._waitingForTabs) {
      this._waitingForTabs = waitingForTabs;
      needsRender = true;
    }

    if (showMobilePromo !== this._showMobilePromo) {
      this._showMobilePromo = showMobilePromo;
      needsRender = true;
    }
    if (showMobilePairSuccess !== this._showMobilePairSuccess) {
      this._showMobilePairSuccess = showMobilePairSuccess;
      needsRender = true;
    }
    if (stateIndex == 4 && this._currentSetupStateIndex !== stateIndex) {
      // trigger an initial request for the synced tabs list
      this.tabListAdded.then(() => {
        this.tabPickupListElem.getSyncedTabData();
      });
    }
    if (stateIndex !== this._currentSetupStateIndex || stateIndex == 0) {
      this._currentSetupStateIndex = stateIndex;
      needsRender = true;
      this.errorState = errorState;
    }
    needsRender && this.render();
  }

  generateErrorMessage() {
    const errorStateHeader = this.querySelector(
      "#tabpickup-steps-view0-header"
    );
    const errorStateDescription = this.querySelector(
      "#error-state-description"
    );
    const errorStateButton = this.querySelector("#error-state-button");
    const errorStateLink = this.querySelector("#error-state-link");
    const errorStateProperties =
      SyncedTabsErrorHandler.getFluentStringsForErrorType(this.errorState);

    document.l10n.setAttributes(errorStateHeader, errorStateProperties.header);
    document.l10n.setAttributes(
      errorStateDescription,
      errorStateProperties.description
    );

    errorStateButton.hidden = this.errorState == "fxa-admin-disabled";

    if (this.errorState != "fxa-admin-disabled") {
      document.l10n.setAttributes(
        errorStateButton,
        errorStateProperties.buttonLabel
      );
      errorStateButton.setAttribute(
        "data-action",
        `view0-${this.errorState}-action`
      );
    }

    if (errorStateProperties.link) {
      document.l10n.setAttributes(
        errorStateLink,
        errorStateProperties.link.label
      );
      errorStateLink.href = errorStateProperties.link.href;
      errorStateLink.hidden = false;
    } else {
      errorStateLink.hidden = true;
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
    const isLoading = this._waitingForTabs;

    mobilePromoElem.hidden = !this._showMobilePromo;
    mobileSuccessElem.hidden = !this._showMobilePairSuccess;

    this.open =
      !TabsSetupFlowManager.isTabSyncSetupComplete ||
      Services.prefs.getBoolPref(UI_OPEN_STATE, true);

    // show/hide either the setup or tab list containers, creating each as necessary
    if (stateIndex < 4) {
      tabsElem.hidden = true;
      setupElem.hidden = false;
      setupElem.selectedViewName = `sync-setup-view${stateIndex}`;

      if (stateIndex == 0 && this.errorState) {
        this.generateErrorMessage();
      }
      return;
    }

    setupElem.hidden = true;
    tabsElem.hidden = false;
    tabsElem.classList.toggle("loading", isLoading);
  }

  async onReload() {
    await TabsSetupFlowManager.syncOnPageReload();
  }
}
customElements.define("tab-pickup-container", TabPickupContainer, {
  extends: "details",
});

export { TabPickupContainer };
