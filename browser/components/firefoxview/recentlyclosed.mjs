/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  classMap,
  html,
  ifDefined,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import {
  isSearchEnabled,
  searchTabList,
  MAX_TABS_FOR_RECENT_BROWSING,
} from "./helpers.mjs";
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
    this._started = false;
    this.boundObserve = (...args) => this.observe(...args);
    this.firstUpdateComplete = false;
    this.fullyUpdated = false;
    this.maxTabsLength = this.recentBrowsing
      ? MAX_TABS_FOR_RECENT_BROWSING
      : -1;
    this.recentlyClosedTabs = [];
    this.searchQuery = "";
    this.searchResults = null;
    this.showAll = false;
  }

  static properties = {
    ...ViewPage.properties,
    searchResults: { type: Array },
    showAll: { type: Boolean },
  };

  static queries = {
    cardEl: "card-container",
    emptyState: "fxview-empty-state",
    searchTextbox: "fxview-search-textbox",
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

  start() {
    if (this._started) {
      return;
    }
    this._started = true;
    this.paused = false;
    this.updateRecentlyClosedTabs();

    Services.obs.addObserver(
      this.boundObserve,
      SS_NOTIFY_CLOSED_OBJECTS_CHANGED
    );
    Services.obs.addObserver(
      this.boundObserve,
      SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH
    );

    if (this.recentBrowsing) {
      this.recentBrowsingElement.addEventListener(
        "fxview-search-textbox-query",
        this
      );
    }

    this.toggleVisibilityInCardContainer();
  }

  stop() {
    if (!this._started) {
      return;
    }
    this._started = false;

    Services.obs.removeObserver(
      this.boundObserve,
      SS_NOTIFY_CLOSED_OBJECTS_CHANGED
    );
    Services.obs.removeObserver(
      this.boundObserve,
      SS_NOTIFY_BROWSER_SHUTDOWN_FLUSH
    );

    if (this.recentBrowsing) {
      this.recentBrowsingElement.removeEventListener(
        "fxview-search-textbox-query",
        this
      );
    }

    this.toggleVisibilityInCardContainer();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.stop();
  }

  handleEvent(event) {
    if (this.recentBrowsing && event.type === "fxview-search-textbox-query") {
      this.onSearchQuery(event);
    }
  }

  // We remove all the observers when the instance is not visible to the user
  viewHiddenCallback() {
    this.stop();
  }

  // We add observers and check for changes to the session store once the user return to this tab.
  // or the instance becomes visible to the user
  viewVisibleCallback() {
    this.start();
  }

  firstUpdated() {
    this.firstUpdateComplete = true;
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
    this.recentlyClosedTabs = recentlyClosedTabsData;
    this.normalizeRecentlyClosedData();
    if (this.searchQuery) {
      this.#updateSearchResults();
    }
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
    this.toggleVisibilityInCardContainer();
  }

  async scheduleUpdate() {
    // Only defer initial update
    if (!this.firstUpdateComplete) {
      await new Promise(resolve => setTimeout(resolve));
    }
    super.scheduleUpdate();
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
        url: "about:firefoxview#history",
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
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview.css"
      />
      ${when(
        !this.recentBrowsing,
        () => html`<div
          class="sticky-container bottom-fade"
          ?hidden=${!this.selectedTab}
        >
          <h2
            class="page-header heading-large"
            data-l10n-id="firefoxview-recently-closed-header"
          ></h2>
          ${when(
            isSearchEnabled(),
            () => html`<div>
              <fxview-search-textbox
                .query=${this.searchQuery}
                data-l10n-id="firefoxview-search-text-box-recentlyclosed"
                data-l10n-attrs="placeholder"
                @fxview-search-textbox-query=${this.onSearchQuery}
                .size=${this.searchTextboxSize}
              ></fxview-search-textbox>
            </div>`
          )}
        </div>`
      )}
      <div class=${classMap({ "cards-container": this.selectedTab })}>
        <card-container
          shortPageName=${this.recentBrowsing ? "recentlyclosed" : null}
          ?showViewAll=${this.recentBrowsing && this.recentlyClosedTabs.length}
          ?preserveCollapseState=${this.recentBrowsing ? true : null}
          ?hideHeader=${this.selectedTab}
          ?hidden=${!this.recentlyClosedTabs.length && !this.recentBrowsing}
          ?isEmptyState=${!this.recentlyClosedTabs.length}
        >
          <h3
            slot="header"
            data-l10n-id="firefoxview-recently-closed-header"
          ></h3>
          ${when(
            this.recentlyClosedTabs.length,
            () =>
              html`
                <fxview-tab-list
                  class="with-dismiss-button"
                  slot="main"
                  .maxTabsLength=${!this.recentBrowsing || this.showAll
                    ? -1
                    : MAX_TABS_FOR_RECENT_BROWSING}
                  .searchQuery=${ifDefined(
                    this.searchResults && this.searchQuery
                  )}
                  .tabItems=${this.searchResults || this.recentlyClosedTabs}
                  @fxview-tab-list-secondary-action=${this.onDismissTab}
                  @fxview-tab-list-primary-action=${this.onReopenTab}
                ></fxview-tab-list>
              `
          )}
          ${when(
            this.recentBrowsing && !this.recentlyClosedTabs.length,
            () => html` <div slot="main">${this.emptyMessageTemplate()}</div> `
          )}
          ${when(
            this.isShowAllLinkVisible(),
            () => html` <div
              @click=${this.enableShowAll}
              @keydown=${this.enableShowAll}
              data-l10n-id="firefoxview-show-all"
              ?hidden=${!this.isShowAllLinkVisible()}
              slot="footer"
              tabindex="0"
              role="link"
            ></div>`
          )}
        </card-container>
        ${when(
          this.selectedTab && !this.recentlyClosedTabs.length,
          () => html` <div>${this.emptyMessageTemplate()}</div> `
        )}
      </div>
    `;
  }

  onSearchQuery(e) {
    this.searchQuery = e.detail.query;
    this.showAll = false;
    this.#updateSearchResults();
  }

  #updateSearchResults() {
    this.searchResults = this.searchQuery
      ? searchTabList(this.searchQuery, this.recentlyClosedTabs)
      : null;
  }

  isShowAllLinkVisible() {
    return (
      this.recentBrowsing &&
      this.searchQuery &&
      this.searchResults.length > MAX_TABS_FOR_RECENT_BROWSING &&
      !this.showAll
    );
  }

  enableShowAll(event) {
    if (
      event.type == "click" ||
      (event.type == "keydown" && event.code == "Enter") ||
      (event.type == "keydown" && event.code == "Space")
    ) {
      event.preventDefault();
      this.showAll = true;
    }
  }
}
customElements.define("view-recentlyclosed", RecentlyClosedTabsInView);
