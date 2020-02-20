/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the result/url loading functionality of UrlbarController.
 */

"use strict";

let sandbox;

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  ResetProfile: "resource://gre/modules/ResetProfile.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
  UrlbarController: "resource:///modules/UrlbarController.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarQueryContext: "resource:///modules/UrlbarUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

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
 * @param {function} condition
 *   A callback that returns true after Top Sites are successfully updated.
 * @param {boolean} searchShortcuts
 *   True if Top Sites search shortcuts should be enabled.
 */
async function updateTopSites(condition, searchShortcuts = false) {
  // Toggle the pref to clear the feed cache and force an update.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtabpage.activity-stream.feeds.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.topsites", true],
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
 * Starts a search that should trigger a tip, picks the tip, and waits for the
 * tip's action to happen.
 *
 * @param {string} searchString
 *   The search string.
 * @param {TIPS} tip
 *   The expected tip type.
 * @param {string} title
 *   The expected tip title.
 * @param {string} button
 *   The expected button title.
 * @param {function} awaitCallback
 *   A function that checks the tip's action.  Should return a promise (or be
 *   async).
 * @returns {*}
 *   The value returned from `awaitCallback`.
 */
function checkIntervention({
  searchString,
  tip,
  title,
  button,
  awaitCallback,
} = {}) {
  // Opening modal dialogs confuses focus on Linux just after them, thus run
  // these checks in separate tabs to better isolate them.
  return BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do a search that triggers the tip.
    let [result, element] = await awaitTip(searchString);
    Assert.strictEqual(result.payload.type, tip);
    await element.ownerDocument.l10n.translateFragment(element);

    let actualTitle = element._elements.get("title").textContent;
    if (typeof title == "string") {
      Assert.equal(actualTitle, title, "Title string");
    } else {
      // regexp
      Assert.ok(title.test(actualTitle), "Title regexp");
    }

    let actualButton = element._elements.get("tipButton").textContent;
    if (typeof button == "string") {
      Assert.equal(actualButton, button, "Button string");
    } else {
      // regexp
      Assert.ok(button.test(actualButton), "Button regexp");
    }

    Assert.ok(BrowserTestUtils.is_visible(element._elements.get("helpButton")));

    let values = await Promise.all([awaitCallback(), pickTip()]);
    Assert.ok(true, "Refresh dialog opened");

    return values[0] || null;
  });
}

/**
 * Starts a search and asserts that the second result is a tip.
 *
 * @param {string} searchString
 *   The search string.
 * @param {Window} win
 * @returns {[result, element]}
 *   The result and its element in the DOM.
 */
async function awaitTip(searchString, win = window) {
  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: searchString,
    waitForFocus,
    fireInputEvent: true,
  });
  Assert.ok(context.results.length >= 2);
  let result = context.results[1];
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TIP);
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(win, 1);
  return [result, element];
}

/**
 * Starts a search and asserts that there are no tips.
 *
 * @param {string} searchString
 *   The search string.
 * @param {Window} win
 */
async function awaitNoTip(searchString, win = window) {
  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: searchString,
    waitForFocus,
    fireInputEvent: true,
  });
  for (let result of context.results) {
    Assert.notEqual(result.type, UrlbarUtils.RESULT_TYPE.TIP);
  }
}

/**
 * Picks the current tip's button.  The view should be open and the second
 * result should be a tip.
 */
async function pickTip() {
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  let button = result.element.row._elements.get("tipButton");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(button, {});
  });
}

/**
 * Sets up the profile so that it can be reset.
 */
function makeProfileResettable() {
  // Make reset possible.
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );
  let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileName = "mochitest-test-profile-temp-" + Date.now();
  let tempProfile = profileService.createProfile(
    currentProfileDir,
    profileName
  );
  Assert.ok(
    ResetProfile.resetSupported(),
    "Should be able to reset from mochitest's temporary profile once it's in the profile manager."
  );

  registerCleanupFunction(() => {
    tempProfile.remove(false);
    Assert.ok(
      !ResetProfile.resetSupported(),
      "Shouldn't be able to reset from mochitest's temporary profile once removed from the profile manager."
    );
  });
}

/**
 * Copied from BrowserTestUtils.jsm, but lets you listen for any one of multiple
 * dialog URIs instead of only one.
 * @param {string} buttonAction
 *   What button should be pressed on the alert dialog.
 * @param {array} uris
 *   The URIs for the alert dialogs.
 * @param {function} [func]
 *   An optional callback.
 */
async function promiseAlertDialogOpen(buttonAction, uris, func) {
  let win = await BrowserTestUtils.domWindowOpened(null, async aWindow => {
    // The test listens for the "load" event which guarantees that the alert
    // class has already been added (it is added when "DOMContentLoaded" is
    // fired).
    await BrowserTestUtils.waitForEvent(aWindow, "load");

    return uris.includes(aWindow.document.documentURI);
  });

  if (func) {
    await func(win);
    return win;
  }

  let dialog = win.document.querySelector("dialog");
  dialog.getButton(buttonAction).click();

  return win;
}

/**
 * Copied from BrowserTestUtils.jsm, but lets you listen for any one of multiple
 * dialog URIs instead of only one.
 * @param {string} buttonAction
 *   What button should be pressed on the alert dialog.
 * @param {array} uris
 *   The URIs for the alert dialogs.
 * @param {function} [func]
 *   An optional callback.
 */
async function promiseAlertDialog(buttonAction, uris, func) {
  let win = await promiseAlertDialogOpen(buttonAction, uris, func);
  return BrowserTestUtils.windowClosed(win);
}
