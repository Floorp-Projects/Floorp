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
  onToggleContainer,
  NOW_THRESHOLD_MS,
} from "./helpers.mjs";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const SS_NOTIFY_CLOSED_OBJECTS_CHANGED = "sessionstore-closed-objects-changed";
const SS_NOTIFY_CLOSED_OBJECTS_TAB_STATE_CHANGED =
  "sessionstore-closed-objects-tab-state-changed";
const UI_OPEN_STATE =
  "browser.tabs.firefox-view.ui-state.recently-closed-tabs.open";

function getWindow() {
  return window.browsingContext.embedderWindowGlobal.browsingContext.window;
}

class RecentlyClosedTabsList extends HTMLElement {
  constructor() {
    super();
    this.maxTabsLength = 25;
    this.closedTabsData = new Map();

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
    const activeIndex = tab.state.index - 1;

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

  updateTabsList() {
    let newClosedTabs = lazy.SessionStore.getClosedTabData(getWindow());
    newClosedTabs = newClosedTabs.slice(0, this.maxTabsLength);

    if (this.closedTabsData.size && !newClosedTabs.length) {
      // if a user purges history, clear the list
      while (this.tabsList.lastElementChild) {
        this.tabsList.lastElementChild.remove();
      }
      document
        .getElementById("recently-closed-tabs-container")
        .togglePlaceholderVisibility(true);
      this.tabsList.hidden = true;
      this.closedTabsData = new Map();
      return;
    }

    // First purge obsolete items out of the map so we don't leak them forever:
    for (let id of this.closedTabsData.keys()) {
      if (!newClosedTabs.some(t => t.closedId == id)) {
        this.closedTabsData.delete(id);
      }
    }

    // Then work out which of the new closed tabs are additions and which update
    // existing items:
    let tabsToAdd = [];
    let tabsToUpdate = [];
    for (let newTab of newClosedTabs) {
      let oldTab = this.closedTabsData.get(newTab.closedId);
      this.closedTabsData.set(newTab.closedId, newTab);
      if (!oldTab) {
        tabsToAdd.push(newTab);
      } else if (
        this.getTabStateValue(oldTab, "url") !=
        this.getTabStateValue(newTab, "url")
      ) {
        tabsToUpdate.push(newTab);
      }
    }

    // If there's nothing to add/update, return.
    if (!tabsToAdd.length && !tabsToUpdate.length) {
      return;
    }

    // Add new tabs.
    for (let tab of tabsToAdd.reverse()) {
      if (this.tabsList.children.length == this.maxTabsLength) {
        this.tabsList.lastChild.remove();
      }
      let li = this.generateListItem(tab);
      this.tabsList.prepend(li);
    }

    // Update any recently closed tabs that now have different URLs:
    for (let tab of tabsToUpdate) {
      let tabElement = this.querySelector(
        `.closed-tab-li[data-tabid="${tab.closedId}"]`
      );
      let url = this.getTabStateValue(tab, "url");
      this.updateURLForListItem(tabElement, url);
    }

    // Now unhide the list if necessary:
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
    li.dataset.tabid = tab.closedId;
    li.setAttribute("tabindex", 0);
    li.setAttribute("role", "button");

    const title = document.createElement("span");
    title.textContent = `${tab.title}`;
    title.classList.add("closed-tab-li-title");

    const favicon = createFaviconElement(tab.image);
    li.append(favicon);

    const targetURI = this.getTabStateValue(tab, "url");

    const urlElement = document.createElement("span");
    urlElement.classList.add("closed-tab-li-url");

    const time = document.createElement("span");
    time.textContent = convertTimestamp(tab.closedAt, this.fluentStrings);
    time.setAttribute("data-timestamp", tab.closedAt);
    time.classList.add("closed-tab-li-time");

    li.append(title, urlElement, time);
    this.updateURLForListItem(li, targetURI);
    return li;
  }

  // Update the URL for a new or previously-populated list item.
  // This is needed because when tabs get closed we don't necessarily
  // have all the requisite information for them immediately.
  updateURLForListItem(li, targetURI) {
    li.dataset.targetURI = targetURI;
    document.l10n.setAttributes(li, "firefoxview-tabs-list-tab-button", {
      targetURI,
    });
    let urlElement = li.querySelector(".closed-tab-li-url");
    if (targetURI) {
      urlElement.textContent = formatURIForDisplay(targetURI);
      urlElement.title = targetURI;
    } else {
      urlElement.textContent = urlElement.title = "";
    }
  }
}
customElements.define("recently-closed-tabs-list", RecentlyClosedTabsList);

class RecentlyClosedTabsContainer extends HTMLDetailsElement {
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
    this.addEventListener("toggle", this);
    getWindow().gBrowser.tabContainer.addEventListener("TabSelect", this);
    this.open = Services.prefs.getBoolPref(UI_OPEN_STATE, true);
  }

  cleanup() {
    getWindow().gBrowser.tabContainer.removeEventListener("TabSelect", this);
    this.removeObserversIfNeeded();
  }

  addObserversIfNeeded() {
    if (!this.observerAdded) {
      Services.obs.addObserver(
        this.boundObserve,
        SS_NOTIFY_CLOSED_OBJECTS_CHANGED
      );
      Services.obs.addObserver(
        this.boundObserve,
        SS_NOTIFY_CLOSED_OBJECTS_TAB_STATE_CHANGED
      );
      this.observerAdded = true;
    }
  }

  removeObserversIfNeeded() {
    if (this.observerAdded) {
      Services.obs.removeObserver(
        this.boundObserve,
        SS_NOTIFY_CLOSED_OBJECTS_CHANGED
      );
      Services.obs.removeObserver(
        this.boundObserve,
        SS_NOTIFY_CLOSED_OBJECTS_TAB_STATE_CHANGED
      );
      this.observerAdded = false;
    }
  }

  // we observe when a tab closes but since this notification fires more frequently and on
  // all windows, we remove the observer when another tab is selected; we check for changes
  // to the session store once the user return to this tab.
  handleObservers(contentDocument) {
    if (contentDocument?.URL == "about:firefoxview") {
      this.addObserversIfNeeded();
      this.list.updateTabsList();
    } else {
      this.removeObserversIfNeeded();
    }
  }

  observe = () => this.list.updateTabsList();

  onLoad() {
    if (this.getClosedTabCount() == 0) {
      this.togglePlaceholderVisibility(true);
    } else {
      this.list.updateTabsList();
    }
    this.addObserversIfNeeded();
  }

  handleEvent(event) {
    if (event.type == "toggle") {
      onToggleContainer(this);
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
  RecentlyClosedTabsContainer,
  {
    extends: "details",
  }
);
