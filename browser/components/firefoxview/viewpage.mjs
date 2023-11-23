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
    };
  }

  constructor() {
    super();
    this.selectedTab = false;
    this.recentBrowsing = Boolean(this.closest("VIEW-RECENTBROWSING"));
    this.onVisibilityChange = this.onVisibilityChange.bind(this);
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
  }

  enter() {
    this.selectedTab = true;
    if (this.isVisible) {
      this.paused = false;
      this.viewVisibleCallback();
    }
  }

  exit() {
    this.selectedTab = false;
    this.paused = true;
    this.viewHiddenCallback();
  }
}
