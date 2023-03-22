/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
});

import {
  formatURIForDisplay,
  convertTimestamp,
  getImageUrl,
  NOW_THRESHOLD_MS,
} from "./helpers.mjs";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const SYNCED_TABS_CHANGED = "services.sync.tabs.changed";

class TabPickupList extends HTMLElement {
  constructor() {
    super();
    this.maxTabsLength = 3;
    this.currentSyncedTabs = [];
    this.boundObserve = (...args) => {
      this.getSyncedTabData(...args);
    };

    // The recency timestamp update period is stored in a pref to allow tests to easily change it
    XPCOMUtils.defineLazyPreferenceGetter(
      lazy,
      "timeMsPref",
      "browser.tabs.firefox-view.updateTimeMs",
      NOW_THRESHOLD_MS,
      () => this.updateTime()
    );
  }

  get tabsList() {
    return this.querySelector("ol");
  }

  get fluentStrings() {
    if (!this._fluentStrings) {
      this._fluentStrings = new Localization(["browser/firefoxView.ftl"], true);
    }
    return this._fluentStrings;
  }

  get timeElements() {
    return this.querySelectorAll("span.synced-tab-li-time");
  }

  connectedCallback() {
    this.placeholderContainer = document.getElementById(
      "synced-tabs-placeholder"
    );
    this.tabPickupContainer = document.getElementById(
      "tabpickup-tabs-container"
    );

    this.addEventListener("click", this);
    Services.obs.addObserver(this.boundObserve, SYNCED_TABS_CHANGED);

    // inform ancestor elements our getSyncedTabData method is available to fetch data
    this.dispatchEvent(new CustomEvent("list-ready", { bubbles: true }));
  }

  handleEvent(event) {
    if (
      event.type == "click" ||
      (event.type == "keydown" && event.keyCode == KeyEvent.DOM_VK_RETURN)
    ) {
      const item = event.target.closest(".synced-tab-li");
      let index = [...this.tabsList.children].indexOf(item);
      let deviceType = item.dataset.deviceType;
      Services.telemetry.recordEvent(
        "firefoxview",
        "tab_pickup",
        "tabs",
        null,
        {
          position: (++index).toString(),
          deviceType,
        }
      );
    }
    if (event.type == "keydown") {
      switch (event.key) {
        case "ArrowRight": {
          event.preventDefault();
          this.moveFocusToSecondElement();
          break;
        }
        case "ArrowLeft": {
          event.preventDefault();
          this.moveFocusToFirstElement();
          break;
        }
        case "ArrowDown": {
          event.preventDefault();
          this.moveFocusToNextElement();
          break;
        }
        case "ArrowUp": {
          event.preventDefault();
          this.moveFocusToPreviousElement();
          break;
        }
        case "Tab": {
          this.resetFocus(event);
        }
      }
    }
  }

