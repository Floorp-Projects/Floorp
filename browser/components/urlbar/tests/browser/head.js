/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the result/url loading functionality of UrlbarController.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  PromptTestUtils: "resource://testing-common/PromptTestUtils.jsm",
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  ResetProfile: "resource://gre/modules/ResetProfile.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
  UrlbarView: "resource:///modules/UrlbarView.jsm",
});

let sandbox;

/* import-globals-from head-common.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-common.js",
  this
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

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
 * Updates the Top Sites feed.
 *
 * @param {function} condition
 *   A callback that returns true after Top Sites are successfully updated.
 * @param {boolean} searchShortcuts
 *   True if Top Sites search shortcuts should be enabled.
 */
async function updateTopSites(condition, searchShortcuts = false) {
  // Toggle the pref to clear the feed cache and force an update.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtabpage.activity-stream.feeds.system.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.system.topsites", true],
      [
        "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
        searchShortcuts,
      ],
    ],
  });

  // Wait for the feed to be updated.
  await TestUtils.waitForCondition(() => {
    let sites = AboutNewTab.getTopSites();
    return condition(sites);
  }, "Waiting for top sites to be updated");
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
