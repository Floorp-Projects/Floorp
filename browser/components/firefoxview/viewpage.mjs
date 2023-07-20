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

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

export class ViewPage extends MozLitElement {
  static get properties() {
    return {
      selectedTab: { type: Boolean },
      overview: { type: Boolean },
    };
  }

  constructor() {
    super();
    this.selectedTab = false;
    this.overview = Boolean(this.closest("VIEW-OVERVIEW"));
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {}

  enter() {
    this.selectedTab = true;
  }

  exit() {
    this.selectedTab = false;
  }

  getWindow() {
    return window.browsingContext.embedderWindowGlobal.browsingContext.window;
  }

  /**
   * This function doesn't just copy the link to the clipboard, it creates a
   * URL object on the clipboard, so when it's pasted into an application that
   * supports it, it displays the title as a link.
   */
  copyLink(e) {
    // Copied from doCommand/placesCmd_copy in PlacesUIUtils.sys.mjs

    // This is a little hacky, but there is a lot of code in Places that handles
    // clipboard stuff, so it's easier to reuse.
    let node = {};
    node.type = 0;
    node.title = this.triggerNode.title;
    node.uri = this.triggerNode.url;

    // This order is _important_! It controls how this and other applications
    // select data to be inserted based on type.
    let contents = [
      { type: lazy.PlacesUtils.TYPE_X_MOZ_URL, entries: [] },
      { type: lazy.PlacesUtils.TYPE_HTML, entries: [] },
      { type: lazy.PlacesUtils.TYPE_PLAINTEXT, entries: [] },
    ];

    contents.forEach(function (content) {
      content.entries.push(lazy.PlacesUtils.wrapNode(node, content.type));
    });

    let xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    xferable.init(null);

    function addData(type, data) {
      xferable.addDataFlavor(type);
      xferable.setTransferData(type, lazy.PlacesUtils.toISupportsString(data));
    }

    contents.forEach(function (content) {
      addData(content.type, content.entries.join(lazy.PlacesUtils.endl));
    });

    Services.clipboard.setData(
      xferable,
      null,
      Ci.nsIClipboard.kGlobalClipboard
    );
  }

  openInNewWindow(e) {
    this.getWindow().openTrustedLinkIn(this.triggerNode.url, "window", {
      private: false,
    });
  }

  openInNewPrivateWindow(e) {
    this.getWindow().openTrustedLinkIn(this.triggerNode.url, "window", {
      private: true,
    });
  }
}
