/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  classMap,
  html,
  map,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import {
  getLogger,
  isSearchEnabled,
  placeLinkOnClipboard,
  searchTabList,
  MAX_TABS_FOR_RECENT_BROWSING,
} from "./helpers.mjs";
import { ViewPage, ViewPageContent } from "./viewpage.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/opentabs-tab-list.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BookmarkList: "resource://gre/modules/BookmarkList.sys.mjs",
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  NonPrivateTabs: "resource:///modules/OpenTabs.sys.mjs",
  getTabsTargetForWindow: "resource:///modules/OpenTabs.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

/**
 * A collection of open tabs grouped by window.
 *
 * @property {Array<Window>} windows
 *   A list of windows with the same privateness
 * @property {string} sortOption
 *   The sorting order of open tabs:
 *   - "recency": Sorted by recent activity. (For recent browsing, this is the only option.)
 *   - "tabStripOrder": Match the order in which they appear on the tab strip.
 */
class OpenTabsInView extends ViewPage {
  static properties = {
    ...ViewPage.properties,
    windows: { type: Array },
    searchQuery: { type: String },
    sortOption: { type: String },
  };
  static queries = {
    viewCards: { all: "view-opentabs-card" },
    optionsContainer: ".open-tabs-options",
    searchTextbox: "fxview-search-textbox",
  };

  initialWindowsReady = false;
  currentWindow = null;
  openTabsTarget = null;

  constructor() {
    super();
    this._started = false;
    this.windows = [];
    this.currentWindow = this.getWindow();
    if (lazy.PrivateBrowsingUtils.isWindowPrivate(this.currentWindow)) {
      this.openTabsTarget = lazy.getTabsTargetForWindow(this.currentWindow);
    } else {
      this.openTabsTarget = lazy.NonPrivateTabs;
    }
    this.searchQuery = "";
    this.sortOption = this.recentBrowsing
      ? "recency"
      : Services.prefs.getStringPref(
          "browser.tabs.firefox-view.ui-state.opentabs.sort-option",
          "recency"
        );
  }

  start() {
    if (this._started) {
      return;
    }
    this._started = true;
    this.#setupTabChangeListener();

    // To resolve the race between this component wanting to render all the windows'
    // tabs, while those windows are still potentially opening, flip this property
    // once the promise resolves and we'll bail out of rendering until then.
    this.openTabsTarget.readyWindowsPromise.finally(() => {
      this.initialWindowsReady = true;
      this._updateWindowList();
    });

    for (let card of this.viewCards) {
      card.paused = false;
      card.viewVisibleCallback?.();
    }

    if (this.recentBrowsing) {
      this.recentBrowsingElement.addEventListener(
        "fxview-search-textbox-query",
        this
      );
    }

    this.bookmarkList = new lazy.BookmarkList(this.#getAllTabUrls(), () =>
      this.viewCards.forEach(card => card.requestUpdate())
    );
  }

  shouldUpdate(changedProperties) {
    if (!this.initialWindowsReady) {
      return false;
    }
    return super.shouldUpdate(changedProperties);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.stop();
  }

  stop() {
    if (!this._started) {
      return;
    }
    this._started = false;
    this.paused = true;

    this.openTabsTarget.removeEventListener("TabChange", this);
    this.openTabsTarget.removeEventListener("TabRecencyChange", this);

    for (let card of this.viewCards) {
      card.paused = true;
      card.viewHiddenCallback?.();
    }

    if (this.recentBrowsing) {
      this.recentBrowsingElement.removeEventListener(
        "fxview-search-textbox-query",
        this
      );
    }

    this.bookmarkList.removeListeners();
  }

  viewVisibleCallback() {
    this.start();
  }

  viewHiddenCallback() {
    this.stop();
  }

