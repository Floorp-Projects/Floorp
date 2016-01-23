/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

registerCleanupFunction(removeCustomToolbars);

// Sanity checks
add_task(function sanityChecks() {
  SimpleTest.doesThrow(() => CustomizableUI.registerArea("@foo"),
                       "Registering areas with an invalid ID should throw.");

  SimpleTest.doesThrow(() => CustomizableUI.registerArea([]),
                       "Registering areas with an invalid ID should throw.");

  SimpleTest.doesThrow(() => CustomizableUI.unregisterArea("@foo"),
                       "Unregistering areas with an invalid ID should throw.");

  SimpleTest.doesThrow(() => CustomizableUI.unregisterArea([]),
                       "Unregistering areas with an invalid ID should throw.");

  SimpleTest.doesThrow(() => CustomizableUI.unregisterArea("unknown"),
                       "Unregistering an area that's not registered should throw.");
});

// Check areas are loaded with their default placements.
add_task(function checkLoadedAres() {
  ok(CustomizableUI.inDefaultState, "Everything should be in its default state.");
});

// Check registering and unregistering a new area.
add_task(function checkRegisteringAndUnregistering() {
  const kToolbarId = "test-registration-toolbar";
  const kButtonId = "test-registration-button";
  createDummyXULButton(kButtonId);
  createToolbarWithPlacements(kToolbarId, ["spring", kButtonId, "spring"]);
  assertAreaPlacements(kToolbarId,
                       [/customizableui-special-spring\d+/,
                        kButtonId,
                        /customizableui-special-spring\d+/]);
  ok(!CustomizableUI.inDefaultState, "With a new toolbar it is no longer in a default state.");
  removeCustomToolbars(); // Will call unregisterArea for us
  ok(CustomizableUI.inDefaultState, "When the toolbar is unregistered, " +
     "everything will return to the default state.");
});

add_task(function* asyncCleanup() {
  yield resetCustomization();
});
