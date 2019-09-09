/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Tests the initial state of the background script for the popup.
 */

function setupBackgroundJsm() {
  const background = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm"
  );
  return background;
}

add_task(function test() {
  info("Test that we get the default values from state.");
  const {
    getState,
    revertPrefs,
    DEFAULT_BUFFER_SIZE,
    DEFAULT_STACKWALK_FEATURE,
  } = setupBackgroundJsm().forTestsOnly;

  Assert.equal(
    getState().buffersize,
    DEFAULT_BUFFER_SIZE,
    "The initial state has the default buffersize."
  );
  Assert.equal(
    getState().features.stackwalk,
    DEFAULT_STACKWALK_FEATURE,
    "The stackwalk feature is initialized to the default."
  );
  revertPrefs();
});

add_task(function test() {
  info("Test that the state and features are properly validated.");
  const {
    getState,
    adjustState,
    revertPrefs,
    initializeState,
    DEFAULT_STACKWALK_FEATURE,
  } = setupBackgroundJsm().forTestsOnly;

  info("Manipulate the state.");
  const state = getState();
  state.features.stackwalk = !DEFAULT_STACKWALK_FEATURE;
  state.features.UNKNOWN_FEATURE_FOR_TESTS = true;
  adjustState(state);
  adjustState(initializeState());

  Assert.equal(
    getState().features.UNKNOWN_FEATURE_FOR_TESTS,
    undefined,
    "The unknown feature is removed."
  );
  Assert.equal(
    getState().features.stackwalk,
    !DEFAULT_STACKWALK_FEATURE,
    "The stackwalk preference is still flipped from the default."
  );
  revertPrefs();
});
