/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1671122 - Fixed bug where second click on HTTPS-Only Mode enable-checkbox disables it again.
// https://bugzilla.mozilla.org/bug/1671122
"use strict";

const HTTPS_ONLY_ENABLED = "enabled";
const HTTPS_ONLY_PBM_ONLY = "privateOnly";
const HTTPS_ONLY_DISABLED = "disabled";

add_task(async function httpsOnlyRadioGroupIsWorking() {
  // Make sure HTTPS-Only mode is only enabled for PBM

  registerCleanupFunction(async function () {
    Services.prefs.clearUserPref("dom.security.https_only_mode");
    Services.prefs.clearUserPref("dom.security.https_only_mode_pbm");
  });

  await SpecialPowers.setBoolPref("dom.security.https_only_mode", false);
  await SpecialPowers.setBoolPref("dom.security.https_only_mode_pbm", true);

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  const doc = gBrowser.selectedBrowser.contentDocument;
  const radioGroup = doc.getElementById("httpsOnlyRadioGroup");
  const enableAllRadio = doc.getElementById("httpsOnlyRadioEnabled");
  const enablePbmRadio = doc.getElementById("httpsOnlyRadioEnabledPBM");
  const disableRadio = doc.getElementById("httpsOnlyRadioDisabled");

  // Check if UI
  check(radioGroup, HTTPS_ONLY_PBM_ONLY);

  // Check if UI updated on pref-change
  await SpecialPowers.setBoolPref("dom.security.https_only_mode_pbm", false);
  check(radioGroup, HTTPS_ONLY_DISABLED);

  // Check if prefs change if clicked on radio button
  enableAllRadio.click();
  check(radioGroup, HTTPS_ONLY_ENABLED);

  // Check if prefs stay the same if clicked on same
  // radio button again (see bug 1671122)
  enableAllRadio.click();
  check(radioGroup, HTTPS_ONLY_ENABLED);

  // Check if prefs are set correctly for PBM-only mode.
  enablePbmRadio.click();
  check(radioGroup, HTTPS_ONLY_PBM_ONLY);

  // Check if prefs are set correctly when disabled again.
  disableRadio.click();
  check(radioGroup, HTTPS_ONLY_DISABLED);

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

function check(radioGroupElement, expectedValue) {
  is(
    radioGroupElement.value,
    expectedValue,
    "Radio Group value should match expected value"
  );
  is(
    SpecialPowers.getBoolPref("dom.security.https_only_mode"),
    expectedValue === HTTPS_ONLY_ENABLED,
    "HTTPS-Only pref should match expected value."
  );
  is(
    SpecialPowers.getBoolPref("dom.security.https_only_mode_pbm"),
    expectedValue === HTTPS_ONLY_PBM_ONLY,
    "HTTPS-Only PBM pref should match expected value."
  );
}
