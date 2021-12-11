/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PageDataParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
});

/**
 * Receives messages from PageDataChild and passes them to the PageData service.
 */
class PageDataParent extends JSWindowActorParent {
  #deferredCollection = null;

  /**
   * Starts data collection in the child process. Returns a promise that
   * resolves to the initial set of data discovered.
   *
   * @returns {Promise<Data[]>}
   */
  collectPageData() {
    if (!this.#deferredCollection) {
      this.#deferredCollection = PromiseUtils.defer();
      this.sendQuery("PageData:Collect").then(
        this.#deferredCollection.resolve,
        this.#deferredCollection.reject
      );
    }

    return this.#deferredCollection.promise;
  }

  /**
   * Called when the page is destroyed.
   */
  didDestroy() {
    this.#deferredCollection?.reject(
      new Error("Page destroyed before collection completed.")
    );
  }

  /**
   * Called when a message is received from the content process.
   *
   * @param {ReceiveMessageArgument} msg
   *   The received message.
   */
  receiveMessage(msg) {
    switch (msg.name) {
      case "PageData:DocumentReady":
        PageDataService.pageLoaded(this, msg.data.url);
        break;
      // TODO: This is for supporting listening to dynamic changes. See
      // PageDataChild.jsm for more information.
      // case "PageData:Collected":
      //   PageDataService.pageDataDiscovered(msg.data.url, msg.data.data);
      //   break;
    }
  }
}
