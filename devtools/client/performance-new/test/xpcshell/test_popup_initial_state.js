/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Tests the initial state of the background script for the popup.
 */

function setupBackgroundJsm() {
  return ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );
}

add_task(function test() {
  info("Test that we get the default preference values from the browser.");
  const { getRecordingPreferencesFromBrowser } = setupBackgroundJsm();

  Assert.notEqual(
    getRecordingPreferencesFromBrowser().entries,
    undefined,
    "The initial state has the default entries."
  );
  Assert.notEqual(
    getRecordingPreferencesFromBrowser().interval,
    undefined,
    "The initial state has the default interval."
  );
  Assert.notEqual(
    getRecordingPreferencesFromBrowser().features,
    undefined,
    "The initial state has the default features."
  );
  Assert.equal(
    getRecordingPreferencesFromBrowser().features.includes("js"),
    true,
    "The js feature is initialized to the default."
  );
  Assert.notEqual(
    getRecordingPreferencesFromBrowser().threads,
    undefined,
    "The initial state has the default threads."
  );
  Assert.equal(
    getRecordingPreferencesFromBrowser().threads.includes("GeckoMain"),
    true,
    "The GeckoMain thread is initialized to the default."
  );
  Assert.notEqual(
    getRecordingPreferencesFromBrowser().objdirs,
    undefined,
    "The initial state has the default objdirs."
  );
  Assert.notEqual(
    getRecordingPreferencesFromBrowser().duration,
    undefined,
    "The duration is initialized to the duration."
  );
});

add_task(function test() {
  info(
    "Test that the state and features are properly validated. This ensures that as " +
      "we add and remove features, the stored preferences do not cause the Gecko " +
      "Profiler interface to crash with invalid values."
  );
  const {
    getRecordingPreferencesFromBrowser,
    setRecordingPreferencesOnBrowser,
    revertRecordingPreferences,
    changePreset,
  } = setupBackgroundJsm();

  changePreset("custom");

  Assert.ok(
    getRecordingPreferencesFromBrowser().features.includes("js"),
    "The js preference is present initially."
  );

  const settings = getRecordingPreferencesFromBrowser();
  settings.features = settings.features.filter(feature => feature !== "js");
  settings.features.push("UNKNOWN_FEATURE_FOR_TESTS");
  setRecordingPreferencesOnBrowser(settings);

  Assert.ok(
    !getRecordingPreferencesFromBrowser().features.includes(
      "UNKNOWN_FEATURE_FOR_TESTS"
    ),
    "The unknown feature is removed."
  );
  Assert.ok(
    !getRecordingPreferencesFromBrowser().features.includes("js"),
    "The js preference is still flipped from the default."
  );
  revertRecordingPreferences();
});
