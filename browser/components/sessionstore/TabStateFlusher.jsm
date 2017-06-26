/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabStateFlusher"];

const Cu = Components.utils;


/**
 * A module that enables async flushes. Updates from frame scripts are
 * throttled to be sent only once per second. If an action wants a tab's latest
 * state without waiting for a second then it can request an async flush and
 * wait until the frame scripts reported back. At this point the parent has the
 * latest data and the action can continue.
 */
this.TabStateFlusher = Object.freeze({
  /**
   * Requests an async flush for the given browser. Returns a promise that will
   * resolve when we heard back from the content process and the parent has
   * all the latest data.
   */
  flush(browser) {
    return TabStateFlusherInternal.flush(browser);
  },

  /**
   * Requests an async flush for all browsers of a given window. Returns a Promise
   * that will resolve when we've heard back from all browsers.
   */
  flushWindow(window) {
    return TabStateFlusherInternal.flushWindow(window);
  },

  /**
   * Resolves the flush request with the given flush ID.
   *
   * @param browser (<xul:browser>)
   *        The browser for which the flush is being resolved.
   * @param flushID (int)
   *        The ID of the flush that was sent to the browser.
   * @param success (bool, optional)
   *        Whether or not the flush succeeded.
   * @param message (string, optional)
   *        An error message that will be sent to the Console in the
   *        event that a flush failed.
   */
  resolve(browser, flushID, success = true, message = "") {
    TabStateFlusherInternal.resolve(browser, flushID, success, message);
  },

  /**
   * Resolves all active flush requests for a given browser. This should be
   * used when the content process crashed or the final update message was
   * seen. In those cases we can't guarantee to ever hear back from the frame
   * script so we just resolve all requests instead of discarding them.
   *
   * @param browser (<xul:browser>)
   *        The browser for which all flushes are being resolved.
   * @param success (bool, optional)
   *        Whether or not the flushes succeeded.
   * @param message (string, optional)
   *        An error message that will be sent to the Console in the
   *        event that the flushes failed.
   */
  resolveAll(browser, success = true, message = "") {
    TabStateFlusherInternal.resolveAll(browser, success, message);
  }
});

var TabStateFlusherInternal = {
  // Stores the last request ID.
  _lastRequestID: 0,

  // A map storing all active requests per browser.
  _requests: new WeakMap(),

  /**
   * Requests an async flush for the given browser. Returns a promise that will
   * resolve when we heard back from the content process and the parent has
   * all the latest data.
   */
  flush(browser) {
    let id = ++this._lastRequestID;
    let mm = browser.messageManager;
    mm.sendAsyncMessage("SessionStore:flush", {id});

    // Retrieve active requests for given browser.
    let permanentKey = browser.permanentKey;
    let perBrowserRequests = this._requests.get(permanentKey) || new Map();

    return new Promise(resolve => {
      // Store resolve() so that we can resolve the promise later.
      perBrowserRequests.set(id, resolve);

      // Update the flush requests stored per browser.
      this._requests.set(permanentKey, perBrowserRequests);
    });
  },

  /**
   * Requests an async flush for all non-lazy browsers of a given window.
   * Returns a Promise that will resolve when we've heard back from all browsers.
   */
  flushWindow(window) {
    let promises = [];
    for (let browser of window.gBrowser.browsers) {
      if (window.gBrowser.getTabForBrowser(browser).linkedPanel) {
        promises.push(this.flush(browser));
      }
    }
    return Promise.all(promises);
  },

  /**
   * Resolves the flush request with the given flush ID.
   *
   * @param browser (<xul:browser>)
   *        The browser for which the flush is being resolved.
   * @param flushID (int)
   *        The ID of the flush that was sent to the browser.
   * @param success (bool, optional)
   *        Whether or not the flush succeeded.
   * @param message (string, optional)
   *        An error message that will be sent to the Console in the
   *        event that a flush failed.
   */
  resolve(browser, flushID, success = true, message = "") {
    // Nothing to do if there are no pending flushes for the given browser.
    if (!this._requests.has(browser.permanentKey)) {
      return;
    }

    // Retrieve active requests for given browser.
    let perBrowserRequests = this._requests.get(browser.permanentKey);
    if (!perBrowserRequests.has(flushID)) {
      return;
    }

    if (!success) {
      Cu.reportError("Failed to flush browser: " + message);
    }

    // Resolve the request with the given id.
    let resolve = perBrowserRequests.get(flushID);
    perBrowserRequests.delete(flushID);
    resolve(success);
  },

  /**
   * Resolves all active flush requests for a given browser. This should be
   * used when the content process crashed or the final update message was
   * seen. In those cases we can't guarantee to ever hear back from the frame
   * script so we just resolve all requests instead of discarding them.
   *
   * @param browser (<xul:browser>)
   *        The browser for which all flushes are being resolved.
   * @param success (bool, optional)
   *        Whether or not the flushes succeeded.
   * @param message (string, optional)
   *        An error message that will be sent to the Console in the
   *        event that the flushes failed.
   */
  resolveAll(browser, success = true, message = "") {
    // Nothing to do if there are no pending flushes for the given browser.
    if (!this._requests.has(browser.permanentKey)) {
      return;
    }

    // Retrieve active requests for given browser.
    let perBrowserRequests = this._requests.get(browser.permanentKey);

    if (!success) {
      Cu.reportError("Failed to flush browser: " + message);
    }

    // Resolve all requests.
    for (let resolve of perBrowserRequests.values()) {
      resolve(success);
    }

    // Clear active requests.
    perBrowserRequests.clear();
  }
};
