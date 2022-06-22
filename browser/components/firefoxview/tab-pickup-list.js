/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

import {
  formatURIForDisplay,
  convertTimestamp,
  createFaviconElement,
} from "./helpers.js";

class TabPickupList extends HTMLElement {
  constructor() {
    super();
    this.maxTabsLength = 3;
  }

  get tabsList() {
    return this.querySelector("ol");
  }

  get fluentStrings() {
    if (!this._fluentStrings) {
      this._fluentStrings = new Localization(["preview/firefoxView.ftl"], true);
    }
    return this._fluentStrings;
  }

  connectedCallback() {
    this.addEventListener("click", this);
  }

  handleEvent(event) {
    if (event.type == "click") {
      this.openTab(event);
    }
  }

  openTab(event) {
    event.preventDefault();
    const item = event.target.closest(".synced-tab-li");
    window.open(item.dataset.targetURI, "_blank");
  }

  async getSyncedTabData() {
    let tabs = [];
    let clients = await SyncedTabs.getTabClients();

    for (let client of clients) {
      for (let tab of client.tabs) {
        tab.device = client.name;
        tab.deviceType = client.clientType;
      }
      tabs = [...tabs, ...client.tabs.reverse()];
    }
    tabs = tabs
      .sort((a, b) => b.lastUsed - a.lastUsed)
      .slice(0, this.maxTabsLength);

    if (tabs.length) {
      this.initiateTabsList(tabs);
    } else {
      // TODO show empty state placeholder
    }
  }

  initiateTabsList(syncedTabs) {
    for (let i = 0; i < syncedTabs.length; i++) {
      const li = this.generateListItem(syncedTabs[i], i);
      this.tabsList.append(li);
    }
    this.tabsList.hidden = false;
  }

  generateListItem(tab, index) {
    const li = document.createElement("li");
    li.classList.add("synced-tab-li");
    li.setAttribute("tabindex", 0);
    li.setAttribute("role", "button");

    const title = document.createElement("span");
    title.textContent = tab.title;
    title.classList.add("synced-tab-li-title");

    const favicon = createFaviconElement(tab.icon);
    const targetURI = tab.url;

    li.dataset.targetURI = targetURI;
    document.l10n.setAttributes(li, "firefoxview-tabs-list-tab-button", {
      targetURI,
    });

    const time = document.createElement("span");
    time.textContent = convertTimestamp(
      tab.lastUsed * 1000,
      this.fluentStrings
    );
    time.classList.add("synced-tab-li-time");

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
    url.classList.add("synced-tab-li-url");
    device.classList.add("synced-tab-li-device");

    // the first list item is diffent from second and third
    if (index == 0) {
      const badge = this.createBadge();
      li.append(favicon, badge, title, url, device, time);
    } else {
      const urlWithDevice = document.createElement("span");
      urlWithDevice.append(url, " • ", device);
      urlWithDevice.classList.add("synced-tab-li-url-device");
      li.append(favicon, title, urlWithDevice, time);
    }

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
}

customElements.define("tab-pickup-list", TabPickupList);
