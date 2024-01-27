/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/card-container.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-empty-state.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-search-textbox.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-tab-list.mjs";

import { placeLinkOnClipboard } from "./helpers.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
});

const WIN_RESIZE_DEBOUNCE_RATE_MS = 500;
const WIN_RESIZE_DEBOUNCE_TIMEOUT_MS = 1000;

/**
 * A base class for content container views displayed on firefox-view.
 *
 * @property {boolean} recentBrowsing
 *   Is part of the recentbrowsing page view
 * @property {boolean} paused
 *   No content will be updated and rendered while paused
 */
export class ViewPageContent extends MozLitElement {
  static get properties() {
    return {
      recentBrowsing: { type: Boolean },
      paused: { type: Boolean },
    };
  }
  constructor() {
    super();
    // don't update or render until explicitly un-paused
    this.paused = true;
  }

  get ownerViewPage() {
    return this.closest("[type='page']") || this;
  }

  get isVisible() {
    if (!this.isConnected || this.ownerDocument.visibilityState != "visible") {
      return false;
    }
    return this.ownerViewPage.selectedTab;
  }

  /**
   * Override this function to run a callback whenever this content is visible.
   */
  viewVisibleCallback() {}

  /**
   * Override this function to run a callback whenever this content is hidden.
   */
  viewHiddenCallback() {}

  getWindow() {
    return window.browsingContext.embedderWindowGlobal.browsingContext.window;
  }

  getBrowserTab() {
    return this.getWindow().gBrowser.getTabForBrowser(
      window.browsingContext.embedderElement
    );
  }

  copyLink(e) {
    placeLinkOnClipboard(this.triggerNode.title, this.triggerNode.url);
    this.recordContextMenuTelemetry("copy-link", e);
  }

  openInNewWindow(e) {
    this.getWindow().openTrustedLinkIn(this.triggerNode.url, "window", {
      private: false,
    });
    this.recordContextMenuTelemetry("open-in-new-window", e);
  }

  openInNewPrivateWindow(e) {
    this.getWindow().openTrustedLinkIn(this.triggerNode.url, "window", {
      private: true,
    });
    this.recordContextMenuTelemetry("open-in-private-window", e);
  }

  recordContextMenuTelemetry(menuAction, event) {
    Services.telemetry.recordEvent(
      "firefoxview_next",
      "context_menu",
      "tabs",
      null,
      {
        menu_action: menuAction,
        data_type: event.target.panel.dataset.tabType,
      }
    );
  }

  shouldUpdate(changedProperties) {
    return !this.paused && super.shouldUpdate(changedProperties);
  }
}

/**
 * A "page" in firefox view, which may be hidden or shown by the named-deck container or
 * via the owner document's visibility
 *
 * @property {boolean} selectedTab
 *   Is this page the selected view in the named-deck container
 */
export class ViewPage extends ViewPageContent {
  static get properties() {
    return {
      selectedTab: { type: Boolean },
      searchTextboxSize: { type: Number },
    };
  }

  constructor() {
    super();
    this.selectedTab = false;
    this.recentBrowsing = Boolean(this.recentBrowsingElement);
    this.onVisibilityChange = this.onVisibilityChange.bind(this);
    this.onResize = this.onResize.bind(this);
  }

  get recentBrowsingElement() {
    return this.closest("VIEW-RECENTBROWSING");
  }

  onResize() {
    this.windowResizeTask = new lazy.DeferredTask(
      () => this.updateAllVirtualLists(),
      WIN_RESIZE_DEBOUNCE_RATE_MS,
      WIN_RESIZE_DEBOUNCE_TIMEOUT_MS
    );
    this.windowResizeTask?.arm();
  }

  onVisibilityChange(event) {
    if (this.isVisible) {
      this.paused = false;
      this.viewVisibleCallback();
    } else if (
      this.ownerViewPage.selectedTab &&
      this.ownerDocument.visibilityState == "hidden"
    ) {
      this.paused = true;
      this.viewHiddenCallback();
    }
  }

  connectedCallback() {
    super.connectedCallback();
    this.ownerDocument.addEventListener(
      "visibilitychange",
      this.onVisibilityChange
    );
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.ownerDocument.removeEventListener(
      "visibilitychange",
      this.onVisibilityChange
    );
    this.getWindow().removeEventListener("resize", this.onResize);
  }

  updateAllVirtualLists() {
    if (!this.paused) {
      let tabLists = [];
      if (this.recentBrowsing) {
        let viewComponents = this.querySelectorAll("[slot]");
        viewComponents.forEach(viewComponent => {
          let currentTabLists = [];
          if (viewComponent.nodeName.includes("OPENTABS")) {
            viewComponent.viewCards.forEach(viewCard => {
              currentTabLists.push(viewCard.tabList);
            });
          } else {
            currentTabLists =
              viewComponent.shadowRoot.querySelectorAll("fxview-tab-list");
          }
          tabLists.push(...currentTabLists);
        });
      } else {
        tabLists = this.shadowRoot.querySelectorAll("fxview-tab-list");
      }
      tabLists.forEach(tabList => {
        if (!tabList.updatesPaused && tabList.rootVirtualListEl?.isVisible) {
          tabList.rootVirtualListEl.recalculateAfterWindowResize();
        }
      });
    }
  }

  toggleVisibilityInCardContainer(isOpenTabs) {
    let cards = [];
    let tabLists = [];
    if (!isOpenTabs) {
      cards = this.shadowRoot.querySelectorAll("card-container");
      tabLists = this.shadowRoot.querySelectorAll("fxview-tab-list");
    } else {
      this.viewCards.forEach(viewCard => {
        if (viewCard.cardEl) {
          cards.push(viewCard.cardEl);
          tabLists.push(viewCard.tabList);
        }
      });
    }
    if (tabLists.length && cards.length) {
      cards.forEach(cardEl => {
        if (cardEl.visible !== !this.paused) {
          cardEl.visible = !this.paused;
        } else if (
          cardEl.isExpanded &&
          Array.from(tabLists).some(
            tabList => tabList.updatesPaused !== this.paused
          )
        ) {
          // If card is already visible and expanded but tab-list has updatesPaused,
          // update the tab-list updatesPaused prop from here instead of card-container
          tabLists.forEach(tabList => {
            tabList.updatesPaused = this.paused;
          });
        }
      });
    }
  }

  enter() {
    this.selectedTab = true;
    if (this.isVisible) {
      this.paused = false;
      this.viewVisibleCallback();
      this.getWindow().addEventListener("resize", this.onResize);
    }
  }

  exit() {
    this.selectedTab = false;
    this.paused = true;
    this.viewHiddenCallback();
    if (!this.windowResizeTask?.isFinalized) {
      this.windowResizeTask?.finalize();
    }
    this.getWindow().removeEventListener("resize", this.onResize);
  }
}
