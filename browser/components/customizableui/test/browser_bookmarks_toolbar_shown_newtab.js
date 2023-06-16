/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

// Entering customize mode should show the toolbar as long as it's not set to "never"
add_task(async function () {
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "Default state to begin");

  let toolbar = document.querySelector("#PersonalToolbar");
  for (let state of ["always", "never", "newtab"]) {
    info(`Testing setting toolbar state to '${state}'`);

    setToolbarVisibility(toolbar, state, true, false);

    await startCustomizing();

    let expected = state != "never";
    await TestUtils.waitForCondition(
      () => !toolbar.collapsed == expected,
      `Waiting for toolbar visibility, state=${state}, visible=${!toolbar.collapsed}, expected=${expected}`
    );
    is(
      !toolbar.collapsed,
      expected,
      "The toolbar should be visible when state isn't 'never'"
    );

    await endCustomizing();
  }
  await resetCustomization();
});
