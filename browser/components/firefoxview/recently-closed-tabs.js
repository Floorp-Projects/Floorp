/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(globalThis, {
  Services: "resource://gre/modules/Services.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
});

const relativeTimeFormat = new Services.intl.RelativeTimeFormat(undefined, {});

class RecentlyClosedTabsList extends HTMLElement {
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
      const item = event.target.closest(".closed-tab-li");
      event.preventDefault();
      this.openTab(item.dataset.targetURI);
    }
  }

  getWindow() {
    return window.windowRoot.ownerGlobal;
  }

  convertTimestamp(timestamp) {
    const elapsed = Date.now() - timestamp;
    const nowThresholdMs = 91000;
    let formattedTime;
    if (elapsed <= nowThresholdMs) {
      // Use a different string for very recent timestamps
      formattedTime = this.fluentStrings.formatValueSync(
        "firefoxview-just-now-timestamp"
      );
    } else {
      formattedTime = relativeTimeFormat.formatBestUnit(new Date(timestamp));
    }
    return formattedTime;
  }

  formatURIForDisplay(uriString) {
    // TODO: Bug 1764816: Make sure we handle file:///, jar:, blob, IP4/IP6 etc. addresses
    let uri;
    try {
      uri = Services.io.newURI(uriString);
    } catch (ex) {
      return uriString;
    }
    if (!uri.asciiHost) {
      return uriString;
    }
    let displayHost;
    try {
      // This might fail if it's an IP address or doesn't have more than 1 part
      displayHost = Services.eTLD.getBaseDomain(uri);
    } catch (ex) {
      return uri.displayHostPort;
    }
    return displayHost.length ? displayHost : uriString;
  }

  getTargetURI(tab) {
    let targetURI = "";
    const tabEntries = tab.state.entries;
    const activeIndex = tabEntries.length - 1;

    if (activeIndex >= 0 && tabEntries[activeIndex]) {
      targetURI = tabEntries[activeIndex].url;
    }

    return targetURI;
  }

  openTab(targetURI) {
    window.open(targetURI, "_blank");
  }

  generateTabs() {
    let closedTabs = SessionStore.getClosedTabData(this.getWindow());
    closedTabs = closedTabs.slice(0, 25);

    for (const tab of closedTabs) {
      let li = document.createElement("li");
      li.classList.add("closed-tab-li");

      if (tab.image) {
        // TODO - figure out how to render this properly
        PlacesUIUtils.setImage(tab, li);
      }

      let title = document.createElement("span");
      title.textContent = `${tab.title}`;
      title.classList.add("closed-tab-li-title");

      const targetURI = this.getTargetURI(tab);
      li.dataset.targetURI = targetURI;
      document.l10n.setAttributes(li, "firefoxview-closed-tabs-tab-button", {
        targetURI,
      });

      let url = document.createElement("span");

      if (targetURI) {
        url.textContent = this.formatURIForDisplay(targetURI);
        url.classList.add("closed-tab-li-url");
      }

      let time = document.createElement("span");
      time.textContent = this.convertTimestamp(tab.closedAt);
      time.classList.add("closed-tab-li-time");

      li.append(title, url, time);
      this.tabsList.appendChild(li);
    }
    this.tabsList.hidden = false;
  }
}
customElements.define("recently-closed-tabs-list", RecentlyClosedTabsList);

class RecentlyClosedTabsContainer extends HTMLElement {
  getWindow = () => window.windowRoot.ownerGlobal;

  connectedCallback() {
    this.noTabsElement = this.querySelector(
      "#recently-closed-tabs-placeholder"
    );
    this.list = this.querySelector("recently-closed-tabs-list");
    this.collapsibleContainer = this.querySelector(
      "#collapsible-tabs-container"
    );
    this.collapsibleButton = this.querySelector("#collapsible-tabs-button");

    this.collapsibleButton.addEventListener("click", this);
  }

  onLoad() {
    if (this.getClosedTabCount() == 0) {
      this.noTabsElement.hidden = false;
      this.collapsibleContainer.classList.add("empty-container");
    } else {
      this.list.generateTabs();
    }
  }

  handleEvent(event) {
    if (event.type == "click" && event.target == this.collapsibleButton) {
      this.toggleTabs();
    }
  }

  getClosedTabCount = () => {
    try {
      return SessionStore.getClosedTabCount(this.getWindow());
    } catch (ex) {
      return 0;
    }
  };

  toggleTabs = () => {
    const arrowUp = "arrow-up";
    const arrowDown = "arrow-down";
    const isOpen = this.collapsibleButton.classList.contains(arrowUp);

    this.collapsibleButton.classList.toggle(arrowUp, !isOpen);
    this.collapsibleButton.classList.toggle(arrowDown, isOpen);
    this.collapsibleContainer.hidden = isOpen;
  };
}
customElements.define(
  "recently-closed-tabs-container",
  RecentlyClosedTabsContainer
);
