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
 * Returns the list of page data collectors for a document.
 *
 * @param {Document} document
 *   The DOM document to collect data for.
 * @returns {PageDataCollector[]}
 */
function getCollectors(document) {
  return [new SchemaOrgPageData(document), new OpenGraphPageData(document)];
}

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
   * The current set of page data collectors for the page and their current data
   * or null if data collection has not begun.
   * @type {Map<PageDataCollector, Data[]> | null}
   */
  #collectors = null;

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
   * Coalesces the data from the page data collectors into a single array.
   *
   * @returns {Data[]}
   */
  #buildData() {
    if (!this.#collectors) {
      return [];
    }

    let results = [];
    for (let data of this.#collectors.values()) {
      if (data !== null) {
        results = results.concat(data);
      }
    }

    return results;
  }

  /**
   * Begins page data collection on the page.
   */
  async #beginCollection() {
    if (this.#collectors !== null) {
      // Already collecting.
      return this.#buildData();
    }

    logConsole.debug("Starting collection", this.document.documentURI);

    // let initialCollection = true;

    this.#collectors = new Map();
    let pending = [];
    for (let collector of getCollectors(this.document)) {
      // TODO: Implement monitoring of pages for changes, e.g. for SPAs changing
      // video without reloading.
      //
      // The commented out code below is a first attempt, that would allow
      // individual collectors to provide updates. It will need fixing to
      // ensure that listeners are either removed or not re-added on fresh
      // page loads, as would happen currently.
      //
      // collector.on("data", (type, data) => {
      //   this.#collectors.set(collector, data);
      //
      //   // Do nothing if intial collection is still ongoing.
      //   if (!initialCollection) {
      //     // TODO debounce this.
      //     this.sendAsyncMessage("PageData:Collected", {
      //       url: this.document.documentURI,
      //       data: this.#buildData(),
      //     });
      //   }
      // });

      pending.push(
        collector.init().then(
          data => {
            this.#collectors.set(collector, data);
          },
          error => {
            this.#collectors.set(collector, []);
            logConsole.error(`Failed collecting page data`, error);
          }
        )
      );
    }

    await Promise.all(pending);
    // initialCollection = false;

    return this.#buildData();
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
        return this.#beginCollection();
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