  #setupTabChangeListener() {
    if (this.sortOption === "recency") {
      this.openTabsTarget.addEventListener("TabRecencyChange", this);
      this.openTabsTarget.removeEventListener("TabChange", this);
    } else {
      this.openTabsTarget.removeEventListener("TabRecencyChange", this);
      this.openTabsTarget.addEventListener("TabChange", this);
    }
  }

  #getAllTabUrls() {
    return this.openTabsTarget
      .getAllTabs()
      .map(({ linkedBrowser }) => linkedBrowser?.currentURI?.spec)
      .filter(Boolean);
  }

  render() {
    if (this.recentBrowsing) {
      return this.getRecentBrowsingTemplate();
    }
    let currentWindowIndex, currentWindowTabs;
    let index = 1;
    const otherWindows = [];
    this.windows.forEach(win => {
      const tabs = this.openTabsTarget.getTabsForWindow(
        win,
        this.sortOption === "recency"
      );
      if (win === this.currentWindow) {
        currentWindowIndex = index++;
        currentWindowTabs = tabs;
      } else {
        otherWindows.push([index++, tabs, win]);
      }
    });

    const cardClasses = classMap({
      "height-limited": this.windows.length > 3,
      "width-limited": this.windows.length > 1,
    });
    let cardCount;
    if (this.windows.length <= 1) {
      cardCount = "one";
    } else if (this.windows.length === 2) {
      cardCount = "two";
    } else {
      cardCount = "three-or-more";
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/view-opentabs.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview.css"
      />
      <div class="sticky-container bottom-fade">
        <h2 class="page-header" data-l10n-id="firefoxview-opentabs-header"></h2>
        <div class="open-tabs-options">
          ${when(
            isSearchEnabled(),
            () => html`<div>
              <fxview-search-textbox
                data-l10n-id="firefoxview-search-text-box-opentabs"
                data-l10n-attrs="placeholder"
                @fxview-search-textbox-query=${this.onSearchQuery}
                .size=${this.searchTextboxSize}
                pageName=${this.recentBrowsing ? "recentbrowsing" : "opentabs"}
              ></fxview-search-textbox>
            </div>`
          )}
          <div class="open-tabs-sort-wrapper">
            <div class="open-tabs-sort-option">
              <input
                type="radio"
                id="sort-by-recency"
                name="open-tabs-sort-option"
                value="recency"
                ?checked=${this.sortOption === "recency"}
                @click=${this.onChangeSortOption}
              />
              <label
                for="sort-by-recency"
                data-l10n-id="firefoxview-sort-open-tabs-by-recency-label"
              ></label>
            </div>
            <div class="open-tabs-sort-option">
              <input
                type="radio"
                id="sort-by-order"
                name="open-tabs-sort-option"
                value="tabStripOrder"
                ?checked=${this.sortOption === "tabStripOrder"}
                @click=${this.onChangeSortOption}
              />
              <label
                for="sort-by-order"
                data-l10n-id="firefoxview-sort-open-tabs-by-order-label"
              ></label>
            </div>
          </div>
        </div>
      </div>
      <div
        card-count=${cardCount}
        class="view-opentabs-card-container cards-container"
      >
        ${when(
          currentWindowIndex && currentWindowTabs,
          () =>
            html`
              <view-opentabs-card
                class=${cardClasses}
                .tabs=${currentWindowTabs}
                .paused=${this.paused}
                data-inner-id="${this.currentWindow.windowGlobalChild
                  .innerWindowId}"
                data-l10n-id="firefoxview-opentabs-current-window-header"
                data-l10n-args="${JSON.stringify({
                  winID: currentWindowIndex,
                })}"
                .searchQuery=${this.searchQuery}
                .bookmarkList=${this.bookmarkList}
              ></view-opentabs-card>
            `
        )}
        ${map(
          otherWindows,
          ([winID, tabs, win]) => html`
            <view-opentabs-card
              class=${cardClasses}
              .tabs=${tabs}
              .paused=${this.paused}
              data-inner-id="${win.windowGlobalChild.innerWindowId}"
              data-l10n-id="firefoxview-opentabs-window-header"
              data-l10n-args="${JSON.stringify({ winID })}"
              .searchQuery=${this.searchQuery}
              .bookmarkList=${this.bookmarkList}
            ></view-opentabs-card>
          `
        )}
      </div>
    `;
  }

  onSearchQuery(e) {
    this.searchQuery = e.detail.query;
  }

  onChangeSortOption(e) {
    this.sortOption = e.target.value;
    this.#setupTabChangeListener();
    if (!this.recentBrowsing) {
      Services.prefs.setStringPref(
        "browser.tabs.firefox-view.ui-state.opentabs.sort-option",
        this.sortOption
      );
    }
  }

  /**
   * Render a template for the 'Recent browsing' page, which shows a shorter list of
   * open tabs in the current window.
   *
   * @returns {TemplateResult}
   *   The recent browsing template.
   */
  getRecentBrowsingTemplate() {
    const tabs = this.openTabsTarget.getRecentTabs();
    return html`<view-opentabs-card
      .tabs=${tabs}
      .recentBrowsing=${true}
      .paused=${this.paused}
      .searchQuery=${this.searchQuery}
      .bookmarkList=${this.bookmarkList}
    ></view-opentabs-card>`;
  }

  handleEvent({ detail, type }) {
    if (this.recentBrowsing && type === "fxview-search-textbox-query") {
      this.onSearchQuery({ detail });
      return;
    }
    let windowIds;
    switch (type) {
      case "TabRecencyChange":
      case "TabChange":
        windowIds = detail.windowIds;
        this._updateWindowList();
        this.bookmarkList.setTrackedUrls(this.#getAllTabUrls());
        break;
    }
    if (this.recentBrowsing) {
      return;
    }
    if (windowIds?.length) {
      // there were tab changes to one or more windows
      for (let winId of windowIds) {
        const cardForWin = this.shadowRoot.querySelector(
          `view-opentabs-card[data-inner-id="${winId}"]`
        );
        if (this.searchQuery) {
          cardForWin?.updateSearchResults();
        }
        cardForWin?.requestUpdate();
      }
    } else {
      let winId = window.windowGlobalChild.innerWindowId;
      let cardForWin = this.shadowRoot.querySelector(
        `view-opentabs-card[data-inner-id="${winId}"]`
      );
      if (this.searchQuery) {
        cardForWin?.updateSearchResults();
      }
    }
  }

  async _updateWindowList() {
    this.windows = this.openTabsTarget.currentWindows;
  }
}
customElements.define("view-opentabs", OpenTabsInView);

/**
 * A card which displays a list of open tabs for a window.
 *
 * @property {boolean} showMore
 *   Whether to force all tabs to be shown, regardless of available space.
 * @property {MozTabbrowserTab[]} tabs
 *   The open tabs to show.
 * @property {string} title
 *   The window title.
 */
class OpenTabsInViewCard extends ViewPageContent {
  static properties = {
    showMore: { type: Boolean },
    tabs: { type: Array },
    title: { type: String },
    recentBrowsing: { type: Boolean },
    searchQuery: { type: String },
    searchResults: { type: Array },
    showAll: { type: Boolean },
    cumulativeSearches: { type: Number },
    bookmarkList: { type: Object },
  };
  static MAX_TABS_FOR_COMPACT_HEIGHT = 7;

  constructor() {
    super();
    this.showMore = false;
    this.tabs = [];
    this.title = "";
    this.recentBrowsing = false;
    this.devices = [];
    this.searchQuery = "";
    this.searchResults = null;
    this.showAll = false;
    this.cumulativeSearches = 0;
  }

  static queries = {
    cardEl: "card-container",
    tabContextMenu: "view-opentabs-contextmenu",
    tabList: "opentabs-tab-list",
  };

  openContextMenu(e) {
    let { originalEvent } = e.detail;
    this.tabContextMenu.toggle({
      triggerNode: e.originalTarget,
      originalEvent,
    });
  }

  getMaxTabsLength() {
    if (this.recentBrowsing && !this.showAll) {
      return MAX_TABS_FOR_RECENT_BROWSING;
    } else if (this.classList.contains("height-limited") && !this.showMore) {
      return OpenTabsInViewCard.MAX_TABS_FOR_COMPACT_HEIGHT;
    }
    return -1;
  }

  isShowAllLinkVisible() {
    return (
      this.recentBrowsing &&
      this.searchQuery &&
      this.searchResults.length > MAX_TABS_FOR_RECENT_BROWSING &&
      !this.showAll
    );
  }

  toggleShowMore(event) {
    if (
      event.type == "click" ||
      (event.type == "keydown" && event.code == "Enter") ||
      (event.type == "keydown" && event.code == "Space")
    ) {
      event.preventDefault();
      this.showMore = !this.showMore;
    }
  }

  enableShowAll(event) {
    if (
      event.type == "click" ||
      (event.type == "keydown" && event.code == "Enter") ||
      (event.type == "keydown" && event.code == "Space")
    ) {
      event.preventDefault();
      Services.telemetry.recordEvent(
        "firefoxview_next",
        "search_show_all",
        "showallbutton",
        null,
        {
          section: "opentabs",
        }
      );
      this.showAll = true;
    }
  }

  onTabListRowClick(event) {
    // Don't open pinned tab if mute/unmute indicator button selected
    if (
      Array.from(event.explicitOriginalTarget.classList).includes(
        "fxview-tab-row-pinned-media-button"
      )
    ) {
      return;
    }
    const tab = event.originalTarget.tabElement;
    const browserWindow = tab.ownerGlobal;
    browserWindow.focus();
    browserWindow.gBrowser.selectedTab = tab;

    Services.telemetry.recordEvent(
      "firefoxview_next",
      "open_tab",
      "tabs",
      null,
      {
        page: this.recentBrowsing ? "recentbrowsing" : "opentabs",
        window: this.title || "Window 1 (Current)",
      }
    );
    if (this.searchQuery) {
      const searchesHistogram = Services.telemetry.getKeyedHistogramById(
        "FIREFOX_VIEW_CUMULATIVE_SEARCHES"
      );
      searchesHistogram.add(
        this.recentBrowsing ? "recentbrowsing" : "opentabs",
        this.cumulativeSearches
      );
      this.cumulativeSearches = 0;
    }
  }

  closeTab(event) {
    const tab = event.originalTarget.tabElement;
    tab?.ownerGlobal.gBrowser.removeTab(tab);

    Services.telemetry.recordEvent(
      "firefoxview_next",
      "close_open_tab",
      "tabs",
      null
    );
  }

  viewVisibleCallback() {
    this.getRootNode().host.toggleVisibilityInCardContainer(true);
  }

  viewHiddenCallback() {
    this.getRootNode().host.toggleVisibilityInCardContainer(true);
  }

  firstUpdated() {
    this.getRootNode().host.toggleVisibilityInCardContainer(true);
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview.css"
      />
      <card-container
        ?preserveCollapseState=${this.recentBrowsing}
        shortPageName=${this.recentBrowsing ? "opentabs" : null}
        ?showViewAll=${this.recentBrowsing}
        ?removeBlockEndMargin=${!this.recentBrowsing}
      >
        ${when(
          this.recentBrowsing,
          () => html`<h3
            slot="header"
            data-l10n-id="firefoxview-opentabs-header"
          ></h3>`,
          () => html`<h3 slot="header">${this.title}</h3>`
        )}
        <div class="fxview-tab-list-container" slot="main">
          <opentabs-tab-list
            .hasPopup=${"menu"}
            ?compactRows=${this.classList.contains("width-limited")}
            @fxview-tab-list-primary-action=${this.onTabListRowClick}
            @fxview-tab-list-secondary-action=${this.openContextMenu}
            @fxview-tab-list-tertiary-action=${this.closeTab}
            secondaryActionClass="options-button"
            tertiaryActionClass="dismiss-button"
            .maxTabsLength=${this.getMaxTabsLength()}
            .tabItems=${this.searchResults ||
            getTabListItems(this.tabs, this.recentBrowsing)}
            .searchQuery=${this.searchQuery}
            .pinnedTabsGridView=${!this.recentBrowsing}
            ><view-opentabs-contextmenu slot="menu"></view-opentabs-contextmenu>
          </opentabs-tab-list>
        </div>
        ${when(
          this.recentBrowsing,
          () => html` <div
            @click=${this.enableShowAll}
            @keydown=${this.enableShowAll}
            data-l10n-id="firefoxview-show-all"
            ?hidden=${!this.isShowAllLinkVisible()}
            slot="footer"
            tabindex="0"
            role="link"
          ></div>`,
          () =>
            html` <div
              @click=${this.toggleShowMore}
              @keydown=${this.toggleShowMore}
              data-l10n-id="${this.showMore
                ? "firefoxview-show-less"
                : "firefoxview-show-more"}"
              ?hidden=${!this.classList.contains("height-limited") ||
              this.tabs.length <=
                OpenTabsInViewCard.MAX_TABS_FOR_COMPACT_HEIGHT}
              slot="footer"
              tabindex="0"
              role="link"
            ></div>`
        )}
      </card-container>
    `;
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("searchQuery")) {
      this.showAll = false;
      this.cumulativeSearches = this.searchQuery
        ? this.cumulativeSearches + 1
        : 0;
    }
    if (changedProperties.has("searchQuery") || changedProperties.has("tabs")) {
      this.updateSearchResults();
    }
  }

  updateSearchResults() {
    this.searchResults = this.searchQuery
      ? searchTabList(this.searchQuery, getTabListItems(this.tabs))
      : null;
  }

  updated() {
    this.updateBookmarkStars();
  }

  async updateBookmarkStars() {
    const tabItems = [...this.tabList.tabItems];
    for (const row of tabItems) {
      const isBookmark = await this.bookmarkList.isBookmark(row.url);
      if (isBookmark && !row.indicators.includes("bookmark")) {
        row.indicators.push("bookmark");
      }
      if (!isBookmark && row.indicators.includes("bookmark")) {
        row.indicators = row.indicators.filter(i => i !== "bookmark");
      }
      row.primaryL10nId = getPrimaryL10nId(
        this.isRecentBrowsing,
        row.indicators
      );
    }
    this.tabList.tabItems = tabItems;
  }
}
customElements.define("view-opentabs-card", OpenTabsInViewCard);

