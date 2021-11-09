/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PageDataChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  OpenGraphPageData: "resource:///modules/pagedata/OpenGraphPageData.jsm",
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  SchemaOrgPageData: "resource:///modules/pagedata/SchemaOrgPageData.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", function() {
  return console.createInstance({
    prefix: "PageData",
    maxLogLevel: Services.prefs.getBoolPref("browser.pagedata.log", false)
      ? "Debug"
      : "Warn",
  });
});

// We defer any attempt to check for page data for a short time after a page
// loads to allow JS to operate.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "READY_DELAY",
  "browser.pagedata.readyDelay",
  500
);

/**
 * The list of page data collectors. These should be sorted in order of
 * specificity, if the same piece of data is provided by two collectors then the
 * earlier wins.
 *
 * Collectors must provide a `collect` function which will be passed the
 * document object and should return the PageData structure. The function may be
 * asynchronous if needed.
 *
 * The data returned need not be valid, collectors should return whatever they
 * can and then we drop anything that is invalid once all data is joined.
 */
XPCOMUtils.defineLazyGetter(this, "DATA_COLLECTORS", function() {
  return [SchemaOrgPageData, OpenGraphPageData];
});

/**
 * The actor responsible for monitoring a page for page data.
 */
class PageDataChild extends JSWindowActorChild {
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
    this.#isContentWindowPrivate = PrivateBrowsingUtils.isContentWindowPrivate(
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
      READY_DELAY,
      Ci.nsITimer.TYPE_ONE_SHOT_LOW_PRIORITY
    );
  }

  /**
   * Collects data from the page.
   */
  async #collectData() {
    logConsole.debug("Starting collection", this.document.documentURI);

    let pending = DATA_COLLECTORS.map(async collector => {
      try {
        return await collector.collect(this.document);
      } catch (e) {
        logConsole.error("Error collecting page data", e);
        return {};
      }
    });

    let pageDataList = await Promise.all(pending);

    let pageData = pageDataList.reduce(PageDataSchema.coalescePageData, {
      date: Date.now(),
      url: this.document.documentURI,
    });

    try {
      return PageDataSchema.validatePageData(pageData);
    } catch (e) {
      logConsole.error("Failed to collect valid page data", e);
      return null;
    }
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
        return this.#collectData();
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
