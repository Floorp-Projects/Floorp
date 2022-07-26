/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { toggleContainer } from "./helpers.mjs";

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);
const TOPIC_SETUPSTATE_CHANGED = "firefox-view.setupstate.changed";

class TabPickupContainer extends HTMLElement {
  constructor() {
    super();
    this.boundObserve = (...args) => this.observe(...args);
    this._currentSetupStateIndex = -1;
  }
  get setupContainerElem() {
    return this.querySelector(".sync-setup-container");
  }

  get tabsContainerElem() {
    return this.querySelector(".synced-tabs-container");
  }

  get collapsibleButton() {
    return this.querySelector("#collapsible-synced-tabs-button");
  }

  connectedCallback() {
    this.addEventListener("click", this);
    Services.obs.addObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);
    this.updateSetupState(TabsSetupFlowManager.uiStateIndex);
  }

  cleanup() {
    Services.obs.removeObserver(this.boundObserve, TOPIC_SETUPSTATE_CHANGED);
  }

  disconnectedCallback() {
    this.cleanup();
  }

  handleEvent(event) {
    if (event.type == "click" && event.target == this.collapsibleButton) {
      toggleContainer(this.collapsibleButton, this.tabsContainerElem);
      return;
    }
    if (event.type == "click" && event.target.dataset.action) {
      switch (event.target.dataset.action) {
        case "view0-primary-action": {
          TabsSetupFlowManager.openFxASignup(event.target.ownerGlobal);
          break;
        }
        case "view1-primary-action": {
          TabsSetupFlowManager.openSyncPreferences(event.target.ownerGlobal);
          break;
        }
        case "view2-primary-action": {
          TabsSetupFlowManager.syncOpenTabs(event.target);
          break;
        }
      }
    }
  }

  async observe(subject, topic, stateIndex) {
    if (topic == TOPIC_SETUPSTATE_CHANGED) {
      this.updateSetupState(TabsSetupFlowManager.uiStateIndex);
    }
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
          Services.urlFormatter.formatURLPref("app.support.baseURL") +
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
    const isLoading = stateIndex == 3;

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
    }
    if (setupElem) {
      setupElem.hidden = true;
    }
    tabsElem.hidden = false;
    tabsElem.classList.toggle("loading", isLoading);

    if (stateIndex == 4) {
      this.collapsibleButton.hidden = false;
    }
  }
}
customElements.define("tab-pickup-container", TabPickupContainer);

export { TabPickupContainer };
