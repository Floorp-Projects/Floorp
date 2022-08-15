/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm"
);

import {
  formatURIForDisplay,
  convertTimestamp,
  createFaviconElement,
  toggleContainer,
  NOW_THRESHOLD_MS,
} from "./helpers.mjs";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const SS_NOTIFY_CLOSED_OBJECTS_CHANGED = "sessionstore-closed-objects-changed";

function getWindow() {
  return window.browsingContext.embedderWindowGlobal.browsingContext.window;
}

class RecentlyClosedTabsList extends HTMLElement {
  constructor() {
    super();
    this.maxTabsLength = 25;
    this.closedTabsData = [];

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
    return this.querySelectorAll("span.closed-tab-li-time");
  }

  connectedCallback() {
    this.addEventListener("click", this);
    this.addEventListener("keydown", this);
    this.intervalID = setInterval(() => this.updateTime(), lazy.timeMsPref);
  }

  disconnectedCallback() {
    clearInterval(this.intervalID);
  }

  handleEvent(event) {
    if (
      event.type == "click" ||
      (event.type == "keydown" && event.keyCode == KeyEvent.DOM_VK_RETURN)
    ) {
      this.openTabAndUpdate(event);
    } else if (
      event.type == "keydown" &&
      !event.shiftKey &&
      !event.ctrlKey &&
      event.target.classList.contains("closed-tab-li")
    ) {
      switch (event.key) {
        case "ArrowDown":
          event.target.nextSibling?.focus();
          break;
        case "ArrowUp":
          event.target.previousSibling?.focus();
          break;
      }
    }
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
    let index = [...this.tabsList.children].indexOf(item);

    lazy.SessionStore.undoCloseTab(getWindow(), index);
    this.tabsList.removeChild(item);

    // record telemetry
    let tabClosedAt = parseInt(
      item.querySelector(".closed-tab-li-time").getAttribute("data-timestamp")
    );

    let now = Date.now();
    let deltaSeconds = (now - tabClosedAt) / 1000;
    Services.telemetry.recordEvent(
      "firefoxview",
      "recently_closed",
      "tabs",
      null,
      {
        position: (++index).toString(),
        delta: deltaSeconds.toString(),
      }
    );
  }

  initiateTabsList() {
    let closedTabs = lazy.SessionStore.getClosedTabData(getWindow());
    closedTabs = closedTabs.slice(0, this.maxTabsLength);
    this.closedTabsData = closedTabs;

    for (const tab of closedTabs) {
      const li = this.generateListItem(tab);
      this.tabsList.append(li);
    }
    this.tabsList.hidden = false;
  }

  updateTabsList() {
    let newClosedTabs = lazy.SessionStore.getClosedTabData(getWindow());
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

  generateListItem(tab) {
    const li = document.createElement("li");
    li.classList.add("closed-tab-li");
    li.setAttribute("tabindex", 0);
    li.setAttribute("role", "button");

    const title = document.createElement("span");
    title.textContent = `${tab.title}`;
    title.classList.add("closed-tab-li-title");

    const favicon = createFaviconElement(tab.image);
    li.append(favicon);

    const targetURI = this.getTabStateValue(tab, "url");
    li.dataset.targetURI = targetURI;
    document.l10n.setAttributes(li, "firefoxview-tabs-list-tab-button", {
      targetURI,
    });

    const url = document.createElement("span");

    if (targetURI) {
      url.textContent = formatURIForDisplay(targetURI);
      url.title = targetURI;
      url.classList.add("closed-tab-li-url");
    }

    const time = document.createElement("span");
    time.textContent = convertTimestamp(tab.closedAt, this.fluentStrings);
    time.setAttribute("data-timestamp", tab.closedAt);
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
      toggleContainer(this.collapsibleButton, this.collapsibleContainer);
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
      return lazy.SessionStore.getClosedTabCount(getWindow());
    } catch (ex) {
      return 0;
    }
  };
}
customElements.define(
  "recently-closed-tabs-container",
  RecentlyClosedTabsContainer
);
