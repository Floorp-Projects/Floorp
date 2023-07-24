/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  classMap,
  html,
  ifDefined,
  map,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { ViewPage } from "./viewpage.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

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
  }

  connectedCallback() {
    super.connectedCallback();
    lazy.EveryWindow.registerCallback(
      this.everyWindowCallbackId,
      win => {
        if (
          win.gBrowser &&
          !lazy.PrivateBrowsingUtils.isWindowPrivate(win) &&
          !win.closed
        ) {
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
        if (win.gBrowser && !lazy.PrivateBrowsingUtils.isWindowPrivate(win)) {
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
  }

  disconnectedCallback() {
    lazy.EveryWindow.unregisterCallback(this.everyWindowCallbackId);
  }

  render() {
    if (!this.selectedTab && !this.overview) {
      return null;
    }
    if (this.overview) {
      return this._getOverviewTemplate();
    }
    const { window: currentWindow } =
      window.browsingContext.embedderWindowGlobal.browsingContext;
    let currentWindowIndex, currentWindowTabs;
    let index = 1;
    const otherWindows = [];
    this.windows.forEach((tabs, win) => {
      if (win === currentWindow) {
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
        class="${classMap({
          "view-opentabs-card-container": true,
          "one-column": this.windows.size <= 1,
          "two-columns": this.windows.size === 2,
          "three-columns": this.windows.size >= 3,
        })} cards-container"
      >
        ${when(
          currentWindowIndex && currentWindowTabs,
          () =>
            html`
              <view-opentabs-card
                class=${cardClasses}
                .tabs=${currentWindowTabs}
                data-inner-id="${currentWindow.windowGlobalChild.innerWindowId}"
                data-l10n-id="firefoxview-opentabs-current-window-header"
                data-l10n-args="${JSON.stringify({
                  winID: currentWindowIndex,
                })}"
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
            ></view-opentabs-card>
          `
        )}
      </div>
    `;
  }

  /**
   * Render a template for the overview page, which shows a single list of
   * recently accessed tabs, rather than a list of tabs per window.
   *
   * @returns {TemplateResult}
   *   The overview template.
   */
  _getOverviewTemplate() {
    const recentTabs = Array.from(this.windows.values())
      .flat()
      .sort((a, b) => b.lastAccessed - a.lastAccessed);
    return html`<card-container preserveCollapseState viewAllPage="opentabs">
      <h2 data-l10n-id="firefoxview-opentabs-header" slot="header"></h2>
      <fxview-tab-list
        @fxview-tab-list-primary-action=${onTabListRowClick}
        maxTabsLength="5"
        slot="main"
        .tabItems=${getTabListItems(recentTabs)}
      ></fxview-tab-list>
    </card-container>`;
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
    if (!this.overview) {
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
          win =>
            win.gBrowser &&
            !lazy.PrivateBrowsingUtils.isWindowPrivate(win) &&
            !win.closed
        )
        .map(win => [win, [...win.gBrowser.tabs]])
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
class OpenTabsInViewCard extends MozLitElement {
  static properties = {
    showMore: { type: Boolean },
    tabs: { type: Array },
    title: { type: String },
  };
  static MAX_TABS_FOR_COMPACT_HEIGHT = 7;

  constructor() {
    super();
    this.showMore = false;
    this.tabs = [];
    this.title = "";
  }

  render() {
    return html`
      <card-container>
        <h2 slot="header">${this.title}</h2>
        <div class="fxview-tab-list-container" slot="main">
          <fxview-tab-list
            ?compactRows=${this.classList.contains("width-limited")}
            @fxview-tab-list-primary-action=${onTabListRowClick}
            .maxTabsLength=${ifDefined(
              this.classList.contains("height-limited") && !this.showMore
                ? OpenTabsInViewCard.MAX_TABS_FOR_COMPACT_HEIGHT
                : null
            )}
            .tabItems=${ifDefined(getTabListItems(this.tabs))}
          ></fxview-tab-list>
        </div>
        <div
          @click=${() => (this.showMore = !this.showMore)}
          data-l10n-id="${this.showMore
            ? "firefoxview-show-less"
            : "firefoxview-show-more"}"
          ?hidden=${!this.classList.contains("height-limited") ||
          this.tabs.length <= OpenTabsInViewCard.MAX_TABS_FOR_COMPACT_HEIGHT}
          slot="footer"
        ></div>
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
      primaryL10nId: "firefoxview-opentabs-focus-tab",
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
