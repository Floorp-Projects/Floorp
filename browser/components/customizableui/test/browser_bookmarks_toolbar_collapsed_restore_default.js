/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

// Restoring default should set Bookmarks Toolbar back to "newtab"
add_task(async function () {
  let prefName = "browser.toolbars.bookmarks.visibility";
  let toolbar = document.querySelector("#PersonalToolbar");
  for (let state of ["always", "never"]) {
    info(`Testing setting toolbar state to '${state}'`);

    await resetCustomization();
    ok(CustomizableUI.inDefaultState, "Default state to begin");

    setToolbarVisibility(toolbar, state, true, false);

    is(
      Services.prefs.getCharPref(prefName),
      state,
      "Pref updated to: " + state
    );
    ok(!CustomizableUI.inDefaultState, "Not in default state");

    await resetCustomization();

    ok(CustomizableUI.inDefaultState, "Back in default state after reset");
    is(
      Services.prefs.getCharPref(prefName),
      "newtab",
      "Pref should get reset to 'newtab'"
    );
  }
});
