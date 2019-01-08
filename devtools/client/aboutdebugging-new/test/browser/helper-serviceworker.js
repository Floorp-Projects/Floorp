/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

/**
 * Temporarily flip all the preferences necessary for service worker testing.
 */
async function enableServiceWorkerDebugging() {
  // Enable service workers.
  await pushPref("dom.serviceWorkers.enabled", true);

  // Accept workers from mochitest's http (normally only available in https).
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  // Force single content process. Necessary until sw e10s refactor is done (Bug 1231208).
  await pushPref("dom.ipc.processCount", 1);
  Services.ppmm.releaseCachedProcesses();
}
/* exported enableServiceWorkerDebugging */

/**
 * Helper to listen once on a message sent using postMessage from the provided tab.
 *
 * @param {Tab} tab
 *        The tab on which the message will be received.
 * @param {String} message
 *        The name of the expected message.
 */
function onTabMessage(tab, message) {
  const mm = tab.linkedBrowser.messageManager;
  return new Promise(resolve => {
    mm.addMessageListener(message, function listener() {
      mm.removeMessageListener(message, listener);
      resolve();
    });
  });
}
/* exported onTabMessage */

async function waitForServiceWorkerRunning(workerText, document) {
  await waitUntil(() => {
    const target = findDebugTargetByText(workerText, document);
    const status = target && target.querySelector(".js-worker-status");
    return status && status.textContent === "Running";
  });

  return findDebugTargetByText(workerText, document);
}
/* exported waitForServiceWorkerRunning */

/**
 * Helper to listen once on a message sent using postMessage from the provided tab.
 *
 * @param {Tab} tab
 *        The tab on which the message will be received.
 * @param {String} message
 *        The name of the expected message.
 */
function forwardServiceWorkerMessage(tab) {
  info("Make the test page notify us when the service worker sends a message.");
  return ContentTask.spawn(tab.linkedBrowser, {}, function() {
    const win = content.wrappedJSObject;
    win.navigator.serviceWorker.addEventListener("message", function(event) {
      sendAsyncMessage(event.data);
    });
  });
}
/* exported forwardServiceWorkerMessage */

/**
 * Unregister the service worker from the content page. The content page should define
 * `getRegistration` to allow this helper to retrieve the service worker registration that
 * should be unregistered.
 *
 * @param {Tab} tab
 *        The tab on which the service worker should be removed.
 */
async function unregisterServiceWorker(tab) {
  return ContentTask.spawn(tab.linkedBrowser, {}, function() {
    const win = content.wrappedJSObject;
    // Check that the content page defines getRegistration.
    is(typeof win.getRegistration, "function", "getRegistration is a valid function");
    win.getRegistration().unregister();
  });
}
/* exported unregisterServiceWorker */
