/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "SyncedTabs",
  "resource://services-sync/SyncedTabs.jsm"
);

import {
  formatURIForDisplay,
  convertTimestamp,
  createFaviconElement,
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
    this.boundObserve = (...args) => this.getSyncedTabData(...args);

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
    this.getSyncedTabData();

    Services.obs.addObserver(this.boundObserve, SYNCED_TABS_CHANGED);
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
  }

  cleanup() {
    Services.obs.removeObserver(this.boundObserve, SYNCED_TABS_CHANGED);
    clearInterval(this.intervalID);
  }

  updateTime() {
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

  updateTabsList(syncedTabs) {
    // don't do anything while the loading state is active
    if (this.tabPickupContainer.classList.contains("loading")) {
      return;
    }

    while (this.tabsList.firstChild) {
      this.tabsList.firstChild.remove();
    }

    if (!syncedTabs.length) {
      this.togglePlaceholderVisibility(true);
      this.tabsList.hidden = true;
      return;
    }

    for (let i = 0; i < this.maxTabsLength; i++) {
      let li = null;
      if (!syncedTabs[i]) {
        li = this.generatePlaceholder();
      } else {
        li = this.generateListItem(syncedTabs[i], i);
      }
      this.tabsList.append(li);
    }

    if (this.tabsList.hidden) {
      this.tabsList.hidden = false;
      this.togglePlaceholderVisibility(false);

      if (!this.intervalID) {
        this.intervalID = setInterval(() => this.updateTime(), lazy.timeMsPref);
      }
    }

    this.sendTabTelemetry(syncedTabs.length);
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

  generateListItem(tab, index) {
    const li = document.createElement("li");
    li.classList.add("synced-tab-li");
    li.dataset.deviceType = tab.deviceType;

    const targetURI = tab.url;
    const a = document.createElement("a");
    a.classList.add("synced-tab-a");
    a.href = targetURI;
    a.target = "_blank";
    document.l10n.setAttributes(a, "firefoxview-tabs-list-tab-button", {
      targetURI,
    });

    const title = document.createElement("span");
    title.textContent = tab.title;
    title.classList.add("synced-tab-li-title");

    const favicon = createFaviconElement(tab.icon);

    const lastUsedMs = tab.lastUsed * 1000;
    const time = document.createElement("span");
    time.textContent = convertTimestamp(lastUsedMs, this.fluentStrings);
    time.classList.add("synced-tab-li-time");
    time.setAttribute("data-timestamp", lastUsedMs);

    const url = document.createElement("span");
    const device = document.createElement("span");
    const deviceIcon = document.createElement("div");
    deviceIcon.classList.add("icon", tab.deviceType);
    deviceIcon.setAttribute("role", "presentation");

    const deviceText = tab.device;
    device.textContent = deviceText;
    device.prepend(deviceIcon);
    device.title = deviceText;

    url.textContent = formatURIForDisplay(tab.url);
    url.title = tab.url;
    url.classList.add("synced-tab-li-url");
    device.classList.add("synced-tab-li-device");

    // the first list item is different from the second and third
    if (index == 0) {
      const badge = this.createBadge();
      a.append(favicon, badge, title, url, device, time);
    } else {
      a.append(favicon, title, url, device, time);
    }

    li.append(a);
    return li;
  }

  createBadge() {
    const badge = document.createElement("div");
    const dot = document.createElement("span");
    const badgeText = document.createElement("span");

    badgeText.setAttribute("data-l10n-id", "firefoxview-pickup-tabs-badge");
    badgeText.classList.add("badge-text");
    badge.classList.add("last-active-badge");
    dot.classList.add("dot");
    badge.append(dot, badgeText);
    return badge;
  }

  sendTabTelemetry(numTabs) {
    Services.telemetry.recordEvent("firefoxview", "synced_tabs", "tabs", null, {
      count: numTabs.toString(),
    });
  }
}

customElements.define("tab-pickup-list", TabPickupList);
