/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXAMPLE_URL = "http://example.com/browser/dom/workers/test/";
const FRAME_SCRIPT_URL = getRootDirectory(gTestPath) + "frame_script.js";

/**
 * Add a tab with given `url`, and load a frame script in it. Returns a promise
 * that will be resolved when the tab finished loading.
 */
function addTab(url) {
  let tab = BrowserTestUtils.addTab(gBrowser, TAB_URL);
  gBrowser.selectedTab = tab;
  let linkedBrowser = tab.linkedBrowser;
  linkedBrowser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
  return new Promise(function (resolve) {
    linkedBrowser.addEventListener("load", function() {
      resolve(tab);
    }, {capture: true, once: true});
  });
}

/**
 * Remove the given `tab`.
 */
function removeTab(tab) {
  gBrowser.removeTab(tab);
}

let nextId = 0;

/**
 * Send a JSON RPC request to the frame script in the given `tab`, invoking the
 * given `method` with the given `params`. Returns a promise that will be
 * resolved with the result of the invocation.
 */
function jsonrpc(tab, method, params) {
  let currentId = nextId++;
  let messageManager = tab.linkedBrowser.messageManager;
  messageManager.sendAsyncMessage("jsonrpc", {
    id: currentId,
    method: method,
    params: params
  });
  return new Promise(function (resolve, reject) {
    messageManager.addMessageListener("jsonrpc", function listener(event) {
      let { id, result, error } = event.data;
      if (id !== currentId) {
        return;
      }
      messageManager.removeMessageListener("jsonrpc", listener);
      if (error) {
        reject(error);
        return;
      }
      resolve(result);
    });
  });
}

/**
 * Create a worker with the given `url` in the given `tab`.
 */
function createWorkerInTab(tab, url) {
  return jsonrpc(tab, "createWorker", [url]);
}

/**
 * Terminate the worker with the given `url` in the given `tab`.
 */
function terminateWorkerInTab(tab, url) {
  return jsonrpc(tab, "terminateWorker", [url]);
}

/**
 * Post the given `message` to the worker with the given `url` in the given
 * `tab`.
 */
function postMessageToWorkerInTab(tab, url, message) {
  return jsonrpc(tab, "postMessageToWorker", [url, message]);
}

/**
 * Disable the cache in the given `tab`.
 */
function disableCacheInTab(tab) {
  return jsonrpc(tab, "disableCache", []);
}
