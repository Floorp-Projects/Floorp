/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { interfaces: Ci } = Components;

let workers = {};

let methods = {
  /**
   * Create a worker with the given `url` in this tab.
   */
  createWorker: function (url) {
    dump("Frame script: creating worker with url '" + url + "'\n");

    workers[url] = new content.Worker(url);
    return Promise.resolve();
  },

  /**
   * Terminate the worker with the given `url` in this tab.
   */
  terminateWorker: function (url) {
    dump("Frame script: terminating worker with url '" + url + "'\n");

    workers[url].terminate();
    delete workers[url];
    return Promise.resolve();
  },

  /**
   * Post the given `message` to the worker with the given `url` in this tab.
   */
  postMessageToWorker: function (url, message) {
    dump("Frame script: posting message to worker with url '" + url + "'\n");

    let worker = workers[url];
    worker.postMessage(message);
    return new Promise(function (resolve) {
      worker.onmessage = function (event) {
        worker.onmessage = null;
        resolve(event.data);
      };
    });
  },

  /**
   * Disable the cache for this tab.
   */
  disableCache: function () {
    docShell.defaultLoadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE
                              | Ci.nsIRequest.INHIBIT_CACHING;
  }
};

addMessageListener("jsonrpc", function (event) {
  let { id, method, params } = event.data;
  Promise.resolve().then(function () {
    return methods[method].apply(undefined, params);
  }).then(function (result) {
    sendAsyncMessage("jsonrpc", {
      id: id,
      result: result
    });
  }).catch(function (error) {
    sendAsyncMessage("jsonrpc", {
      id: id,
      error: error.toString()
    });
  });
});
