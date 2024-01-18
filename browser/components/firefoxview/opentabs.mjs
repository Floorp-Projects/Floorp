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

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

const TOPIC_CURRENT_BROWSER_CHANGED = "net:current-browser-id";

/**
 * A collection of open tabs grouped by window.
 *
 * @property {Map<Window, MozTabbrowserTab[]>} windows
 *   A mapping of windows to their respective list of open tabs.
 */
class OpenTabsInView extends ViewPage {
  static properties = {
    ...ViewPage.properties,
    windows: { type: Map },
    searchQuery: { type: String },
  };
  static queries = {
    viewCards: { all: "view-opentabs-card" },
    searchTextbox: "fxview-search-textbox",
  };

  static TAB_ATTRS_TO_WATCH = Object.freeze(["image", "label"]);

  constructor() {
    super();
    this._started = false;
    this.everyWindowCallbackId = `firefoxview-${Services.uuid.generateUUID()}`;
    this.windows = new Map();
    this.currentWindow = this.getWindow();
    this.isPrivateWindow = lazy.PrivateBrowsingUtils.isWindowPrivate(
      this.currentWindow
    );
    this.boundObserve = (...args) => this.observe(...args);
    this.searchQuery = "";
  }

  start() {
    if (this._started) {
      return;
    }
    this._started = true;

    Services.obs.addObserver(this.boundObserve, TOPIC_CURRENT_BROWSER_CHANGED);

    lazy.EveryWindow.registerCallback(
      this.everyWindowCallbackId,
      win => {
        if (win.gBrowser && this._shouldShowOpenTabs(win) && !win.closed) {
          const { tabContainer } = win.gBrowser;
          tabContainer.addEventListener("TabSelect", this);
          tabContainer.addEventListener("TabAttrModified", this);
          tabContainer.addEventListener("TabClose", this);
          tabContainer.addEventListener("TabMove", this);
          tabContainer.addEventListener("TabOpen", this);
          tabContainer.addEventListener("TabPinned", this);
          tabContainer.addEventListener("TabUnpinned", this);
          // BrowserWindowWatcher doesnt always notify "net:current-browser-id" when
          // restoring a window, so we need to listen for "activate" events here as well.
          win.addEventListener("activate", this);
          this._updateOpenTabsList();
        }
      },
      win => {
        if (win.gBrowser && this._shouldShowOpenTabs(win)) {
          const { tabContainer } = win.gBrowser;
          tabContainer.removeEventListener("TabSelect", this);
          tabContainer.removeEventListener("TabAttrModified", this);
          tabContainer.removeEventListener("TabClose", this);
          tabContainer.removeEventListener("TabMove", this);
          tabContainer.removeEventListener("TabOpen", this);
          tabContainer.removeEventListener("TabPinned", this);
          tabContainer.removeEventListener("TabUnpinned", this);
          win.removeEventListener("activate", this);
          this._updateOpenTabsList();
        }
      }
    );
    // EveryWindow will invoke the callback for existing windows - including this one
    // So this._updateOpenTabsList will get called for the already-open window

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

    lazy.EveryWindow.unregisterCallback(this.everyWindowCallbackId);

    Services.obs.removeObserver(
      this.boundObserve,
      TOPIC_CURRENT_BROWSER_CHANGED
    );

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
  }

  viewVisibleCallback() {
    this.start();
  }

  viewHiddenCallback() {
    this.stop();
  }

  async observe(subject, topic, data) {
    switch (topic) {
      case TOPIC_CURRENT_BROWSER_CHANGED:
        this.requestUpdate();
        break;
    }
  }

