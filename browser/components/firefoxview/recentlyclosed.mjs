/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
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

function getWindow() {
  return window.browsingContext.embedderWindowGlobal.browsingContext.window;
}

class RecentlyClosedTabsInView extends ViewPage {
  constructor() {
    super();
    this.boundObserve = (...args) => this.observe(...args);
    this.maxTabsLength = this.overview ? 5 : 25;
    this.recentlyClosedTabs = [];
  }

  static queries = {
    cardEl: "card-container",
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
    getWindow().gBrowser.tabContainer.addEventListener("TabSelect", this);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
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
    if (contentDocument?.URL.split("#")[0] == "about:firefoxview-next") {
      this.addObserversIfNeeded();
      this.updateRecentlyClosedTabs();
    } else {
      this.removeObserversIfNeeded();
    }
  }

  handleEvent(event) {
    if (event.type == "TabSelect") {
      this.handleObservers(event.target.linkedBrowser.contentDocument);
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

  updateRecentlyClosedTabs() {
    let recentlyClosedTabsData = lazy.SessionStore.getClosedTabDataForWindow(
      getWindow()
    );
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
    lazy.SessionStore.undoCloseById(closedId);
  }

  onDismissTab(e) {
    let recentlyClosedList = lazy.SessionStore.getClosedTabDataForWindow(
      getWindow()
    );
    let closedTabIndex = recentlyClosedList.findIndex(closedTab => {
      return closedTab.closedId === parseInt(e.originalTarget.closedId, 10);
    });
    if (closedTabIndex < 0) {
      // Tab not found in recently closed list
      return;
    }
    lazy.SessionStore.forgetClosedTab(getWindow(), closedTabIndex);
  }

  render() {
    if (!this.selectedTab && !this.overview) {
      return null;
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview-next.css"
      />
      <div class="sticky-container bottom-fade" ?hidden=${!this.selectedTab}>
        <h2
          class="page-header"
          data-l10n-id="firefoxview-recently-closed-header"
        ></h2>
      </div>
      <div class="cards-container">
        <card-container
          .viewAllPage=${this.overview && this.recentlyClosedTabs.length
            ? "recentlyclosed"
            : null}
          ?preserveCollapseState=${this.overview ? true : null}
          ?hideHeader=${this.selectedTab}
        >
          <h2
            slot="header"
            data-l10n-id="firefoxview-recently-closed-header"
          ></h2>
          <fxview-tab-list
            class="with-dismiss-button"
            ?hidden=${!this.recentlyClosedTabs.length}
            slot="main"
            .maxTabsLength=${this.maxTabsLength}
            .tabItems=${this.recentlyClosedTabs}
            @fxview-tab-list-secondary-action=${this.onDismissTab}
            @fxview-tab-list-primary-action=${this.onReopenTab}
          ></fxview-tab-list>
          <div slot="main" ?hidden=${this.recentlyClosedTabs.length}>
            <!-- TO-DO: Bug 1841795 - Add Recently Closed empty states -->
          </div>
          <div
            slot="footer"
            name="history"
            ?hidden=${!this.selectedTab || !this.recentlyClosedTabs.length}
          >
            <a
              href="about:firefoxview-next#history"
              data-l10n-id="firefoxview-view-more-browsing-history"
            ></a>
          </div>
        </card-container>
      </div>
    `;
  }
}
customElements.define("view-recentlyclosed", RecentlyClosedTabsInView);
