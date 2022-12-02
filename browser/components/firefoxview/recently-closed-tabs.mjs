/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
const SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH = "sessionstore-browser-shutdown-flush";
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
      (event.type == "click" && !event.altKey) ||
      (event.type == "keydown" && event.keyCode == KeyEvent.DOM_VK_RETURN) ||
      (event.type == "keydown" && event.keyCode == KeyEvent.DOM_VK_SPACE)
    ) {
      if (!event.target.classList.contains("closed-tab-li-dismiss")) {
        this.openTabAndUpdate(event);
      } else {
        this.dismissTabAndUpdate(event);
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

  focusFirstItemOrHeader(dismissedIndex) {
    // When a tab is removed from the list, the focus should
    // remain on the list or the list header. This prevents context
    // switching when navigating back to Firefox View.
    let recentlyClosedList = [...this.tabsList.children];
    if (recentlyClosedList.length) {
      recentlyClosedList.forEach(element =>
        element.setAttribute("tabindex", "-1")
      );
      let mainContent;
      if (dismissedIndex) {
        // Select the item above the one that was just dismissed
        mainContent = recentlyClosedList[dismissedIndex - 1].querySelector(
          ".closed-tab-li-main"
        );
      } else {
        mainContent = recentlyClosedList[0].querySelector(
          ".closed-tab-li-main"
        );
      }
      mainContent.setAttribute("tabindex", "0");
      mainContent.focus();
    } else {
      document.getElementById("recently-closed-tabs-header-section").focus();
    }
  }

  openTabAndUpdate(event) {
    event.preventDefault();
    const item = event.target.closest(".closed-tab-li");
    // only used for telemetry
    const position = [...this.tabsList.children].indexOf(item) + 1;
    const closedId = item.dataset.tabid;

    lazy.SessionStore.undoCloseById(closedId);
    this.tabsList.removeChild(item);

    this.focusFirstItemOrHeader();

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
        position: position.toString(),
        delta: deltaSeconds.toString(),
      }
    );
  }

  dismissTabAndUpdate(event) {
    event.preventDefault();
    const item = event.target.closest(".closed-tab-li");
    let recentlyClosedList = lazy.SessionStore.getClosedTabData(getWindow());
    let closedTabIndex = recentlyClosedList.findIndex(closedTab => {
      return closedTab.closedId === parseInt(item.dataset.tabid, 10);
    });
    if (closedTabIndex < 0) {
      // Tab not found in recently closed list
      return;
    }
    this.tabsList.removeChild(item);
    lazy.SessionStore.forgetClosedTab(getWindow(), closedTabIndex);

    this.focusFirstItemOrHeader(closedTabIndex);

    // record telemetry
    let tabClosedAt = parseInt(
      item.querySelector(".closed-tab-li-time").dataset.timestamp
    );

    let now = Date.now();
    let deltaSeconds = (now - tabClosedAt) / 1000;
    Services.telemetry.recordEvent(
      "firefoxview",
      "recently_closed",
      "tabs",
      null,
      {
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
      let mainContent = li.querySelector(".closed-tab-li-main");
      // Only the first item in the list should be focusable
      if (!this.tabsList.children.length) {
        mainContent.setAttribute("tabindex", "0");
      } else if (this.tabsList.children.length) {
        mainContent.setAttribute("tabindex", "0");
        this.tabsList.children[0].setAttribute("tabindex", "-1");
      }
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

    const title = document.createElement("span");
    title.textContent = `${tab.title}`;
    title.classList.add("closed-tab-li-title");

    const targetURI = this.getTabStateValue(tab, "url");
    const image = tab.image;
    const favicon = createFaviconElement(image, targetURI);

    const urlElement = document.createElement("span");
    urlElement.classList.add("closed-tab-li-url");

    const time = document.createElement("span");
    const convertedTime = convertTimestamp(tab.closedAt, this.fluentStrings);
    time.textContent = convertedTime;
    time.setAttribute("data-timestamp", tab.closedAt);
    time.classList.add("closed-tab-li-time");

    const mainContent = document.createElement("span");
    mainContent.classList.add("closed-tab-li-main");
    mainContent.setAttribute("role", "link");
    mainContent.setAttribute("tabindex", 0);
    mainContent.append(favicon, title, urlElement, time);

    const dismissButton = document.createElement("button");
    let tabTitle = tab.title ?? "";
    document.l10n.setAttributes(
      dismissButton,
      "firefoxview-closed-tabs-dismiss-tab",
      {
        tabTitle,
      }
    );
    dismissButton.classList.add("closed-tab-li-dismiss");

    li.append(mainContent, dismissButton);
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
        SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH
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
        SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH
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
      this.maybeUpdateFocus();
    } else {
      this.removeObserversIfNeeded();
    }
  }

  observe(subject, topic, data) {
    if (
      topic == SS_NOTIFY_CLOSED_OBJECTS_CHANGED ||
      (topic == SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH &&
        subject.ownerGlobal == getWindow())
    ) {
      this.list.updateTabsList();
    }
  }

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

  /**
   * Manages focus when returning to the Firefox View tab
   *
   * @memberof RecentlyClosedTabsContainer
   */
  maybeUpdateFocus() {
    // Check if focus is in the container element
    if (this.contains(document.activeElement)) {
      let listItems = this.list.querySelectorAll("li");
      // More tabs may have been added to the list, so we'll refocus
      // the first item in the list.
      if (listItems.length) {
        listItems[0].querySelector(".closed-tab-li-main").focus();
      } else {
        this.querySelector("summary").focus();
      }
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
