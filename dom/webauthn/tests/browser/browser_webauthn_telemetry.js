/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Return the scalars from the parent-process.
function getParentProcessScalars(aChannel, aKeyed = false, aClear = false) {
  const scalars = aKeyed ?
    Services.telemetry.snapshotKeyedScalars(aChannel, aClear)["parent"] :
    Services.telemetry.snapshotScalars(aChannel, aClear)["parent"];
  return scalars || {};
}

function getTelemetryForScalar(aName) {
  let scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT, true);
  return scalars[aName] || 0;
}

function cleanupTelemetry() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById("WEBAUTHN_CREATE_CREDENTIAL_MS").clear();
  Services.telemetry.getHistogramById("WEBAUTHN_GET_ASSERTION_MS").clear();
}

function validateHistogramEntryCount(aHistogramName, aExpectedCount) {
  let hist = Services.telemetry.getHistogramById(aHistogramName);
  let resultIndexes = hist.snapshot();

  let entriesSeen = 0;
  for (let i = 0; i < resultIndexes.counts.length; i++) {
    entriesSeen += resultIndexes.counts[i];
  }

  is(entriesSeen, aExpectedCount, "Expecting " + aExpectedCount + " histogram entries in " +
     aHistogramName);
}

// Loads a page, and expects to have an element ID "result" added to the DOM when the page is done.
async function executeTestPage(aUri) {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, aUri);
  try {
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
      let condition = () => content.document.getElementById("result");
      await ContentTaskUtils.waitForCondition(condition,
                                              "Waited too long for operation to finish on "
                                              + content.document.location);
    });
  } catch(e) {
    ok(false, "Exception thrown executing test page: " + e);
  } finally {
    // Remove all the extra windows and tabs.
    return BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
}

add_task(async function test_loopback() {
  // These tests can't run simultaneously as the preference changes will race.
  // So let's run them sequentially here, but in an async function so we can
  // use await.
  const testPage = "https://example.com/browser/dom/webauthn/tests/browser/frame_webauthn_success.html";
  {
    cleanupTelemetry();
    // Enable the soft token, and execute a simple end-to-end test
    Services.prefs.setBoolPref("security.webauth.webauthn", true);
    Services.prefs.setBoolPref("security.webauth.webauthn_enable_softtoken", true);
    Services.prefs.setBoolPref("security.webauth.webauthn_enable_usbtoken", false);

    await executeTestPage(testPage);

    let webauthn_used = getTelemetryForScalar("security.webauthn_used");
    ok(webauthn_used, "Scalar keys are set: " + Object.keys(webauthn_used).join(", "));
    is(webauthn_used["U2FRegisterFinish"], 1, "webauthn_used U2FRegisterFinish scalar should be 1");
    is(webauthn_used["U2FSignFinish"], 1, "webauthn_used U2FSignFinish scalar should be 1");
    is(webauthn_used["U2FSignAbort"], undefined, "webauthn_used U2FSignAbort scalar must be unset");
    is(webauthn_used["U2FRegisterAbort"], undefined, "webauthn_used U2FRegisterAbort scalar must be unset");

    validateHistogramEntryCount("WEBAUTHN_CREATE_CREDENTIAL_MS", 1);
    validateHistogramEntryCount("WEBAUTHN_GET_ASSERTION_MS", 1);
  }

  {
    cleanupTelemetry();
    // Same as test_successful_loopback, but we will swap to using a (non-existent)
    // usb token. This will cause U2FRegisterAbort to fire, but will not execute the
    // Sign function, and no histogram entries will log.
    Services.prefs.setBoolPref("security.webauth.webauthn", true);
    Services.prefs.setBoolPref("security.webauth.webauthn_enable_softtoken", false);
    Services.prefs.setBoolPref("security.webauth.webauthn_enable_usbtoken", true);

    await executeTestPage(testPage);

    let webauthn_used = getTelemetryForScalar("security.webauthn_used");
    ok(webauthn_used, "Scalar keys are set: " + Object.keys(webauthn_used).join(", "));
    is(webauthn_used["U2FRegisterFinish"], undefined, "webauthn_used U2FRegisterFinish must be unset");
    is(webauthn_used["U2FSignFinish"], undefined, "webauthn_used U2FSignFinish scalar must be unset");
    is(webauthn_used["U2FRegisterAbort"], 1, "webauthn_used U2FRegisterAbort scalar should be a 1");
    is(webauthn_used["U2FSignAbort"], undefined, "webauthn_used U2FSignAbort scalar must be unset");

    validateHistogramEntryCount("WEBAUTHN_CREATE_CREDENTIAL_MS", 0);
    validateHistogramEntryCount("WEBAUTHN_GET_ASSERTION_MS", 0);
  }

  // There aren't tests for register succeeding and sign failing, as I don't see an easy way to prompt
  // the soft token to fail that way _and_ trigger the Abort telemetry.
});
