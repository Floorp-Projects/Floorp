/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  SyncedTabsController: "resource:///modules/SyncedTabsController.sys.mjs",
});

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

import {
  html,
  ifDefined,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";
import {
  escapeHtmlEntities,
  MAX_TABS_FOR_RECENT_BROWSING,
  navigateToLink,
} from "./helpers.mjs";

const UI_OPEN_STATE = "browser.tabs.firefox-view.ui-state.tab-pickup.open";

class SyncedTabsInView extends ViewPage {
  controller = new lazy.SyncedTabsController(this, {
    contextMenu: true,
    pairDeviceCallback: () =>
      Services.telemetry.recordEvent(
        "firefoxview_next",
        "fxa_mobile",
        "sync",
        null,
        {
          has_devices: TabsSetupFlowManager.secondaryDeviceConnected.toString(),
        }
      ),
    signupCallback: () =>
      Services.telemetry.recordEvent(
        "firefoxview_next",
        "fxa_continue",
        "sync",
        null
      ),
  });

  constructor() {
    super();
    this._started = false;
    this._id = Math.floor(Math.random() * 10e6);
    if (this.recentBrowsing) {
      this.maxTabsLength = MAX_TABS_FOR_RECENT_BROWSING;
    } else {
      // Setting maxTabsLength to -1 for no max
      this.maxTabsLength = -1;
    }
    this.fullyUpdated = false;
    this.showAll = false;
    this.cumulativeSearches = 0;
    this.onSearchQuery = this.onSearchQuery.bind(this);
  }

  static properties = {
    ...ViewPage.properties,
    showAll: { type: Boolean },
    cumulativeSearches: { type: Number },
  };

  static queries = {
    cardEls: { all: "card-container" },
    emptyState: "fxview-empty-state",
    searchTextbox: "fxview-search-textbox",
    tabLists: { all: "fxview-tab-list" },
  };

  start() {
    if (this._started) {
      return;
    }
    this._started = true;
    this.controller.addSyncObservers();
    this.controller.updateStates();
    this.onVisibilityChange();

    if (this.recentBrowsing) {
      this.recentBrowsingElement.addEventListener(
        "fxview-search-textbox-query",
        this.onSearchQuery
      );
    }
  }

  stop() {
    if (!this._started) {
      return;
    }
    this._started = false;
    TabsSetupFlowManager.updateViewVisibility(this._id, "unloaded");
    this.onVisibilityChange();
    this.controller.removeSyncObservers();

    if (this.recentBrowsing) {
      this.recentBrowsingElement.removeEventListener(
        "fxview-search-textbox-query",
        this.onSearchQuery
      );
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.stop();
  }

  viewVisibleCallback() {
    this.start();
  }

  viewHiddenCallback() {
    this.stop();
  }

  onVisibilityChange() {
    const isOpen = this.open;
    const isVisible = this.isVisible;
    if (isVisible && isOpen) {
      this.update();
      TabsSetupFlowManager.updateViewVisibility(this._id, "visible");
    } else {
      TabsSetupFlowManager.updateViewVisibility(
        this._id,
        isVisible ? "closed" : "hidden"
      );
    }

    this.toggleVisibilityInCardContainer();
  }

  generateMessageCard({
    action,
    buttonLabel,
    descriptionArray,
    descriptionLink,
    error,
    header,
    headerIconUrl,
    mainImageUrl,
  }) {
    return html`
      <fxview-empty-state
        headerLabel=${header}
        .descriptionLabels=${descriptionArray}
        .descriptionLink=${ifDefined(descriptionLink)}
        class="empty-state synced-tabs error"
        ?isSelectedTab=${this.selectedTab}
        ?isInnerCard=${this.recentBrowsing}
        mainImageUrl="${ifDefined(mainImageUrl)}"
        ?errorGrayscale=${error}
        headerIconUrl="${ifDefined(headerIconUrl)}"
        id="empty-container"
      >
        <button
          class="primary"
          slot="primary-action"
          ?hidden=${!buttonLabel}
          data-l10n-id="${ifDefined(buttonLabel)}"
          data-action="${action}"
          @click=${e => this.controller.handleEvent(e)}
        ></button>
      </fxview-empty-state>
    `;
  }

  onOpenLink(event) {
    navigateToLink(event);

    Services.telemetry.recordEvent(
      "firefoxview_next",
      "synced_tabs",
      "tabs",
      null,
      {
        page: this.recentBrowsing ? "recentbrowsing" : "syncedtabs",
      }
    );

    if (this.controller.searchQuery) {
      const searchesHistogram = Services.telemetry.getKeyedHistogramById(
        "FIREFOX_VIEW_CUMULATIVE_SEARCHES"
      );
      searchesHistogram.add(
        this.recentBrowsing ? "recentbrowsing" : "syncedtabs",
        this.cumulativeSearches
      );
      this.cumulativeSearches = 0;
    }
  }

  onContextMenu(e) {
    this.triggerNode = e.originalTarget;
    e.target.querySelector("panel-list").toggle(e.detail.originalEvent);
  }

  panelListTemplate() {
    return html`
      <panel-list slot="menu" data-tab-type="syncedtabs">
        <panel-item
          @click=${this.openInNewWindow}
          data-l10n-id="fxviewtabrow-open-in-window"
          data-l10n-attrs="accesskey"
        ></panel-item>
        <panel-item
          @click=${this.openInNewPrivateWindow}
          data-l10n-id="fxviewtabrow-open-in-private-window"
          data-l10n-attrs="accesskey"
        ></panel-item>
        <hr />
        <panel-item
          @click=${this.copyLink}
          data-l10n-id="fxviewtabrow-copy-link"
          data-l10n-attrs="accesskey"
        ></panel-item>
      </panel-list>
    `;
  }

  noDeviceTabsTemplate(deviceName, deviceType, isSearchResultsEmpty = false) {
    const template = html`<h3
        slot=${ifDefined(this.recentBrowsing ? null : "header")}
        class="device-header"
      >
        <span class="icon ${deviceType}" role="presentation"></span>
        ${deviceName}
      </h3>
      ${when(
        isSearchResultsEmpty,
        () => html`
          <div
            slot=${ifDefined(this.recentBrowsing ? null : "main")}
            class="blackbox notabs search-results-empty"
            data-l10n-id="firefoxview-search-results-empty"
            data-l10n-args=${JSON.stringify({
              query: escapeHtmlEntities(this.controller.searchQuery),
            })}
          ></div>
        `,
        () => html`
          <div
            slot=${ifDefined(this.recentBrowsing ? null : "main")}
            class="blackbox notabs"
            data-l10n-id="firefoxview-syncedtabs-device-notabs"
          ></div>
        `
      )}`;
    return this.recentBrowsing
      ? template
      : html`<card-container
          shortPageName=${this.recentBrowsing ? "syncedtabs" : null}
          >${template}</card-container
        >`;
  }

  onSearchQuery(e) {
    this.controller.searchQuery = e.detail.query;
    this.cumulativeSearches = e.detail.query ? this.cumulativeSearches + 1 : 0;
    this.showAll = false;
  }

  deviceTemplate(deviceName, deviceType, tabItems) {
    return html`<h3
        slot=${!this.recentBrowsing ? "header" : null}
        class="device-header"
      >
        <span class="icon ${deviceType}" role="presentation"></span>
        ${deviceName}
      </h3>
      <fxview-tab-list
        slot="main"
        secondaryActionClass="options-button"
        hasPopup="menu"
        .tabItems=${ifDefined(tabItems)}
        .searchQuery=${this.controller.searchQuery}
        maxTabsLength=${this.showAll ? -1 : this.maxTabsLength}
        @fxview-tab-list-primary-action=${this.onOpenLink}
        @fxview-tab-list-secondary-action=${this.onContextMenu}
        secondaryActionClass="options-button"
      >
        ${this.panelListTemplate()}
      </fxview-tab-list>`;
  }

  generateTabList() {
    let renderArray = [];
    let renderInfo = this.controller.getRenderInfo();
    for (let id in renderInfo) {
      let tabItems = renderInfo[id].tabItems;
      if (tabItems.length) {
        const template = this.recentBrowsing
          ? this.deviceTemplate(
              renderInfo[id].name,
              renderInfo[id].deviceType,
              tabItems
            )
          : html`<card-container
              shortPageName=${this.recentBrowsing ? "syncedtabs" : null}
              >${this.deviceTemplate(
                renderInfo[id].name,
                renderInfo[id].deviceType,
                tabItems
              )}
            </card-container>`;
        renderArray.push(template);
        if (this.isShowAllLinkVisible(tabItems)) {
          renderArray.push(html` <div class="show-all-link-container">
            <div
              class="show-all-link"
              @click=${this.enableShowAll}
              @keydown=${this.enableShowAll}
              data-l10n-id="firefoxview-show-all"
              tabindex="0"
              role="link"
            ></div>
          </div>`);
        }
      } else {
        // Check renderInfo[id].tabs.length to determine whether to display an
        // empty tab list message or empty search results message.
        // If there are no synced tabs, we always display the empty tab list
        // message, even if there is an active search query.
        renderArray.push(
          this.noDeviceTabsTemplate(
            renderInfo[id].name,
            renderInfo[id].deviceType,
            Boolean(renderInfo[id].tabs.length)
          )
        );
      }
    }
    return renderArray;
  }

  isShowAllLinkVisible(tabItems) {
    return (
      this.recentBrowsing &&
      this.controller.searchQuery &&
      tabItems.length > this.maxTabsLength &&
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
      Services.telemetry.recordEvent(
        "firefoxview_next",
        "search_show_all",
        "showallbutton",
        null,
        {
          section: "syncedtabs",
        }
      );
    }
  }

  generateCardContent() {
    const cardProperties = this.controller.getMessageCard();
    return cardProperties
      ? this.generateMessageCard(cardProperties)
      : this.generateTabList();
  }

  render() {
    this.open =
      !TabsSetupFlowManager.isTabSyncSetupComplete ||
      Services.prefs.getBoolPref(UI_OPEN_STATE, true);

    let renderArray = [];
    renderArray.push(html` <link
      rel="stylesheet"
      href="chrome://browser/content/firefoxview/view-syncedtabs.css"
    />`);
    renderArray.push(html` <link
      rel="stylesheet"
      href="chrome://browser/content/firefoxview/firefoxview.css"
    />`);

    if (!this.recentBrowsing) {
      renderArray.push(html`<div class="sticky-container bottom-fade">
        <h2
          class="page-header"
          data-l10n-id="firefoxview-synced-tabs-header"
        ></h2>
        <div class="syncedtabs-header">
          <div>
            <fxview-search-textbox
              data-l10n-id="firefoxview-search-text-box-syncedtabs"
              data-l10n-attrs="placeholder"
              @fxview-search-textbox-query=${this.onSearchQuery}
              .size=${this.searchTextboxSize}
              pageName=${this.recentBrowsing ? "recentbrowsing" : "syncedtabs"}
            ></fxview-search-textbox>
          </div>
          ${when(
            this.controller.currentSetupStateIndex === 4,
            () => html`
              <button
                class="small-button"
                data-action="add-device"
                @click=${e => this.controller.handleEvent(e)}
              >
                <img
                  class="icon"
                  role="presentation"
                  src="chrome://global/skin/icons/plus.svg"
                  alt="plus sign"
                /><span
                  data-l10n-id="firefoxview-syncedtabs-connect-another-device"
                  data-action="add-device"
                ></span>
              </button>
            `
          )}
        </div>
      </div>`);
    }

    if (this.recentBrowsing) {
      renderArray.push(
        html`<card-container
          preserveCollapseState
          shortPageName="syncedtabs"
          ?showViewAll=${this.controller.currentSetupStateIndex == 4 &&
          this.controller.currentSyncedTabs.length}
          ?isEmptyState=${!this.controller.currentSyncedTabs.length}
        >
          >
          <h3
            slot="header"
            data-l10n-id="firefoxview-synced-tabs-header"
            class="recentbrowsing-header"
          ></h3>
          <div slot="main">${this.generateCardContent()}</div>
        </card-container>`
      );
    } else {
      renderArray.push(
        html`<div class="cards-container">${this.generateCardContent()}</div>`
      );
    }
    return renderArray;
  }

  updated() {
    this.fullyUpdated = true;
    this.toggleVisibilityInCardContainer();
  }
}
customElements.define("view-syncedtabs", SyncedTabsInView);
