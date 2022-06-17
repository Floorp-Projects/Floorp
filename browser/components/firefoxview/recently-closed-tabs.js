/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
});

const relativeTimeFormat = new Services.intl.RelativeTimeFormat(undefined, {});
const SS_NOTIFY_CLOSED_OBJECTS_CHANGED = "sessionstore-closed-objects-changed";

function getWindow() {
  return window.browsingContext.embedderWindowGlobal.browsingContext.window;
}

class RecentlyClosedTabsList extends HTMLElement {
  constructor() {
    super();
    this.maxTabsLength = 25;
    this.closedTabsData = [];
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
    this.addEventListener("keydown", this);
  }

  handleEvent(event) {
    if (
      event.type == "click" ||
      (event.type == "keydown" && event.keyCode == KeyEvent.DOM_VK_RETURN)
    ) {
      this.openTabAndUpdate(event);
    }
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

  getTabStateValue(tab, key) {
    let value = "";
    const tabEntries = tab.state.entries;
    const activeIndex = tabEntries.length - 1;

    if (activeIndex >= 0 && tabEntries[activeIndex]) {
      value = tabEntries[activeIndex][key];
    }

    return value;
  }

  openTabAndUpdate(event) {
    event.preventDefault();
    const item = event.target.closest(".closed-tab-li");
    const index = [...this.tabsList.children].indexOf(item);

    SessionStore.undoCloseTab(getWindow(), index);
    this.tabsList.removeChild(item);
  }

  initiateTabsList() {
    let closedTabs = SessionStore.getClosedTabData(getWindow());
    closedTabs = closedTabs.slice(0, this.maxTabsLength);
    this.closedTabsData = closedTabs;

    for (const tab of closedTabs) {
      const li = this.generateListItem(tab);
      this.tabsList.append(li);
    }
    this.tabsList.hidden = false;
  }

  updateTabsList() {
    let newClosedTabs = SessionStore.getClosedTabData(getWindow());
    newClosedTabs = newClosedTabs.slice(0, this.maxTabsLength);

    if (this.closedTabsData.length && !newClosedTabs.length) {
      // if a user purges history, clear the list
      [...this.tabsList.children].forEach(node =>
        this.tabsList.removeChild(node)
      );
      document
        .getElementById("recently-closed-tabs-container")
        .togglePlaceholderVisibility(true);
      this.tabsList.hidden = true;
      this.closedTabsData = [];
      return;
    }

    const tabsToAdd = newClosedTabs.filter(
      newTab =>
        !this.closedTabsData.some(tab => {
          return (
            this.getTabStateValue(tab, "ID") ==
            this.getTabStateValue(newTab, "ID")
          );
        })
    );

    if (!tabsToAdd.length) {
      return;
    }

    for (let tab of tabsToAdd.reverse()) {
      if (this.tabsList.children.length == this.maxTabsLength) {
        this.tabsList.lastChild.remove();
      }
      let li = this.generateListItem(tab);
      this.tabsList.prepend(li);
    }

    this.closedTabsData = newClosedTabs;

    // for situations where the tab list will initially be empty (such as
    // with new profiles or automatic session restore is disabled) and
    // this.initiateTabsList won't be called
    if (this.tabsList.hidden) {
      this.tabsList.hidden = false;
      document
        .getElementById("recently-closed-tabs-container")
        .togglePlaceholderVisibility(false);
    }
  }

  setFavicon(tab) {
    const imageUrl = tab.image
      ? PlacesUIUtils.getImageURL(tab)
      : "chrome://global/skin/icons/defaultFavicon.svg";
    let favicon = document.createElement("div");

    favicon.style.backgroundImage = `url('${imageUrl}')`;
    favicon.classList.add("favicon");
    favicon.setAttribute("role", "presentation");
    return favicon;
  }

  generateListItem(tab) {
    const li = document.createElement("li");
    li.classList.add("closed-tab-li");
    li.setAttribute("tabindex", 0);
    li.setAttribute("role", "button");

    const title = document.createElement("span");
    title.textContent = `${tab.title}`;
    title.classList.add("closed-tab-li-title");

    const favicon = this.setFavicon(tab);
    li.append(favicon);

    const targetURI = this.getTabStateValue(tab, "url");
    li.dataset.targetURI = targetURI;
    document.l10n.setAttributes(li, "firefoxview-closed-tabs-tab-button", {
      targetURI,
    });

    const url = document.createElement("span");

    if (targetURI) {
      url.textContent = this.formatURIForDisplay(targetURI);
      url.classList.add("closed-tab-li-url");
    }

    const time = document.createElement("span");
    time.textContent = this.convertTimestamp(tab.closedAt);
    time.classList.add("closed-tab-li-time");

    li.append(title, url, time);
    return li;
  }
}
customElements.define("recently-closed-tabs-list", RecentlyClosedTabsList);

class RecentlyClosedTabsContainer extends HTMLElement {
  constructor() {
    super();
    this.observerAdded = false;
    this.boundObserve = (...args) => this.observe(...args);
  }

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

    getWindow().gBrowser.tabContainer.addEventListener("TabSelect", this);
  }

  cleanup() {
    getWindow().gBrowser.tabContainer.removeEventListener("TabSelect", this);

    if (this.observerAdded) {
      Services.obs.removeObserver(
        this.boundObserve,
        SS_NOTIFY_CLOSED_OBJECTS_CHANGED
      );
    }
  }

  // we observe when a tab closes but since this notification fires more frequently and on
  // all windows, we remove the observer when another tab is selected; we check for changes
  // to the session store once the user return to this tab.
  handleObservers(contentDocument) {
    if (
      !this.observerAdded &&
      contentDocument &&
      contentDocument.URL == "about:firefoxview"
    ) {
      Services.obs.addObserver(
        this.boundObserve,
        SS_NOTIFY_CLOSED_OBJECTS_CHANGED
      );
      this.observerAdded = true;
      this.list.updateTabsList();
    } else if (this.observerAdded) {
      Services.obs.removeObserver(
        this.boundObserve,
        SS_NOTIFY_CLOSED_OBJECTS_CHANGED
      );
      this.observerAdded = false;
    }
  }

  observe = () => this.list.updateTabsList();

  onLoad() {
    if (this.getClosedTabCount() == 0) {
      this.togglePlaceholderVisibility(true);
    } else {
      this.list.initiateTabsList();
    }
    Services.obs.addObserver(
      this.boundObserve,
      SS_NOTIFY_CLOSED_OBJECTS_CHANGED
    );
    this.observerAdded = true;
  }

  handleEvent(event) {
    if (event.type == "click" && event.target == this.collapsibleButton) {
      this.toggleTabs();
    } else if (event.type == "TabSelect") {
      this.handleObservers(event.target.linkedBrowser.contentDocument);
    }
  }

  togglePlaceholderVisibility(visible) {
    this.noTabsElement.toggleAttribute("hidden", !visible);
    this.collapsibleContainer.classList.toggle("empty-container", visible);
  }

  getClosedTabCount = () => {
    try {
      return SessionStore.getClosedTabCount(getWindow());
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
