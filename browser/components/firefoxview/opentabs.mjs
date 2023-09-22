/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  classMap,
  html,
  map,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  UIState: "resource://services-sync/UIState.sys.mjs",
  TabsSetupFlowManager:
    "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

const TOPIC_DEVICELIST_UPDATED = "fxaccounts:devicelist_updated";

/**
 * A collection of open tabs grouped by window.
 *
 * @property {Map<Window, MozTabbrowserTab[]>} windows
 *   A mapping of windows to their respective list of open tabs.
 */
class OpenTabsInView extends ViewPage {
  static properties = {
    windows: { type: Map },
  };
  static TAB_ATTRS_TO_WATCH = Object.freeze(["image", "label"]);

  constructor() {
    super();
    this.everyWindowCallbackId = `firefoxview-${Services.uuid.generateUUID()}`;
    this.windows = new Map();
    this.currentWindow = this.getWindow();
    this.isPrivateWindow = lazy.PrivateBrowsingUtils.isWindowPrivate(
      this.currentWindow
    );
    this.boundObserve = (...args) => this.observe(...args);
    this.devices = [];
  }

  connectedCallback() {
    super.connectedCallback();
    lazy.EveryWindow.registerCallback(
      this.everyWindowCallbackId,
      win => {
        if (win.gBrowser && this._shouldShowOpenTabs(win) && !win.closed) {
          const { tabContainer } = win.gBrowser;
          tabContainer.addEventListener("TabAttrModified", this);
          tabContainer.addEventListener("TabClose", this);
          tabContainer.addEventListener("TabMove", this);
          tabContainer.addEventListener("TabOpen", this);
          tabContainer.addEventListener("TabPinned", this);
          tabContainer.addEventListener("TabUnpinned", this);
          this._updateOpenTabsList();
        }
      },
      win => {
        if (win.gBrowser && this._shouldShowOpenTabs(win)) {
          const { tabContainer } = win.gBrowser;
          tabContainer.removeEventListener("TabAttrModified", this);
          tabContainer.removeEventListener("TabClose", this);
          tabContainer.removeEventListener("TabMove", this);
          tabContainer.removeEventListener("TabOpen", this);
          tabContainer.removeEventListener("TabPinned", this);
          tabContainer.removeEventListener("TabUnpinned", this);
          this._updateOpenTabsList();
        }
      }
    );
    this._updateOpenTabsList();
    this.addObserversIfNeeded();

    if (this.currentWindow.gSync) {
      this.devices = this.currentWindow.gSync.getSendTabTargets();
    }
  }

  disconnectedCallback() {
    lazy.EveryWindow.unregisterCallback(this.everyWindowCallbackId);
    this.removeObserversIfNeeded();
  }

  addObserversIfNeeded() {
    if (!this.observerAdded) {
      Services.obs.addObserver(this.boundObserve, lazy.UIState.ON_UPDATE);
      Services.obs.addObserver(this.boundObserve, TOPIC_DEVICELIST_UPDATED);
      this.observerAdded = true;
    }
  }

  removeObserversIfNeeded() {
    if (this.observerAdded) {
      Services.obs.removeObserver(this.boundObserve, lazy.UIState.ON_UPDATE);
      Services.obs.removeObserver(this.boundObserve, TOPIC_DEVICELIST_UPDATED);
      this.observerAdded = false;
    }
  }

  async observe(subject, topic, data) {
    switch (topic) {
      case lazy.UIState.ON_UPDATE:
        if (!this.devices.length && lazy.TabsSetupFlowManager.fxaSignedIn) {
          this.devices = this.currentWindow.gSync.getSendTabTargets();
        }
        break;
      case TOPIC_DEVICELIST_UPDATED:
        const deviceListUpdated =
          await lazy.fxAccounts.device.refreshDeviceList();
        if (deviceListUpdated) {
          this.devices = this.currentWindow.gSync.getSendTabTargets();
        }
    }
  }