  render() {
    if (this.recentBrowsing) {
      return this.getRecentBrowsingTemplate();
    }
    let currentWindowIndex, currentWindowTabs;
    let index = 1;
    const otherWindows = [];
    this.windows.forEach((tabs, win) => {
      if (win === this.currentWindow) {
        currentWindowIndex = index++;
        currentWindowTabs = tabs;
      } else {
        otherWindows.push([index++, tabs, win]);
      }
    });

    const cardClasses = classMap({
      "height-limited": this.windows.size > 3,
      "width-limited": this.windows.size > 1,
    });
    let cardCount;
    if (this.windows.size <= 1) {
      cardCount = "one";
    } else if (this.windows.size === 2) {
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
        <h2
          class="page-header heading-large"
          data-l10n-id="firefoxview-opentabs-header"
        ></h2>
        ${when(
          isSearchEnabled(),
          () => html`<div>
            <fxview-search-textbox
              data-l10n-id="firefoxview-search-text-box-opentabs"
              data-l10n-attrs="placeholder"
              @fxview-search-textbox-query=${this.onSearchQuery}
              .size=${this.searchTextboxSize}
            ></fxview-search-textbox>
          </div>`
        )}
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
            ></view-opentabs-card>
          `
        )}
      </div>
    `;
  }

  onSearchQuery(e) {
    this.searchQuery = e.detail.query;
  }

  /**
   * Render a template for the 'Recent browsing' page, which shows a shorter list of
   * open tabs in the current window.
   *
   * @returns {TemplateResult}
   *   The recent browsing template.
   */
  getRecentBrowsingTemplate() {
    const tabs = Array.from(this.windows.values())
      .flat()
      .sort((a, b) => {
        let dt = b.lastSeenActive - a.lastSeenActive;
        if (dt) {
          return dt;
        }
        // try to break a deadlock by sorting the selected tab higher
        if (!(a.selected || b.selected)) {
          return 0;
        }
        return a.selected ? -1 : 1;
      });
    return html`<view-opentabs-card
      .tabs=${tabs}
      .recentBrowsing=${true}
      .paused=${this.paused}
      .searchQuery=${this.searchQuery}
    ></view-opentabs-card>`;
  }

  handleEvent({ detail, target, type }) {
    if (this.recentBrowsing && type === "fxview-search-textbox-query") {
      this.onSearchQuery({ detail });
      return;
    }
    const win = target.ownerGlobal;
    const tabs = this.windows.get(win);
    switch (type) {
      case "TabSelect": {
        // if we're switching away from our tab, we can halt any updates immediately
        if (detail.previousTab == this.getBrowserTab()) {
          this.stop();
        }
        return;
      }
      case "TabAttrModified":
        if (
          !detail.changed.some(attr =>
            OpenTabsInView.TAB_ATTRS_TO_WATCH.includes(attr)
          )
        ) {
          // We don't care about this attr, bail out to avoid change detection.
          return;
        }
        break;
      case "TabClose":
        tabs.splice(target._tPos, 1);
        break;
      case "TabMove":
        [tabs[detail], tabs[target._tPos]] = [tabs[target._tPos], tabs[detail]];
        break;
      case "TabOpen":
        tabs.splice(target._tPos, 0, target);
        break;
      case "TabPinned":
      case "TabUnpinned":
        this.windows.set(win, [...win.gBrowser.tabs]);
        break;
    }
    this.requestUpdate();
    if (!this.recentBrowsing) {
      const cardForWin = this.shadowRoot.querySelector(
        `view-opentabs-card[data-inner-id="${win.windowGlobalChild.innerWindowId}"]`
      );
      if (this.searchQuery) {
        cardForWin?.updateSearchResults();
      }
      cardForWin?.requestUpdate();
    }
  }

  _updateOpenTabsList() {
    this.windows = this._getOpenTabsPerWindow();
  }

  /**
   * Get a list of open tabs for each window.
   *
   * @returns {Map<Window, MozTabbrowserTab[]>}
   */
  _getOpenTabsPerWindow() {
    return new Map(
      Array.from(Services.wm.getEnumerator("navigator:browser"))
        .filter(
          win => win.gBrowser && this._shouldShowOpenTabs(win) && !win.closed
        )
        .map(win => [win, [...win.gBrowser.tabs]])
    );
  }

  _shouldShowOpenTabs(win) {
    return (
      win == this.currentWindow ||
      (!this.isPrivateWindow && !lazy.PrivateBrowsingUtils.isWindowPrivate(win))
    );
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
  }

  static queries = {
    cardEl: "card-container",
    tabContextMenu: "view-opentabs-contextmenu",
    tabList: "fxview-tab-list",
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
      this.showAll = true;
    }
  }

  onTabListRowClick(event) {
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
          <fxview-tab-list
            class="with-context-menu"
            .hasPopup=${"menu"}
            ?compactRows=${this.classList.contains("width-limited")}
            @fxview-tab-list-primary-action=${this.onTabListRowClick}
            @fxview-tab-list-secondary-action=${this.openContextMenu}
            .maxTabsLength=${this.getMaxTabsLength()}
            .tabItems=${this.searchResults || getTabListItems(this.tabs)}
            .searchQuery=${this.searchQuery}
            ><view-opentabs-contextmenu slot="menu"></view-opentabs-contextmenu>
          </fxview-tab-list>
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
          data-l10n-id="fxviewtabrow-close-tab"
          data-l10n-attrs="accesskey"
          @click=${this.closeTab}
        ></panel-item>
        <panel-item
          data-l10n-id="fxviewtabrow-move-tab"
          data-l10n-attrs="accesskey"
          submenu="move-tab-menu"
          >${this.moveMenuTemplate()}</panel-item
        >
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
 * Convert a list of tabs into the format expected by the fxview-tab-list
 * component.
 *
 * @param {MozTabbrowserTab[]} tabs
 *   Tabs to format.
 * @returns {object[]}
 *   Formatted objects.
 */
function getTabListItems(tabs) {
  return tabs
    ?.filter(tab => !tab.closing && !tab.hidden && !tab.pinned)
    .map(tab => ({
      icon: tab.getAttribute("image"),
      primaryL10nId: "firefoxview-opentabs-tab-row",
      primaryL10nArgs: JSON.stringify({
        url: tab.linkedBrowser?.currentURI?.spec,
      }),
      secondaryL10nId: "fxviewtabrow-options-menu-button",
      secondaryL10nArgs: JSON.stringify({ tabTitle: tab.label }),
      tabElement: tab,
      time: tab.lastAccessed,
      title: tab.label,
      url: tab.linkedBrowser?.currentURI?.spec,
    }));
}