/**
 * A context menu of actions available for open tab list items.
 */
class OpenTabsContextMenu extends MozLitElement {
  static properties = {
    devices: { type: Array },
    triggerNode: { type: Object },
  };

  static queries = {
    panelList: "panel-list",
  };

  constructor() {
    super();
    this.triggerNode = null;
    this.devices = [];
  }

  get logger() {
    return getLogger("OpenTabsContextMenu");
  }

  get ownerViewPage() {
    return this.ownerDocument.querySelector("view-opentabs");
  }

  async fetchDevices() {
    const currentWindow = this.ownerViewPage.getWindow();
    if (currentWindow?.gSync) {
      try {
        await lazy.fxAccounts.device.refreshDeviceList();
      } catch (e) {
        this.logger.warn("Could not refresh the FxA device list", e);
      }
      this.devices = currentWindow.gSync.getSendTabTargets();
    }
  }

  async toggle({ triggerNode, originalEvent }) {
    if (this.panelList?.open) {
      // the menu will close so avoid all the other work to update its contents
      this.panelList.toggle(originalEvent);
      return;
    }
    this.triggerNode = triggerNode;
    await this.fetchDevices();
    await this.getUpdateComplete();
    this.panelList.toggle(originalEvent);
  }

  copyLink(e) {
    placeLinkOnClipboard(this.triggerNode.title, this.triggerNode.url);
    this.ownerViewPage.recordContextMenuTelemetry("copy-link", e);
  }

