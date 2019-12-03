/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This directory contains tests that check the update-related functions of
// browser.experiments.urlbar.  We mock updates by using the test helpers in
// toolkit/mozapps/update/tests/browser.

"use strict";

/* import-globals-from ../../../../../../../toolkit/mozapps/update/tests/browser/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/head.js",
  this
);

const SCHEMA_BASENAME = "schema.json";
const SCRIPT_BASENAME = "api.js";

const SCHEMA_PATH = getTestFilePath(SCHEMA_BASENAME);
const SCRIPT_PATH = getTestFilePath(SCRIPT_BASENAME);

let schemaSource;
let scriptSource;

add_task(async function loadSource() {
  schemaSource = await (await fetch("file://" + SCHEMA_PATH)).text();
  scriptSource = await (await fetch("file://" + SCRIPT_PATH)).text();
});

/**
 * Loads a mock extension with our browser.experiments.urlbar API and a
 * background script.  Be sure to call `await ext.unload()` when you're done
 * with it.
 *
 * @param {function} background
 *   This function is serialized and becomes the background script.
 * @returns {object}
 *   The extension.
 */
async function loadExtension(background) {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["urlbar"],
      experiment_apis: {
        experiments_urlbar: {
          schema: SCHEMA_BASENAME,
          parent: {
            scopes: ["addon_parent"],
            paths: [["experiments", "urlbar"]],
            script: SCRIPT_BASENAME,
          },
        },
      },
    },
    files: {
      [SCHEMA_BASENAME]: schemaSource,
      [SCRIPT_BASENAME]: scriptSource,
    },
    isPrivileged: true,
    background,
  });
  await ext.startup();
  return ext;
}

/**
 * The getBrowserUpdateStatus tests are all similar.  Use this to add a test
 * task that sets up a mock browser update and loads an extension that checks
 * for updates and gets the update status.
 *
 * @param {object} params
 *   See the files in toolkit/mozapps/update/tests/browser.
 * @param {array} steps
 *   See the files in toolkit/mozapps/update/tests/browser.
 * @param {string} expectedUpdateStatus
 *   One of the BrowserUpdateStatus enums.
 * @param {function} [setUp]
 *   An optional setup functon that will be called first.
 */
function add_getBrowserUpdateStatus_task(
  params,
  steps,
  expectedUpdateStatus,
  setUp
) {
  add_task(async function() {
    if (setUp) {
      let skip = await setUp();
      if (skip) {
        Assert.ok(true, "Skipping test");
        return;
      }
    }

    await initUpdate(params);

    let ext = await loadExtension(async () => {
      browser.test.onMessage.addListener(async command => {
        switch (command) {
          case "check":
            await browser.experiments.urlbar.checkForBrowserUpdate();
            browser.test.sendMessage("done");
            break;
          case "get": {
            let done = false;
            let interval = setInterval(async () => {
              let status = await browser.experiments.urlbar.getBrowserUpdateStatus();
              if (status != "checking" && !done) {
                done = true;
                clearInterval(interval);
                browser.test.sendMessage("done", status);
              }
            }, 100);
            break;
          }
        }
      });
    });

    ext.sendMessage("check");
    await ext.awaitMessage("done");

    await processUpdateSteps(steps);

    ext.sendMessage("get");
    let status = await ext.awaitMessage("done");
    Assert.equal(status, expectedUpdateStatus);

    await ext.unload();
  });
}

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
