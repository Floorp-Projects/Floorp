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
  const { getRecordingPreferences } = setupBackgroundJsm();

  const preferences = getRecordingPreferences(
    "aboutprofiling",
    Services.profiler.GetFeatures()
  );

  Assert.notEqual(
    preferences.entries,
    undefined,
    "The initial state has the default entries."
  );
  Assert.notEqual(
    preferences.interval,
    undefined,
    "The initial state has the default interval."
  );
  Assert.notEqual(
    preferences.features,
    undefined,
    "The initial state has the default features."
  );
  Assert.equal(
    preferences.features.includes("js"),
    true,
    "The js feature is initialized to the default."
  );
  Assert.notEqual(
    preferences.threads,
    undefined,
    "The initial state has the default threads."
  );
  Assert.equal(
    preferences.threads.includes("GeckoMain"),
    true,
    "The GeckoMain thread is initialized to the default."
  );
  Assert.notEqual(
    preferences.objdirs,
    undefined,
    "The initial state has the default objdirs."
  );
  Assert.notEqual(
    preferences.duration,
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
    getRecordingPreferences,
    setRecordingPreferences,
    changePreset,
  } = setupBackgroundJsm();

  const supportedFeatures = Services.profiler.GetFeatures();

  changePreset("aboutprofiling", "custom", supportedFeatures);

  Assert.ok(
    getRecordingPreferences(
      "aboutprofiling",
      supportedFeatures
    ).features.includes("js"),
    "The js preference is present initially."
  );

  const settings = getRecordingPreferences("aboutprofiling", supportedFeatures);
  settings.features = settings.features.filter(feature => feature !== "js");
  settings.features.push("UNKNOWN_FEATURE_FOR_TESTS");
  setRecordingPreferences("aboutprofiling", settings);

  Assert.ok(
    !getRecordingPreferences(
      "aboutprofiling",
      supportedFeatures
    ).features.includes("UNKNOWN_FEATURE_FOR_TESTS"),
    "The unknown feature is removed."
  );
  Assert.ok(
    !getRecordingPreferences(
      "aboutprofiling",
      supportedFeatures
    ).features.includes("js"),
    "The js preference is still flipped from the default."
  );
});
