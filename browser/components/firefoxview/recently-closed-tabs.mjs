/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

import {
  formatURIForDisplay,
  convertTimestamp,
  getImageUrl,
  onToggleContainer,
  NOW_THRESHOLD_MS,
} from "./helpers.mjs";

import {
  html,
  ifDefined,
  styleMap,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

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

class RecentlyClosedTabsList extends MozLitElement {
  constructor() {
    super();
    this.maxTabsLength = 25;
    this.recentlyClosedTabs = [];
    this.lastFocusedIndex = -1;

    // The recency timestamp update period is stored in a pref to allow tests to easily change it
    XPCOMUtils.defineLazyPreferenceGetter(
      lazy,
      "timeMsPref",
      "browser.tabs.firefox-view.updateTimeMs",
      NOW_THRESHOLD_MS,
      timeMsPref => {
        clearInterval(this.intervalID);
        this.intervalID = setInterval(() => this.requestUpdate(), timeMsPref);
        this.requestUpdate();
      }
    );
  }

  createRenderRoot() {
    return this;
  }

  static queries = {
    tabsList: "ol",
    timeElements: { all: "span.closed-tab-li-time" },
  };

  get fluentStrings() {
    if (!this._fluentStrings) {
      this._fluentStrings = new Localization(["browser/firefoxView.ftl"], true);
    }
    return this._fluentStrings;
  }

  connectedCallback() {
    super.connectedCallback();
    this.intervalID = setInterval(() => this.requestUpdate(), lazy.timeMsPref);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    clearInterval(this.intervalID);
  }

  getTabStateValue(tab, key) {
    let value = "";
    const tabEntries = tab.entries;
    const activeIndex = tab.index - 1;

    if (activeIndex >= 0 && tabEntries[activeIndex]) {
      value = tabEntries[activeIndex][key];
    }

    return value;
  }

  openTabAndUpdate(event) {
    if (
      (event.type == "click" && !event.altKey) ||
      (event.type == "keydown" && event.code == "Enter") ||
      (event.type == "keydown" && event.code == "Space")
    ) {
      const item = event.target.closest(".closed-tab-li");
      // only used for telemetry
      const position = [...this.tabsList.children].indexOf(item) + 1;
      const closedId = item.dataset.tabid;

      lazy.SessionStore.undoCloseById(closedId, undefined, getWindow());

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
  }

  dismissTabAndUpdate(event) {
    event.preventDefault();
    const item = event.target.closest(".closed-tab-li");
    this.dismissTabAndUpdateForElement(item);
  }

  dismissTabAndUpdateForElement(item) {
    const sourceWindow = lazy.SessionStore.getWindowById(
      item.dataset.sourceWindowId
    );
    let recentlyClosedList =
      lazy.SessionStore.getClosedTabDataForWindow(sourceWindow);
    let closedTabIndex = recentlyClosedList.findIndex(closedTab => {
      return closedTab.closedId === parseInt(item.dataset.tabid, 10);
    });
    if (closedTabIndex < 0) {
      // Tab not found in recently closed list
      return;
    }
    // in order forget a closed tab, we need to pass the window the closed tab data is associated with
    lazy.SessionStore.forgetClosedTab(sourceWindow, closedTabIndex);

    // record telemetry
    let tabClosedAt = parseInt(
      item.querySelector(".closed-tab-li-time").dataset.timestamp
    );

    let now = Date.now();
    let deltaSeconds = (now - tabClosedAt) / 1000;
    Services.telemetry.recordEvent(
      "firefoxview",
      "dismiss_closed_tab",
      "tabs",
      null,
      {
        delta: deltaSeconds.toString(),
      }
    );
  }

  getClosedTabsDataForOpenWindows() {
    // get closed tabs in currently-open windows
    const closedTabsData = lazy.SessionStore.getClosedTabData(getWindow()).map(
      tabData => {
        // flatten the object; move properties of `.state` into the top-level object
        const stateData = tabData.state;
        delete tabData.state;
        return {
          ...tabData,
          ...stateData,
        };
      }
    );
    return closedTabsData;
  }

  updateRecentlyClosedTabs() {
    // add recently-closed tabs from currently-open windows
    const recentlyClosedTabsData = this.getClosedTabsDataForOpenWindows();

    recentlyClosedTabsData.sort((a, b) => a.closedAt < b.closedAt);
    this.recentlyClosedTabs = recentlyClosedTabsData.slice(
      0,
      this.maxTabsLength
    );
    this.requestUpdate();
  }

  render() {
    let { recentlyClosedTabs } = this;
    let closedTabsContainer = document.getElementById(
      "recently-closed-tabs-container"
    );

    if (!recentlyClosedTabs.length) {
      // Show empty message if no recently closed tabs
      closedTabsContainer.toggleContainerStyleForEmptyMsg(true);
      return html` ${this.emptyMessageTemplate()} `;
    }

    closedTabsContainer.toggleContainerStyleForEmptyMsg(false);

    return html`
      <ol class="closed-tabs-list">
        ${recentlyClosedTabs.map((tab, i) =>
          this.recentlyClosedTabTemplate(tab, !i)
        )}
      </ol>
    `;
  }

  willUpdate() {
    if (this.tabsList && this.tabsList.contains(document.activeElement)) {
      let activeLi = document.activeElement.closest(".closed-tab-li");
      this.lastFocusedIndex = [...this.tabsList.children].indexOf(activeLi);
    } else {
      this.lastFocusedIndex = -1;
    }
  }

  updated() {
    let focusRestored = false;
    if (
      this.lastFocusedIndex >= 0 &&
      (!this.tabsList || this.lastFocusedIndex >= this.tabsList.children.length)
    ) {
      if (this.tabsList) {
        let items = [...this.tabsList.children];
        let newFocusIndex = items.length - 1;
        let newFocus = items[newFocusIndex];
        if (newFocus) {
          focusRestored = true;
          newFocus.querySelector(".closed-tab-li-main").focus();
        }
      }
      if (!focusRestored) {
        document.getElementById("recently-closed-tabs-header-section").focus();
      }
    }
    this.lastFocusedIndex = -1;
  }

  emptyMessageTemplate() {
    return html`
      <div
        id="recently-closed-tabs-placeholder"
        class="placeholder-content"
        role="presentation"
      >
        <img
          id="recently-closed-empty-image"
          src="chrome://browser/content/firefoxview/recently-closed-empty.svg"
          role="presentation"
          alt=""
        />
        <div class="placeholder-text">
          <h4
            data-l10n-id="firefoxview-closed-tabs-placeholder-header"
            class="placeholder-header"
          ></h4>
          <p
            data-l10n-id="firefoxview-closed-tabs-placeholder-body2"
            class="placeholder-body"
          ></p>
        </div>
      </div>
    `;
  }

  recentlyClosedTabTemplate(tab, primary) {
    const targetURI = this.getTabStateValue(tab, "url");
    const convertedTime = convertTimestamp(
      tab.closedAt,
      this.fluentStrings,
      lazy.timeMsPref
    );
    return html`
      <li
        class="closed-tab-li"
        data-tabid=${tab.closedId}
        data-source-window-id=${tab.sourceWindowId}
        data-targeturi=${targetURI}
        tabindex=${ifDefined(primary ? null : "-1")}
        @contextmenu=${e => (this.contextTriggerNode = e.currentTarget)}
      >
        <span
          class="closed-tab-li-main"
          role="button"
          tabindex="0"
          @click=${e => this.openTabAndUpdate(e)}
          @keydown=${e => this.openTabAndUpdate(e)}
        >
          <div
            class="favicon"
            style=${styleMap({
              backgroundImage: `url(${getImageUrl(tab.icon, targetURI)})`,
            })}
          ></div>
          <a
            href=${targetURI}
            class="closed-tab-li-title"
            tabindex="-1"
            @click=${e => e.preventDefault()}
          >
            ${tab.title}
          </a>
          <a
            href=${targetURI}
            class="closed-tab-li-url"
            data-l10n-id="firefoxview-tabs-list-tab-button"
            data-l10n-args=${JSON.stringify({ targetURI })}
            tabindex="-1"
            @click=${e => e.preventDefault()}
          >
            ${formatURIForDisplay(targetURI)}
          </a>
          <span class="closed-tab-li-time" data-timestamp=${tab.closedAt}>
            ${convertedTime}
          </span>
        </span>
        <button
          class="closed-tab-li-dismiss"
          data-l10n-id="firefoxview-closed-tabs-dismiss-tab"
          data-l10n-args=${JSON.stringify({ tabTitle: tab.title })}
          @click=${e => this.dismissTabAndUpdate(e)}
        ></button>
      </li>
    `;
  }

  // Update the URL for a new or previously-populated list item.
  // This is needed because when tabs get closed we don't necessarily
  // have all the requisite information for them immediately.
  updateURLForListItem(li, targetURI) {
    li.dataset.targetURI = targetURI;
    let urlElement = li.querySelector(".closed-tab-li-url");
    document.l10n.setAttributes(
      urlElement,
      "firefoxview-tabs-list-tab-button",
      {
        targetURI,
      }
    );
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
    getWindow().addEventListener("command", this, true);
    getWindow()
      .document.getElementById("contentAreaContextMenu")
      .addEventListener("popuphiding", this);
    this.open = Services.prefs.getBoolPref(UI_OPEN_STATE, true);
  }

  cleanup() {
    getWindow().gBrowser.tabContainer.removeEventListener("TabSelect", this);
    getWindow().removeEventListener("command", this, true);
    getWindow()
      .document.getElementById("contentAreaContextMenu")
      .removeEventListener("popuphiding", this);
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
      this.list.updateRecentlyClosedTabs();
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
      this.list.updateRecentlyClosedTabs();
    }
  }

  onLoad() {
    this.list.updateRecentlyClosedTabs();
    this.addObserversIfNeeded();
  }

  handleEvent(event) {
    if (event.type == "toggle") {
      onToggleContainer(this);
    } else if (event.type == "TabSelect") {
      this.handleObservers(event.target.linkedBrowser.contentDocument);
    } else if (
      event.type === "command" &&
      event.target.closest(".context-menu-open-link") &&
      this.list.contextTriggerNode
    ) {
      this.list.dismissTabAndUpdateForElement(this.list.contextTriggerNode);
    } else if (event.type === "popuphiding") {
      delete this.list.contextTriggerNode;
    }
  }

  toggleContainerStyleForEmptyMsg(visible) {
    this.collapsibleContainer.classList.toggle("empty-container", visible);
  }
}
customElements.define(
  "recently-closed-tabs-container",
  RecentlyClosedTabsContainer,
  {
    extends: "details",
  }
);
