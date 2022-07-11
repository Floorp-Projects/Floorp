/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabStateFlusher"];

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm"
);

/**
 * A module that enables async flushes. Updates from frame scripts are
 * throttled to be sent only once per second. If an action wants a tab's latest
 * state without waiting for a second then it can request an async flush and
 * wait until the frame scripts reported back. At this point the parent has the
 * latest data and the action can continue.
 */
var TabStateFlusher = Object.freeze({
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
  },
});

var TabStateFlusherInternal = {
  // Stores the last request ID.
  _lastRequestID: 0,

  // A map storing all active requests per browser. A request is a
  // triple of a map containing all flush requests, a promise that
  // resolve when a request for a browser is canceled, and the
  // function to call to cancel a reqeust.
  _requests: new WeakMap(),

  initEntry(entry) {
    entry.perBrowserRequests = new Map();
    entry.cancelPromise = new Promise(resolve => {
      entry.cancel = resolve;
    }).then(result => {
      TabStateFlusherInternal.initEntry(entry);
      return result;
    });

    return entry;
  },

  /**
   * Requests an async flush for the given browser. Returns a promise that will
   * resolve when we heard back from the content process and the parent has
   * all the latest data.
   */
  flush(browser) {
    let id = ++this._lastRequestID;
    let nativePromise = Promise.resolve();
    if (browser && browser.frameLoader) {
      /*
        Request native listener to flush the tabState.
        Resolves when flush is complete.
      */
      nativePromise = browser.frameLoader.requestTabStateFlush();
    }

    if (!Services.appinfo.sessionHistoryInParent) {
      /*
        In the event that we have to trigger a process switch and thus change
        browser remoteness, session store needs to register and track the new
        browser window loaded and to have message manager listener registered
        ** before ** TabStateFlusher send "SessionStore:flush" message. This fixes
        the race where we send the message before the message listener is
        registered for it.
        */
      lazy.SessionStore.ensureInitialized(browser.ownerGlobal);

      let mm = browser.messageManager;
      mm.sendAsyncMessage("SessionStore:flush", {
        id,
        epoch: lazy.SessionStore.getCurrentEpoch(browser),
      });
    }

    // Retrieve active requests for given browser.
    let permanentKey = browser.permanentKey;
    let request = this._requests.get(permanentKey);
    if (!request) {
      // If we don't have any requests for this browser, create a new
      // entry for browser.
      request = this.initEntry({});
      this._requests.set(permanentKey, request);
    }

    // Non-SHIP flushes resolve this after the "SessionStore:update" message. We
    // don't use that message for SHIP, so it's fine to resolve the request
    // immediately after the native promise resolves, since SessionStore will
    // have processed all updates from this browser by that point.
    let requestPromise = Promise.resolve();
    if (!Services.appinfo.sessionHistoryInParent) {
      requestPromise = new Promise(resolve => {
        // Store resolve() so that we can resolve the promise later.
        request.perBrowserRequests.set(id, resolve);
      });
    }

    return Promise.race([
      nativePromise.then(_ => requestPromise),
      request.cancelPromise,
    ]);
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
    let { perBrowserRequests } = this._requests.get(browser.permanentKey);
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

    // Retrieve the cancel function for a given browser.
    let { cancel } = this._requests.get(browser.permanentKey);

    if (!success) {
      Cu.reportError("Failed to flush browser: " + message);
    }

    // Resolve all requests.
    cancel(success);
  },
};