  closeTab(e) {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.removeTab(tab);
    this.ownerViewPage.recordContextMenuTelemetry("close-tab", e);
  }

  pinTab(e) {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.pinTab(tab);
    this.ownerViewPage.recordContextMenuTelemetry("pin-tab", e);
  }

  unpinTab(e) {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.unpinTab(tab);
    this.ownerViewPage.recordContextMenuTelemetry("unpin-tab", e);
  }

  toggleAudio(e) {
    const tab = this.triggerNode.tabElement;
    tab.toggleMuteAudio();
    this.ownerViewPage.recordContextMenuTelemetry(
      `${
        this.triggerNode.indicators.includes("muted") ? "unmute" : "mute"
      }-tab`,
      e
    );
  }

  moveTabsToStart(e) {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.moveTabsToStart(tab);
    this.ownerViewPage.recordContextMenuTelemetry("move-tab-start", e);
  }

  moveTabsToEnd(e) {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.moveTabsToEnd(tab);
    this.ownerViewPage.recordContextMenuTelemetry("move-tab-end", e);
  }

  moveTabsToWindow(e) {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.replaceTabsWithWindow(tab);
    this.ownerViewPage.recordContextMenuTelemetry("move-tab-window", e);
  }

  moveMenuTemplate() {
    const tab = this.triggerNode?.tabElement;
    if (!tab) {
      return null;
    }
    const browserWindow = tab.ownerGlobal;
    const tabs = browserWindow?.gBrowser.visibleTabs || [];
    const position = tabs.indexOf(tab);

    return html`
      <panel-list slot="submenu" id="move-tab-menu">
        ${position > 0
          ? html`<panel-item
              @click=${this.moveTabsToStart}
              data-l10n-id="fxviewtabrow-move-tab-start"
              data-l10n-attrs="accesskey"
            ></panel-item>`
          : null}
        ${position < tabs.length - 1
          ? html`<panel-item
              @click=${this.moveTabsToEnd}
              data-l10n-id="fxviewtabrow-move-tab-end"
              data-l10n-attrs="accesskey"
            ></panel-item>`
          : null}
        <panel-item
          @click=${this.moveTabsToWindow}
          data-l10n-id="fxviewtabrow-move-tab-window"
          data-l10n-attrs="accesskey"
        ></panel-item>
      </panel-list>
    `;
  }

