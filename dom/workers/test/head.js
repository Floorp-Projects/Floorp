/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Add a tab with given `url`. Returns a promise
 * that will be resolved when the tab finished loading.
 */
function addTab(url) {
  return BrowserTestUtils.openNewForegroundTab(gBrowser, TAB_URL);
}

/**
 * Remove the given `tab`.
 */
function removeTab(tab) {
  gBrowser.removeTab(tab);
}

/**
 * Create a worker with the given `url` in the given `tab`.
 */
function createWorkerInTab(tab, url) {
  info("Creating worker with url '" + url + "'\n");
  return SpecialPowers.spawn(tab.linkedBrowser, [url], urlChild => {
    if (!content._workers) {
      content._workers = {};
    }
    content._workers[urlChild] = new content.Worker(urlChild);
  });
}

/**
 * Terminate the worker with the given `url` in the given `tab`.
 */
function terminateWorkerInTab(tab, url) {
  info("Terminating worker with url '" + url + "'\n");
  return SpecialPowers.spawn(tab.linkedBrowser, [url], urlChild => {
    content._workers[urlChild].terminate();
    delete content._workers[urlChild];
  });
}

/**
 * Post the given `message` to the worker with the given `url` in the given
 * `tab`.
 */
function postMessageToWorkerInTab(tab, url, message) {
  info("Posting message to worker with url '" + url + "'\n");
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [url, message],
    (urlChild, messageChild) => {
      let worker = content._workers[urlChild];
      worker.postMessage(messageChild);
      return new Promise(function (resolve) {
        worker.onmessage = function (event) {
          worker.onmessage = null;
          resolve(event.data);
        };
      });
    }
  );
}

/**
 * Disable the cache in the given `tab`.
 */
function disableCacheInTab(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.docShell.defaultLoadFlags =
      Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING;
  });
}
