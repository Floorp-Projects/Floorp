/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { classMap, html } from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/card-container.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-tab-list.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

const SS_NOTIFY_CLOSED_OBJECTS_CHANGED = "sessionstore-closed-objects-changed";
const SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH = "sessionstore-browser-shutdown-flush";
const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";
const INCLUDE_CLOSED_TABS_FROM_CLOSED_WINDOWS =
  "browser.sessionstore.closedTabsFromClosedWindows";

function getWindow() {
  return window.browsingContext.embedderWindowGlobal.browsingContext.window;
}

class RecentlyClosedTabsInView extends ViewPage {
  constructor() {
    super();
    this.boundObserve = (...args) => this.observe(...args);
    this.fullyUpdated = false;
    this.maxTabsLength = this.recentBrowsing ? 5 : 25;
    this.recentlyClosedTabs = [];
  }

  static queries = {
    cardEl: "card-container",
    emptyState: "fxview-empty-state",
    tabList: "fxview-tab-list",
  };

  observe(subject, topic, data) {
    if (
      topic == SS_NOTIFY_CLOSED_OBJECTS_CHANGED ||
      (topic == SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH &&
        subject.ownerGlobal == getWindow())
    ) {
      this.updateRecentlyClosedTabs();
    }
  }

  connectedCallback() {
    super.connectedCallback();
    this.updateRecentlyClosedTabs();
    this.addObserversIfNeeded();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
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
  // all windows, we remove the observer when another tab is selected
  viewTabHiddenCallback() {
    this.removeObserversIfNeeded();
  }

  // we check for changes to the session store once the user return to this tab.
  viewTabVisibleCallback() {
    this.addObserversIfNeeded();
    this.updateRecentlyClosedTabs();
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

  updateRecentlyClosedTabs() {
    let recentlyClosedTabsData = lazy.SessionStore.getClosedTabData(
      getWindow()
    );
    if (Services.prefs.getBoolPref(INCLUDE_CLOSED_TABS_FROM_CLOSED_WINDOWS)) {
      recentlyClosedTabsData.push(
        ...lazy.SessionStore.getClosedTabDataFromClosedWindows()
      );
    }
    // sort the aggregated list to most-recently-closed first
    recentlyClosedTabsData.sort((a, b) => a.closedAt < b.closedAt);
    this.recentlyClosedTabs = recentlyClosedTabsData.slice(
      0,
      this.maxTabsLength
    );
    this.normalizeRecentlyClosedData();
    this.requestUpdate();
  }

  normalizeRecentlyClosedData() {
    // Normalize data for fxview-tabs-list
    this.recentlyClosedTabs.forEach(recentlyClosedItem => {
      const targetURI = this.getTabStateValue(recentlyClosedItem, "url");
      recentlyClosedItem.time = recentlyClosedItem.closedAt;
      recentlyClosedItem.icon = recentlyClosedItem.image;
      recentlyClosedItem.primaryL10nId = "fxviewtabrow-tabs-list-tab";
      recentlyClosedItem.primaryL10nArgs = JSON.stringify({
        targetURI,
      });
      recentlyClosedItem.secondaryL10nId =
        "firefoxview-closed-tabs-dismiss-tab";
      recentlyClosedItem.secondaryL10nArgs = JSON.stringify({
        tabTitle: recentlyClosedItem.title,
      });
      recentlyClosedItem.url = targetURI;
    });
  }

  onReopenTab(e) {
    const closedId = parseInt(e.originalTarget.closedId, 10);
    const sourceClosedId = parseInt(e.originalTarget.sourceClosedId, 10);
    if (isNaN(sourceClosedId)) {
      lazy.SessionStore.undoCloseById(closedId, getWindow());
    } else {
      lazy.SessionStore.undoClosedTabFromClosedWindow(
        { sourceClosedId },
        closedId,
        getWindow()
      );
    }

    // Record telemetry
    let tabClosedAt = parseInt(e.originalTarget.time);
    const position =
      Array.from(this.tabList.rowEls).indexOf(e.originalTarget) + 1;

    let now = Date.now();
    let deltaSeconds = (now - tabClosedAt) / 1000;
    Services.telemetry.recordEvent(
      "firefoxview_next",
      "recently_closed",
      "tabs",
      null,
      {
        position: position.toString(),
        delta: deltaSeconds.toString(),
        page: this.recentBrowsing ? "recentbrowsing" : "recentlyclosed",
      }
    );
  }

  onDismissTab(e) {
    const closedId = parseInt(e.originalTarget.closedId, 10);
    const sourceClosedId = parseInt(e.originalTarget.sourceClosedId, 10);
    const sourceWindowId = e.originalTarget.souceWindowId;
    if (sourceWindowId || !isNaN(sourceClosedId)) {
      lazy.SessionStore.forgetClosedTabById(closedId, {
        sourceClosedId,
        sourceWindowId,
      });
    } else {
      lazy.SessionStore.forgetClosedTabById(closedId);
    }

    // Record telemetry
    let tabClosedAt = parseInt(e.originalTarget.time);
    const position =
      Array.from(this.tabList.rowEls).indexOf(e.originalTarget) + 1;

    let now = Date.now();
    let deltaSeconds = (now - tabClosedAt) / 1000;
    Services.telemetry.recordEvent(
      "firefoxview_next",
      "dismiss_closed_tab",
      "tabs",
      null,
      {
        position: position.toString(),
        delta: deltaSeconds.toString(),
        page: this.recentBrowsing ? "recentbrowsing" : "recentlyclosed",
      }
    );
  }

  willUpdate() {
    this.fullyUpdated = false;
  }

  updated() {
    this.fullyUpdated = true;
  }

  emptyMessageTemplate() {
    let descriptionHeader;
    let descriptionLabels;
    let descriptionLink;
    if (Services.prefs.getBoolPref(NEVER_REMEMBER_HISTORY_PREF, false)) {
      // History pref set to never remember history
      descriptionHeader = "firefoxview-dont-remember-history-empty-header";
      descriptionLabels = [
        "firefoxview-dont-remember-history-empty-description",
        "firefoxview-dont-remember-history-empty-description-two",
      ];
      descriptionLink = {
        url: "about:preferences#privacy",
        name: "history-settings-url-two",
      };
    } else {
      descriptionHeader = "firefoxview-recentlyclosed-empty-header";
      descriptionLabels = [
        "firefoxview-recentlyclosed-empty-description",
        "firefoxview-recentlyclosed-empty-description-two",
      ];
      descriptionLink = {
        url: "about:firefoxview-next#history",
        name: "history-url",
        sameTarget: "true",
      };
    }
    return html`
      <fxview-empty-state
        headerLabel=${descriptionHeader}
        .descriptionLabels=${descriptionLabels}
        .descriptionLink=${descriptionLink}
        class="empty-state recentlyclosed"
        ?isInnerCard=${this.recentBrowsing}
        ?isSelectedTab=${this.selectedTab}
        mainImageUrl="chrome://browser/content/firefoxview/recentlyclosed-empty.svg"
      >
      </fxview-empty-state>
    `;
  }

  render() {
    if (!this.selectedTab && !this.recentBrowsing) {
      return null;
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview-next.css"
      />
      <div class="sticky-container bottom-fade" ?hidden=${!this.selectedTab}>
        <h2
          class="page-header heading-large"
          data-l10n-id="firefoxview-recently-closed-header"
        ></h2>
      </div>
      <div class=${classMap({ "cards-container": this.selectedTab })}>
        <card-container
          shortPageName=${this.recentBrowsing ? "recentlyclosed" : null}
          ?showViewAll=${this.recentBrowsing && this.recentlyClosedTabs.length}
          ?preserveCollapseState=${this.recentBrowsing ? true : null}
          ?hideHeader=${this.selectedTab}
          ?hidden=${!this.recentlyClosedTabs.length && !this.recentBrowsing}
        >
          <h3
            slot="header"
            data-l10n-id="firefoxview-recently-closed-header"
          ></h3>
          <fxview-tab-list
            class="with-dismiss-button"
            ?hidden=${!this.recentlyClosedTabs.length}
            slot="main"
            .maxTabsLength=${this.maxTabsLength}
            .tabItems=${this.recentlyClosedTabs}
            @fxview-tab-list-secondary-action=${this.onDismissTab}
            @fxview-tab-list-primary-action=${this.onReopenTab}
          ></fxview-tab-list>
          ${this.recentBrowsing
            ? html`
                <div slot="main" ?hidden=${this.recentlyClosedTabs.length}>
                  ${this.emptyMessageTemplate()}
                </div>
              `
            : ""}
        </card-container>
        ${this.selectedTab
          ? html`
              <div ?hidden=${this.recentlyClosedTabs.length}>
                ${this.emptyMessageTemplate()}
              </div>
            `
          : ""}
      </div>
    `;
  }
}
customElements.define("view-recentlyclosed", RecentlyClosedTabsInView);
