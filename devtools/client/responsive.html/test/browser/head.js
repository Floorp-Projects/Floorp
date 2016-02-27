/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../framework/test/shared-head.js */
/* import-globals-from ../../../framework/test/shared-redux-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-redux-head.js",
  this);

Services.prefs.setBoolPref("devtools.responsive.html.enabled", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.responsive.html.enabled");
});
const { ResponsiveUIManager } = Cu.import("resource://devtools/client/responsivedesign/responsivedesign.jsm", {});

/**
 * Open responsive design mode for the given tab.
 */
var openRDM = Task.async(function*(tab) {
  info("Opening responsive design mode");
  let manager = ResponsiveUIManager;
  let ui = yield manager.openIfNeeded(window, tab);
  info("Responsive design mode opened");
  return { ui, manager };
});

/**
 * Close responsive design mode for the given tab.
 */
var closeRDM = Task.async(function*(tab) {
  info("Closing responsive design mode");
  let manager = ResponsiveUIManager;
  manager.closeIfNeeded(window, tab);
  info("Responsive design mode closed");
});

/**
 * Adds a new test task that adds a tab with the given URL, opens responsive
 * design mode, runs the given generator, closes responsive design mode, and
 * removes the tab.
 *
 * Example usage:
 *
 *   addRDMTask(TEST_URL, function*({ ui, manager }) {
 *     // Your tests go here...
 *   });
 */
function addRDMTask(url, generator) {
  add_task(function*() {
    const tab = yield addTab(url);
    const results = yield openRDM(tab);

    try {
      yield* generator(results);
    } catch (err) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(err));
    }

    yield closeRDM(tab);
    yield removeTab(tab);
  });
}

var waitForFrameLoad = Task.async(function*(frame, targetURL) {
  let window = frame.contentWindow;
  if ((window.document.readyState == "complete" ||
       window.document.readyState == "interactive") &&
      window.location.href == targetURL) {
    return;
  }
  yield once(frame, "load");
});
