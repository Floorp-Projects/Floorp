/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the result/url loading functionality of UrlbarController.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
  PromptTestUtils: "resource://testing-common/PromptTestUtils.sys.mjs",
  ResetProfile: "resource://gre/modules/ResetProfile.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarController: "resource:///modules/UrlbarController.sys.mjs",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "PlacesFrecencyRecalculator", () => {
  return Cc["@mozilla.org/places/frecency-recalculator;1"].getService(
    Ci.nsIObserver
  ).wrappedJSObject;
});

let sandbox;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-common.js",
  this
);

registerCleanupFunction(async () => {
  // Ensure the Urlbar popup is always closed at the end of a test, to save having
  // to do it within each test.
  await UrlbarTestUtils.promisePopupClose(window);
});

async function selectAndPaste(str, win = window) {
  await SimpleTest.promiseClipboardChange(str, () => {
    clipboardHelper.copyString(str);
  });
  win.gURLBar.select();
  win.document.commandDispatcher
    .getControllerForCommand("cmd_paste")
    .doCommand("cmd_paste");
}

/**
 * Waits for a load in any browser or a timeout, whichever comes first.
 *
 * @param {window} win
 *   The top-level browser window to listen in.
 * @param {number} timeoutMs
 *   The timeout in ms.
 * @returns {event|null}
 *   If a load event was detected before the timeout fired, then the event is
 *   returned.  event.target will be the browser in which the load occurred.  If
 *   the timeout fired before a load was detected, null is returned.
 */
async function waitForLoadOrTimeout(win = window, timeoutMs = 1000) {
  let event;
  let listener;
  let timeout;
  let eventName = "BrowserTestUtils:ContentEvent:load";
  try {
    event = await Promise.race([
      new Promise(resolve => {
        listener = resolve;
        win.addEventListener(eventName, listener, true);
      }),
      new Promise(resolve => {
        timeout = win.setTimeout(resolve, timeoutMs);
      }),
    ]);
  } finally {
    win.removeEventListener(eventName, listener, true);
    win.clearTimeout(timeout);
  }
  return event || null;
}