  render() {
    if (!this.selectedTab && !this.recentBrowsing) {
      return null;
    }
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
        href="chrome://browser/content/firefoxview/firefoxview-next.css"
      />
      <div class="sticky-container bottom-fade">
        <h2 class="page-header" data-l10n-id="firefoxview-opentabs-header"></h2>
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
                data-inner-id="${this.currentWindow.windowGlobalChild
                  .innerWindowId}"
                data-l10n-id="firefoxview-opentabs-current-window-header"
                data-l10n-args="${JSON.stringify({
                  winID: currentWindowIndex,
                })}"
                .devices=${this.devices}
              ></view-opentabs-card>
            `
        )}
        ${map(
          otherWindows,
          ([winID, tabs, win]) => html`
            <view-opentabs-card
              class=${cardClasses}
              .tabs=${tabs}
              data-inner-id="${win.windowGlobalChild.innerWindowId}"
              data-l10n-id="firefoxview-opentabs-window-header"
              data-l10n-args="${JSON.stringify({ winID })}"
              .devices=${this.devices}
            ></view-opentabs-card>
          `
        )}
      </div>
    `;
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
      .sort((a, b) => b.lastAccessed - a.lastAccessed);
    return html`<view-opentabs-card
      .tabs=${tabs}
      .recentBrowsing=${true}
      .devices=${this.devices}
    ></view-opentabs-card>`;
  }

  handleEvent({ detail, target, type }) {
    const win = target.ownerGlobal;
    const tabs = this.windows.get(win);
    switch (type) {
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
      const selector = `view-opentabs-card[data-inner-id="${win.windowGlobalChild.innerWindowId}"]`;
      this.shadowRoot.querySelector(selector)?.requestUpdate();
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
class OpenTabsInViewCard extends ViewPage {
  static properties = {
    showMore: { type: Boolean },
    tabs: { type: Array },
    title: { type: String },
    recentBrowsing: { type: Boolean },
    devices: { type: Array },
    triggerNode: { type: Object },
  };
  static MAX_TABS_FOR_COMPACT_HEIGHT = 7;

  constructor() {
    super();
    this.showMore = false;
    this.tabs = [];
    this.title = "";
    this.recentBrowsing = false;
    this.devices = [];
  }

  static queries = {
    cardEl: "card-container",
    panelList: "panel-list",
    tabList: "fxview-tab-list",
  };

  closeTab(e) {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.removeTab(tab);
    this.recordContextMenuTelemetry("close-tab", e);
  }

  moveTabsToStart() {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.moveTabsToStart(tab);
  }

  moveTabsToEnd() {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.moveTabsToEnd(tab);
  }

  moveTabsToWindow() {
    const tab = this.triggerNode.tabElement;
    tab?.ownerGlobal.gBrowser.replaceTabsWithWindow(tab);
  }

  moveMenuTemplate() {
    const tab = this.triggerNode?.tabElement;
    const browserWindow = tab?.ownerGlobal;
    const position = tab?._tPos;
    const tabs = browserWindow?.gBrowser.tabs || [];

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

    if (device && this.triggerNode) {
      await this.getWindow().gSync.sendTabToDevice(
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

  panelListTemplate() {
    return html`
      <panel-list slot="menu" data-tab-type="opentabs">
        <panel-item
          data-l10n-id="fxviewtabrow-close-tab"
          data-l10n-attrs="accesskey"
          @click=${this.closeTab}
        ></panel-item>
        <panel-item
          data-l10n-id="fxviewtabrow-move-tab"
          data-l10n-attrs="accesskey"
          submenu="move-tab-menu"
        >
          ${this.moveMenuTemplate()}
        </panel-item>
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

  openContextMenu(e) {
    this.triggerNode = e.originalTarget;
    this.panelList.toggle(e.detail.originalEvent);
  }

  getMaxTabsLength() {
    if (this.recentBrowsing) {
      return 5;
    } else if (this.classList.contains("height-limited") && !this.showMore) {
      return OpenTabsInViewCard.MAX_TABS_FOR_COMPACT_HEIGHT;
    }
    return -1;
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

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview-next.css"
      />
      <card-container
        ?preserveCollapseState=${this.recentBrowsing}
        shortPageName=${this.recentBrowsing ? "opentabs" : null}
        ?showViewAll=${this.recentBrowsing}
      >
        ${this.recentBrowsing
          ? html`<h3
              slot="header"
              data-l10n-id=${"firefoxview-opentabs-header"}
            ></h3>`
          : html`<h3 slot="header">${this.title}</h3>`}
        <div class="fxview-tab-list-container" slot="main">
          <fxview-tab-list
            class="with-context-menu"
            .hasPopup=${"menu"}
            ?compactRows=${this.classList.contains("width-limited")}
            @fxview-tab-list-primary-action=${onTabListRowClick}
            @fxview-tab-list-secondary-action=${this.openContextMenu}
            .maxTabsLength=${this.getMaxTabsLength()}
            .tabItems=${getTabListItems(this.tabs)}
            >${this.panelListTemplate()}</fxview-tab-list
          >
        </div>
        ${!this.recentBrowsing
          ? html` <div
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
            ></div>`
          : null}
      </card-container>
    `;
  }
}
customElements.define("view-opentabs-card", OpenTabsInViewCard);

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

function onTabListRowClick(event) {
  const tab = event.originalTarget.tabElement;
  const browserWindow = tab.ownerGlobal;
  browserWindow.focus();
  browserWindow.gBrowser.selectedTab = tab;
}
