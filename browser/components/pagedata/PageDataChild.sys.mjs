/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

// We defer any attempt to check for page data for a short time after a page
// loads to allow JS to operate.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "READY_DELAY",
  "browser.pagedata.readyDelay",
  500
);

/**
 * The actor responsible for monitoring a page for page data.
 */
export class PageDataChild extends JSWindowActorChild {
  #isContentWindowPrivate = true;
  /**
   * Used to debounce notifications about a page being ready.
   * @type {Timer | null}
   */
  #deferTimer = null;

  /**
   * Called when the actor is created for a new page.
   */
  actorCreated() {
    this.#isContentWindowPrivate = lazy.PrivateBrowsingUtils.isContentWindowPrivate(
      this.contentWindow
    );
  }

  /**
   * Called when the page is destroyed.
   */
  didDestroy() {
    if (this.#deferTimer) {
      this.#deferTimer.cancel();
    }
  }

  /**
   * Called when the page has signalled it is done loading. This signal is
   * debounced by READY_DELAY.
   */
  #deferReady() {
    if (!this.#deferTimer) {
      this.#deferTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }

    // If the timer was already running this re-starts it.
    this.#deferTimer.initWithCallback(
      () => {
        this.#deferTimer = null;
        this.sendAsyncMessage("PageData:DocumentReady", {
          url: this.document.documentURI,
        });
      },
      lazy.READY_DELAY,
      Ci.nsITimer.TYPE_ONE_SHOT_LOW_PRIORITY
    );
  }

  /**
   * Called when a message is received from the parent process.
   *
   * @param {ReceiveMessageArgument} msg
   *   The received message.
   *
   * @returns {Promise | undefined}
   *   A promise for the requested data or undefined if no data was requested.
   */
  receiveMessage(msg) {
    if (this.#isContentWindowPrivate) {
      return undefined;
    }

    switch (msg.name) {
      case "PageData:CheckLoaded":
        // The service just started in the parent. Check if this document is
        // already loaded.
        if (this.document.readystate == "complete") {
          this.#deferReady();
        }
        break;
      case "PageData:Collect":
        return lazy.PageDataSchema.collectPageData(this.document);
    }

    return undefined;
  }

  /**
   * DOM event handler.
   *
   * @param {Event} event
   *   The DOM event.
   */
  handleEvent(event) {
    if (this.#isContentWindowPrivate) {
      return;
    }

    switch (event.type) {
      case "DOMContentLoaded":
      case "pageshow":
        this.#deferReady();
        break;
    }
  }
}
