/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Turning off Pocket pref should still be considered default state.
add_task(async function() {
  ok(CustomizableUI.inDefaultState, "Default state to begin");

  Assert.ok(
    Services.prefs.getBoolPref("extensions.pocket.enabled"),
    "Pocket feature is enabled by default"
  );

  Services.prefs.setBoolPref("extensions.pocket.enabled", false);

  ok(CustomizableUI.inDefaultState, "Should still be default state");
  await resetCustomization();

  Assert.ok(
    !Services.prefs.getBoolPref("extensions.pocket.enabled"),
    "Pocket feature is still off"
  );
  ok(CustomizableUI.inDefaultState, "Should still be default state");

  Services.prefs.setBoolPref("extensions.pocket.enabled", true);
});
