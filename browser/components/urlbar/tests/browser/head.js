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

XPCOMUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.import(
    "resource://testing-common/UrlbarTestUtils.jsm"
  );
  module.init(this);
  return module;
});

XPCOMUtils.defineLazyGetter(this, "SearchTestUtils", () => {
  const { SearchTestUtils: module } = ChromeUtils.import(
    "resource://testing-common/SearchTestUtils.jsm"
  );
  module.init(this);
  return module;
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

/**
 * Asserts a result is a quick suggest result.
 *
 * @param {string} sponsoredURL
 *   The expected sponsored URL.
 * @param {string} nonsponsoredURL
 *   The expected nonsponsored URL.
 * @param {number} [index]
 *   The expected index of the quick suggest result. Pass -1 to use the index
 *   of the last result.
 * @param {boolean} [isSponsored]
 *   True if the result is expected to be sponsored and false if non-sponsored.
 * @param {object} [win]
 * @returns {result}
 *   The quick suggest result.
 */
async function assertIsQuickSuggest({
  sponsoredURL,
  nonsponsoredURL,
  index = -1,
  isSponsored = true,
  win = window,
} = {}) {
  if (index < 0) {
    index = UrlbarTestUtils.getResultCount(win) - 1;
    Assert.greater(index, -1, "Sanity check: Result count should be > 0");
  }

  let result = await UrlbarTestUtils.getDetailsOfResultAt(win, index);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.isSponsored, isSponsored, "Result isSponsored");

  let url;
  let actionText;
  if (isSponsored) {
    url = sponsoredURL;
    actionText = "Sponsored";
  } else {
    url = nonsponsoredURL;
    actionText = "";
  }
  Assert.equal(result.url, url, "Result URL");
  Assert.equal(
    result.element.row._elements.get("action").textContent,
    actionText,
    "Result action text"
  );

  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "The help button should be present");

  return result;
}
