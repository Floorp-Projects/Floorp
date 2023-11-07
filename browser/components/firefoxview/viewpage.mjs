/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/card-container.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-empty-state.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/firefoxview/fxview-tab-list.mjs";

import { placeLinkOnClipboard } from "./helpers.mjs";

export class ViewPage extends MozLitElement {
  static get properties() {
    return {
      selectedTab: { type: Boolean },
      recentBrowsing: { type: Boolean },
    };
  }

  constructor() {
    super();
    this.selectedTab = false;
    this.recentBrowsing = Boolean(this.closest("VIEW-RECENTBROWSING"));
  }

  connectedCallback() {
    super.connectedCallback();
    this.ownerDocument.addEventListener("visibilitychange", this);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.ownerDocument.removeEventListener("visibilitychange", this);
  }

  handleEvent(event) {
    switch (event.type) {
      case "visibilitychange":
        if (this.ownerDocument.visibilityState === "visible") {
          this.viewTabVisibleCallback();
        } else {
          this.viewTabHiddenCallback();
        }
        break;
    }
  }

  /**
   * Override this function to run a callback whenever Firefox View is visible.
   */
  viewTabVisibleCallback() {}

  /**
   * Override this function to run a callback whenever Firefox View is hidden.
   */
  viewTabHiddenCallback() {}

  enter() {
    this.selectedTab = true;
  }

  exit() {
    this.selectedTab = false;
  }

  getWindow() {
    return window.browsingContext.embedderWindowGlobal.browsingContext.window;
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
}
