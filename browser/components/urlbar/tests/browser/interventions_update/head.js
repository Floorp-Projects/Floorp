/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This directory contains tests that check the update-related interventions.
// We mock updates by using the test helpers in
// toolkit/mozapps/update/tests/browser.
//
// If you're running these tests and you get an error like this:
//
// FAIL head.js import threw an exception - Error opening input stream (invalid filename?): chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/head.js
//
// Then run `mach test toolkit/mozapps/update/tests/browser` first.  You can
// stop mach as soon as it starts the first test, but this is necessary so that
// mach builds the update tests in your objdir.

"use strict";

/* import-globals-from ../../../../../../toolkit/mozapps/update/tests/browser/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/head.js",
  this
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ResetProfile: "resource://gre/modules/ResetProfile.jsm",
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

// The types of intervention tips.
const TIPS = {
  NONE: "",
  CLEAR: "clear",
  REFRESH: "refresh",
  UPDATE_RESTART: "update_restart",
  UPDATE_ASK: "update_ask",
  UPDATE_REFRESH: "update_refresh",
  UPDATE_WEB: "update_web",
};

// For each intervention type, a search string that trigger the intervention.
const SEARCH_STRINGS = {
  CLEAR: "firefox history",
  REFRESH: "firefox slow",
  UPDATE: "firefox update",
};

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update1.interventions", true]],
  });
  registerCleanupFunction(() => {
    // We need to reset the provider's appUpdater.status between tests so that
    // each test doesn't interfere with the next.
    UrlbarProviderInterventions.resetAppUpdater();
  });
});

/**
 * Initializes a mock app update.  Adapted from runAboutDialogUpdateTest:
 * https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/head.js
 *
 * @param {object} params
 *   See the files in toolkit/mozapps/update/tests/browser.
 */
async function initUpdate(params) {
  gEnv.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
      [PREF_APP_UPDATE_URL_MANUAL, gDetailsURL],
    ],
  });

  await setupTestUpdater();

  let queryString = params.queryString ? params.queryString : "";
  let updateURL =
    URL_HTTP_UPDATE_SJS +
    "?detailsURL=" +
    gDetailsURL +
    queryString +
    getVersionParams();
  if (params.backgroundUpdate) {
    setUpdateURL(updateURL);
    gAUS.checkForBackgroundUpdates();
    if (params.continueFile) {
      await continueFileHandler(params.continueFile);
    }
    if (params.waitForUpdateState) {
      await TestUtils.waitForCondition(
        () =>
          gUpdateManager.activeUpdate &&
          gUpdateManager.activeUpdate.state == params.waitForUpdateState,
        "Waiting for update state: " + params.waitForUpdateState,
        undefined,
        200
      ).catch(e => {
        // Instead of throwing let the check below fail the test so the panel
        // ID and the expected panel ID is printed in the log.
        logTestInfo(e);
      });
      // Display the UI after the update state equals the expected value.
      Assert.equal(
        gUpdateManager.activeUpdate.state,
        params.waitForUpdateState,
        "The update state value should equal " + params.waitForUpdateState
      );
    }
  } else {
    updateURL += "&slowUpdateCheck=1&useSlowDownloadMar=1";
    setUpdateURL(updateURL);
  }
}

/**
 * Performs steps in a mock update.  Adapted from runAboutDialogUpdateTest:
 * https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/head.js
 *
 * @param {array} steps
 *   See the files in toolkit/mozapps/update/tests/browser.
 */
async function processUpdateSteps(steps) {
  for (let step of steps) {
    await processUpdateStep(step);
  }
}

/**
 * Performs a step in a mock update.  Adapted from runAboutDialogUpdateTest:
 * https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/head.js
 *
 * @param {object} step
 *   See the files in toolkit/mozapps/update/tests/browser.
 */
async function processUpdateStep(step) {
  if (typeof step == "function") {
    step();
    return;
  }

  const { panelId, checkActiveUpdate, continueFile, downloadInfo } = step;
  if (checkActiveUpdate) {
    await TestUtils.waitForCondition(
      () => gUpdateManager.activeUpdate,
      "Waiting for active update"
    );
    Assert.ok(
      !!gUpdateManager.activeUpdate,
      "There should be an active update"
    );
    Assert.equal(
      gUpdateManager.activeUpdate.state,
      checkActiveUpdate.state,
      "The active update state should equal " + checkActiveUpdate.state
    );
  } else {
    Assert.ok(
      !gUpdateManager.activeUpdate,
      "There should not be an active update"
    );
  }

  if (panelId == "downloading") {
    for (let i = 0; i < downloadInfo.length; ++i) {
      let data = downloadInfo[i];
      // The About Dialog tests always specify a continue file.
      await continueFileHandler(continueFile);
      let patch = getPatchOfType(data.patchType);
      // The update is removed early when the last download fails so check
      // that there is a patch before proceeding.
      let isLastPatch = i == downloadInfo.length - 1;
      if (!isLastPatch || patch) {
        let resultName = data.bitsResult ? "bitsResult" : "internalResult";
        patch.QueryInterface(Ci.nsIWritablePropertyBag);
        await TestUtils.waitForCondition(
          () => patch.getProperty(resultName) == data[resultName],
          "Waiting for expected patch property " +
            resultName +
            " value: " +
            data[resultName],
          undefined,
          200
        ).catch(e => {
          // Instead of throwing let the check below fail the test so the
          // property value and the expected property value is printed in
          // the log.
          logTestInfo(e);
        });
        Assert.equal(
          patch.getProperty(resultName),
          data[resultName],
          "The patch property " +
            resultName +
            " value should equal " +
            data[resultName]
        );
      }
    }
  } else if (continueFile) {
    await continueFileHandler(continueFile);
  }
}

/**
 * Checks an intervention tip.  This works by starting a search that should
 * trigger a tip, picks the tip, and waits for the tip's action to happen.
 *
 * @param {string} searchString
 *   The search string.
 * @param {string} tip
 *   The expected tip type.
 * @param {string/regexp} title
 *   The expected tip title.
 * @param {string/regexp} button
 *   The expected button title.
 * @param {function} awaitCallback
 *   A function that checks the tip's action.  Should return a promise (or be
 *   async).
 * @returns {object}
 *   The value returned from `awaitCallback`.
 */
async function doUpdateTest({
  searchString,
  tip,
  title,
  button,
  awaitCallback,
} = {}) {
  // Do a search that triggers the tip.
  let [result, element] = await awaitTip(searchString);

  Assert.strictEqual(result.payload.type, tip, "Tip type");

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

  Assert.ok(
    BrowserTestUtils.is_visible(element._elements.get("helpButton")),
    "Help button visible"
  );

  // Pick the tip and wait for the action.
  let values = await Promise.all([awaitCallback(), pickTip()]);

  return values[0] || null;
}

/**
 * Starts a search and asserts that the second result is a tip.
 *
 * @param {string} searchString
 *   The search string.
 * @param {window} win
 *   The window.
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
 * Waits for the quit-application-requested notification and cancels it (so that
 * the app isn't actually restarted).
 */
async function awaitAppRestartRequest() {
  await TestUtils.topicObserved(
    "quit-application-requested",
    (cancelQuit, data) => {
      if (data == "restart") {
        cancelQuit.QueryInterface(Ci.nsISupportsPRBool).data = true;
        return true;
      }
      return false;
    }
  );
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