  /**
   * Handles removing and setting tabindex on elements
   * while moving focus to the next element
   *
   * @param {HTMLElement} currentElement currently focused element
   * @param {HTMLElement} nextElement element that should receive focus next
   * @memberof TabPickupList
   * @private
   */
  #manageTabIndexAndFocus(currentElement, nextElement) {
    currentElement.setAttribute("tabindex", "-1");
    nextElement.removeAttribute("tabindex");
    nextElement.focus();
  }

  moveFocusToFirstElement() {
    let selectableElements = Array.from(this.tabsList.querySelectorAll("a"));
    let firstElement = selectableElements[0];
    let selectedElement = this.tabsList.querySelector("a:not([tabindex]");
    this.#manageTabIndexAndFocus(selectedElement, firstElement);
  }

  moveFocusToSecondElement() {
    let selectableElements = Array.from(this.tabsList.querySelectorAll("a"));
    let secondElement = selectableElements[1];
    if (secondElement) {
      let selectedElement = this.tabsList.querySelector("a:not([tabindex]");
      this.#manageTabIndexAndFocus(selectedElement, secondElement);
    }
  }

  moveFocusToNextElement() {
    let selectableElements = Array.from(this.tabsList.querySelectorAll("a"));
    let selectedElement = this.tabsList.querySelector("a:not([tabindex]");
    let nextElement =
      selectableElements.findIndex(elem => elem == selectedElement) + 1;
    if (nextElement < selectableElements.length) {
      this.#manageTabIndexAndFocus(
        selectedElement,
        selectableElements[nextElement]
      );
    }
  }
  moveFocusToPreviousElement() {
    let selectableElements = Array.from(this.tabsList.querySelectorAll("a"));
    let selectedElement = this.tabsList.querySelector("a:not([tabindex]");
    let previousElement =
      selectableElements.findIndex(elem => elem == selectedElement) - 1;
    if (previousElement >= 0) {
      this.#manageTabIndexAndFocus(
        selectedElement,
        selectableElements[previousElement]
      );
    }
  }
  resetFocus(e) {
    let selectableElements = Array.from(this.tabsList.querySelectorAll("a"));
    let selectedElement = this.tabsList.querySelector("a:not([tabindex]");
    selectedElement.setAttribute("tabindex", "-1");
    selectableElements[0].removeAttribute("tabindex");
    if (e.shiftKey) {
      e.preventDefault();
      document
        .getElementById("tab-pickup-container")
        .querySelector("summary")
        .focus();
    }
  }

  cleanup() {
    Services.obs.removeObserver(this.boundObserve, SYNCED_TABS_CHANGED);
    clearInterval(this.intervalID);
  }

  updateTime() {
    // when pref is 0, avoid the update altogether (used for tests)
    if (!lazy.timeMsPref) {
      return;
    }
    for (let timeEl of this.timeElements) {
      timeEl.textContent = convertTimestamp(
        parseInt(timeEl.getAttribute("data-timestamp")),
        this.fluentStrings,
        lazy.timeMsPref
      );
    }
  }

  togglePlaceholderVisibility(visible) {
    this.placeholderContainer.toggleAttribute("hidden", !visible);
    this.placeholderContainer.classList.toggle("empty-container", visible);
  }

  async getSyncedTabData() {
    let tabs = await lazy.SyncedTabs.getRecentTabs(50);

    this.updateTabsList(tabs);
  }

  tabsEqual(a, b) {
    return JSON.stringify(a) == JSON.stringify(b);
  }

  updateTabsList(syncedTabs) {
    if (!syncedTabs.length) {
      while (this.tabsList.firstChild) {
        this.tabsList.firstChild.remove();
      }
      this.togglePlaceholderVisibility(true);
      this.tabsList.hidden = true;
      this.currentSyncedTabs = syncedTabs;
      this.sendTabTelemetry(0);
      return;
    }

    // Slice syncedTabs to maxTabsLength assuming maxTabsLength
    // doesn't change between renders
    const tabsToRender = syncedTabs.slice(0, this.maxTabsLength);

    // Pad the render list with placeholders
    for (let i = tabsToRender.length; i < this.maxTabsLength; i++) {
      tabsToRender.push({
        type: "placeholder",
      });
    }

    // Return early if new tabs are the same as previous ones
    if (
      JSON.stringify(tabsToRender) == JSON.stringify(this.currentSyncedTabs)
    ) {
      return;
    }

    for (let i = 0; i < tabsToRender.length; i++) {
      const tabData = tabsToRender[i];
      let li = this.tabsList.children[i];
      if (li) {
        if (this.tabsEqual(tabData, this.currentSyncedTabs[i])) {
          // Nothing to change
          continue;
        }
        if (tabData.type == "placeholder") {
          // Replace a tab item with a placeholder
          this.tabsList.replaceChild(this.generatePlaceholder(), li);
          continue;
        } else if (this.currentSyncedTabs[i]?.type == "placeholder") {
          // Replace the placeholder with a tab item
          const tabItem = this.generateListItem(i);
          this.tabsList.replaceChild(tabItem, li);
          li = tabItem;
        }
      } else if (tabData.type == "placeholder") {
        this.tabsList.appendChild(this.generatePlaceholder());
        continue;
      } else {
        li = this.tabsList.appendChild(this.generateListItem(i));
      }
      this.updateListItem(li, tabData);
    }

    this.currentSyncedTabs = tabsToRender;
    // Record the full tab count
    this.sendTabTelemetry(syncedTabs.length);

    if (this.tabsList.hidden) {
      this.tabsList.hidden = false;
      this.togglePlaceholderVisibility(false);

      if (!this.intervalID) {
        this.intervalID = setInterval(() => this.updateTime(), lazy.timeMsPref);
      }
    }
  }

  generatePlaceholder() {
    const li = document.createElement("li");
    li.classList.add("synced-tab-li-placeholder");
    li.setAttribute("role", "presentation");

    const favicon = document.createElement("span");
    favicon.classList.add("li-placeholder-favicon");

    const title = document.createElement("span");
    title.classList.add("li-placeholder-title");

    const domain = document.createElement("span");
    domain.classList.add("li-placeholder-domain");

    li.append(favicon, title, domain);
    return li;
  }

  /*
     Populate a list item with content from a tab object
  */
  updateListItem(li, tab) {
    const targetURI = tab.url;
    const lastUsedMs = tab.lastUsed * 1000;
    const deviceText = tab.device;

    li.dataset.deviceType = tab.deviceType;

    li.querySelector("a").href = targetURI;
    li.querySelector(".synced-tab-li-title").textContent = tab.title;

    const favicon = li.querySelector(".favicon");
    const imageUrl = getImageUrl(tab.icon, targetURI);
    favicon.style.backgroundImage = `url('${imageUrl}')`;

    const time = li.querySelector(".synced-tab-li-time");
    time.textContent = convertTimestamp(lastUsedMs, this.fluentStrings);
    time.setAttribute("data-timestamp", lastUsedMs);

    const deviceIcon = document.createElement("div");
    deviceIcon.classList.add("icon", tab.deviceType);
    deviceIcon.setAttribute("role", "presentation");

    const device = li.querySelector(".synced-tab-li-device");
    device.textContent = deviceText;
    device.prepend(deviceIcon);
    device.title = deviceText;

    const url = li.querySelector(".synced-tab-li-url");
    url.textContent = formatURIForDisplay(tab.url);
    url.title = tab.url;
    document.l10n.setAttributes(url, "firefoxview-tabs-list-tab-button", {
      targetURI,
    });
  }

  /*
     Generate an empty list item ready to represent tab data
  */
  generateListItem(index) {
    // Create new list item
    const li = document.createElement("li");
    li.classList.add("synced-tab-li");

    const a = document.createElement("a");
    a.classList.add("synced-tab-a");
    a.target = "_blank";
    if (index != 0) {
      a.setAttribute("tabindex", "-1");
    }
    a.addEventListener("keydown", this);
    li.appendChild(a);

    const favicon = document.createElement("div");
    favicon.classList.add("favicon");
    a.appendChild(favicon);

    // Hide badge with CSS if not the first child
    const badge = this.createBadge();
    a.appendChild(badge);

    const title = document.createElement("span");
    title.classList.add("synced-tab-li-title");
    a.appendChild(title);

    const url = document.createElement("span");
    url.classList.add("synced-tab-li-url");
    a.appendChild(url);

    const device = document.createElement("span");
    device.classList.add("synced-tab-li-device");
    a.appendChild(device);

    const time = document.createElement("span");
    time.classList.add("synced-tab-li-time");
    a.appendChild(time);

    return li;
  }

  createBadge() {
    const badge = document.createElement("div");
    const dot = document.createElement("span");
    const badgeTextEl = document.createElement("span");
    const badgeText = this.fluentStrings.formatValueSync(
      "firefoxview-pickup-tabs-badge"
    );

    badgeTextEl.classList.add("badge-text");
    badgeTextEl.textContent = badgeText;
    badge.classList.add("last-active-badge");
    dot.classList.add("dot");
    badge.append(dot, badgeTextEl);
    return badge;
  }

  sendTabTelemetry(numTabs) {
    Services.telemetry.recordEvent("firefoxview", "synced_tabs", "tabs", null, {
      count: numTabs.toString(),
    });
  }
}

customElements.define("tab-pickup-list", TabPickupList);
