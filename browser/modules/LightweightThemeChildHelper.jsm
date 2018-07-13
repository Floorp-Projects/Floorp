/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["LightweightThemeChildHelper"];

/**
 * LightweightThemeChildHelper forwards theme data to in-content pages.
 */
var LightweightThemeChildHelper = {
  listener: null,
  whitelist: [],

  /**
   * Listen to theme updates for the current process
   * @param {Array} whitelist The pages that can receive theme updates.
   */
  listen(whitelist) {
    if (!this.listener) {
      // Clone the whitelist to avoid leaking the global the whitelist
      // originates from.
      this.whitelist = new Set([...whitelist]);
      this.listener = ({ changedKeys }) => {
        if (changedKeys.find(change => change.startsWith("theme/"))) {
          this._updateProcess(changedKeys);
        }
      };
      Services.cpmm.sharedData.addEventListener("change", this.listener);
      Services.obs.addObserver(() => {
        Services.cpmm.sharedData.removeEventListener("change", this.listener);
      }, "xpcom-shutdown");
    }
  },

  /**
   * Update the theme data for the whole process
   * @param {Array} changedKeys The sharedData keys that were changed.
   */
  _updateProcess(changedKeys) {
    const windowEnumerator = Services.ww.getWindowEnumerator();
    while (windowEnumerator.hasMoreElements()) {
      const window = windowEnumerator.getNext().QueryInterface(Ci.nsIDOMWindow);
      const tabChildGlobal = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIDocShell)
                                   .sameTypeRootTreeItem
                                   .QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIContentFrameMessageManager);
      const {chromeOuterWindowID, content} = tabChildGlobal;
      if (changedKeys.includes(`theme/${chromeOuterWindowID}`) &&
          content && this.whitelist.has(content.document.documentURI)) {
        this.update(chromeOuterWindowID, content);
      }
    }
  },

  /**
   * Forward the theme data to the page.
   * @param {Object} outerWindowID The outerWindowID the parent process window has.
   * @param {Object} content The receiving global
   */
  update(outerWindowID, content) {
    const event = Cu.cloneInto({
      detail: {
        data: Services.cpmm.sharedData.get(`theme/${outerWindowID}`)
      },
    }, content);
    content.dispatchEvent(new content.CustomEvent("LightweightTheme:Set",
                                                  event));
  },
};