  async sendTabToDevice(e) {
    let deviceId = e.target.getAttribute("device-id");
    let device = this.devices.find(dev => dev.id == deviceId);
    const viewPage = this.ownerViewPage;
    viewPage.recordContextMenuTelemetry("send-tab-device", e);

    if (device && this.triggerNode) {
      await viewPage
        .getWindow()
        .gSync.sendTabToDevice(
          this.triggerNode.url,
          [device],
          this.triggerNode.title
        );
    }
  }

  sendTabTemplate() {
    return html` <panel-list slot="submenu" id="send-tab-menu">
      ${this.devices.map(device => {
        return html`
          <panel-item @click=${this.sendTabToDevice} device-id=${device.id}
            >${device.name}</panel-item
          >
        `;
      })}
    </panel-list>`;
  }

  render() {
    const tab = this.triggerNode?.tabElement;
    if (!tab) {
      return null;
    }

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview.css"
      />
      <panel-list data-tab-type="opentabs">
        <panel-item
          data-l10n-id="fxviewtabrow-move-tab"
          data-l10n-attrs="accesskey"
          submenu="move-tab-menu"
          >${this.moveMenuTemplate()}</panel-item
        >
        <panel-item
          data-l10n-id=${tab.pinned
            ? "fxviewtabrow-unpin-tab"
            : "fxviewtabrow-pin-tab"}
          data-l10n-attrs="accesskey"
          @click=${tab.pinned ? this.unpinTab : this.pinTab}
        ></panel-item>
        <panel-item
          data-l10n-id=${tab.hasAttribute("muted")
            ? "fxviewtabrow-unmute-tab"
            : "fxviewtabrow-mute-tab"}
          data-l10n-attrs="accesskey"
          @click=${this.toggleAudio}
        ></panel-item>
        <hr />
        <panel-item
          data-l10n-id="fxviewtabrow-copy-link"
          data-l10n-attrs="accesskey"
          @click=${this.copyLink}
        ></panel-item>
        ${this.devices.length >= 1
          ? html`<panel-item
              data-l10n-id="fxviewtabrow-send-tab"
              data-l10n-attrs="accesskey"
              submenu="send-tab-menu"
              >${this.sendTabTemplate()}</panel-item
            >`
          : null}
      </panel-list>
    `;
  }
}
customElements.define("view-opentabs-contextmenu", OpenTabsContextMenu);

/**
 * Checks if a given tab is within a container (contextual identity)
 *
 * @param {MozTabbrowserTab[]} tab
 *   Tab to fetch container info on.
 * @returns {object[]}
 *   Container object.
 */
function getContainerObj(tab) {
  let userContextId = tab.getAttribute("usercontextid");
  let containerObj = null;
  if (userContextId) {
    containerObj =
      lazy.ContextualIdentityService.getPublicIdentityFromId(userContextId);
  }
  return containerObj;
}

/**
 * Gets an array of tab indicators (if any) when normalizing for fxview-tab-list
 *
 * @param {MozTabbrowserTab[]} tab
 *   Tab to fetch container info on.
 * @returns {Array[]}
 *  Array of named tab indicators
 */
function getIndicatorsForTab(tab) {
  const url = tab.linkedBrowser?.currentURI?.spec || "";
  let tabIndicators = [];
  let hasAttention =
    (tab.pinned &&
      (tab.hasAttribute("attention") || tab.hasAttribute("titlechanged"))) ||
    (!tab.pinned && tab.hasAttribute("attention"));

  if (tab.pinned) {
    tabIndicators.push("pinned");
  }
  if (getContainerObj(tab)) {
    tabIndicators.push("container");
  }
  if (hasAttention) {
    tabIndicators.push("attention");
  }
  if (tab.hasAttribute("soundplaying") && !tab.hasAttribute("muted")) {
    tabIndicators.push("soundplaying");
  }
  if (tab.hasAttribute("muted")) {
    tabIndicators.push("muted");
  }
  if (checkIfPinnedNewTab(url)) {
    tabIndicators.push("pinnedOnNewTab");
  }

  return tabIndicators;
}
/**
 * Gets the primary l10n id for a tab when normalizing for fxview-tab-list
 *
 * @param {boolean} isRecentBrowsing
 *   Whether the tabs are going to be displayed on the Recent Browsing page or not
 * @param {Array[]} tabIndicators
 *   Array of tab indicators for the given tab
 * @returns {string}
 *  L10n ID string
 */
function getPrimaryL10nId(isRecentBrowsing, tabIndicators) {
  let indicatorL10nId = null;
  if (!isRecentBrowsing) {
    if (
      tabIndicators?.includes("pinned") &&
      tabIndicators?.includes("bookmark")
    ) {
      indicatorL10nId = "firefoxview-opentabs-bookmarked-pinned-tab";
    } else if (tabIndicators?.includes("pinned")) {
      indicatorL10nId = "firefoxview-opentabs-pinned-tab";
    } else if (tabIndicators?.includes("bookmark")) {
      indicatorL10nId = "firefoxview-opentabs-bookmarked-tab";
    }
  }
  return indicatorL10nId;
}

/**
 * Gets the primary l10n args for a tab when normalizing for fxview-tab-list
 *
 * @param {MozTabbrowserTab[]} tab
 *   Tab to fetch container info on.
 * @param {boolean} isRecentBrowsing
 *   Whether the tabs are going to be displayed on the Recent Browsing page or not
 * @param {string} url
 *   URL for the given tab
 * @returns {string}
 *  L10n ID args
 */
function getPrimaryL10nArgs(tab, isRecentBrowsing, url) {
  return JSON.stringify({ tabTitle: tab.label, url });
}

/**
 * Check if a given url is pinned on the new tab page
 *
 * @param {string} url
 *   url to check
 * @returns {boolean}
 *   is tabbed pinned on new tab page
 */
function checkIfPinnedNewTab(url) {
  return url && lazy.NewTabUtils.pinnedLinks.isPinned({ url });
}

/**
 * Convert a list of tabs into the format expected by the fxview-tab-list
 * component.
 *
 * @param {MozTabbrowserTab[]} tabs
 *   Tabs to format.
 * @param {boolean} isRecentBrowsing
 *   Whether the tabs are going to be displayed on the Recent Browsing page or not
 * @returns {object[]}
 *   Formatted objects.
 */
function getTabListItems(tabs, isRecentBrowsing) {
  let filtered = tabs?.filter(tab => !tab.closing && !tab.hidden);

  return filtered.map(tab => {
    let tabIndicators = getIndicatorsForTab(tab);
    let containerObj = getContainerObj(tab);
    const url = tab?.linkedBrowser?.currentURI?.spec || "";
    return {
      containerObj,
      indicators: tabIndicators,
      icon: tab.getAttribute("image"),
      primaryL10nId: getPrimaryL10nId(isRecentBrowsing, tabIndicators),
      primaryL10nArgs: getPrimaryL10nArgs(tab, isRecentBrowsing, url),
      secondaryL10nId:
        isRecentBrowsing || (!isRecentBrowsing && !tab.pinned)
          ? "fxviewtabrow-options-menu-button"
          : null,
      secondaryL10nArgs:
        isRecentBrowsing || (!isRecentBrowsing && !tab.pinned)
          ? JSON.stringify({ tabTitle: tab.label })
          : null,
      tertiaryL10nId:
        isRecentBrowsing || (!isRecentBrowsing && !tab.pinned)
          ? "fxviewtabrow-close-tab-button"
          : null,
      tertiaryL10nArgs:
        isRecentBrowsing || (!isRecentBrowsing && !tab.pinned)
          ? JSON.stringify({ tabTitle: tab.label })
          : null,
      tabElement: tab,
      time: tab.lastAccessed,
      title: tab.label,
      url,
    };
  });
}
