/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const tabsSetupFlowManager = new (class {
  initialize(elem) {
    this.elem = elem;
    // placeholder initial value
    // Bug 1763138 - Get the actual initial state for the setup flow
    this.setupState = 0;

    this.elem.addEventListener("click", this);
    this.elem.updateSetupState(this.setupState);
  }
  handleEvent(event) {
    if (event.type == "click" && event.target.dataset.action) {
      switch (event.target.dataset.action) {
        case "view0-primary-action": {
          this.advanceToAddDeviceStep(event.target);
          break;
        }
        case "view1-primary-action": {
          this.advanceToSyncTabsStep(event.target);
          break;
        }
        case "view2-primary-action": {
          this.advanceToSetupComplete(event.target);
          break;
        }
        case "view3-primary-action": {
          this.advanceToGetMyTabs(event.target);
          break;
        }
      }
    }
  }
  advanceToAddDeviceStep(containerElem) {
    // Bug 1763138 - Implement the actual logic to advance to next step
    this.setupState = 1;
    this.elem.updateSetupState(this.setupState);
  }
  advanceToSyncTabsStep(containerElem) {
    // Bug 1763138 - Implement the actual logic to advance to next step
    this.setupState = 2;
    this.elem.updateSetupState(this.setupState);
  }
  advanceToSetupComplete(containerElem) {
    // Bug 1763138 - Implement the actual logic to advance to next step
    this.setupState = 3;
    this.elem.updateSetupState(this.setupState);
  }
  advanceToGetMyTabs(containerElem) {
    // Bug 1763138 - Implement the actual logic to advance to next step
    this.setupState = 4;
    this.elem.updateSetupState(this.setupState);
  }
})();

export class TabsPickupContainer extends HTMLElement {
  constructor() {
    super();
    this.manager = null;
    this._currentSetupState = -1;
  }
  get setupContainerElem() {
    return this.querySelector("#tabpickup-setup-steps");
  }
  get tabsContainerElem() {
    return this.querySelector("#tabpickup-tabs-container");
  }
  appendTemplatedElement(templateId, elementId) {
    const template = document.getElementById(templateId);
    const templateContent = template.content;
    const cloned = templateContent.cloneNode(true);
    if (elementId) {
      cloned.firstElementChild.id = elementId;
    }
    this.appendChild(cloned);
  }
  updateSetupState(stateIndex) {
    if (stateIndex === undefined) {
      stateIndex = this._currentSetupState;
    }
    if (stateIndex == this._currentSetupState) {
      return;
    }
    this._currentSetupState = stateIndex;
    this.render();
  }
  render() {
    if (!this.isConnected) {
      return;
    }
    let setupElem = this.setupContainerElem;
    let tabsElem = this.tabsContainerElem;
    const stateIndex = this._currentSetupState;

    // show/hide either the setup or tab list containers, creating each as necessary
    if (stateIndex < 4) {
      if (!setupElem) {
        this.appendTemplatedElement(
          "sync-setup-template",
          "tabpickup-setup-steps"
        );
        setupElem = this.setupContainerElem;
      }
      if (tabsElem) {
        tabsElem.hidden = true;
      }
      setupElem.hidden = false;
      setupElem.selectedViewName = `sync-setup-view${stateIndex}`;
    } else {
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
    }
  }
}
customElements.define("tabs-pickup-container", TabsPickupContainer);

export { tabsSetupFlowManager };
