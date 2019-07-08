/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_DRAG_SPACE = "browser.tabs.extraDragSpace";

add_task(async function setup() {
  await startCustomizing();
});

add_task(async function test_dragspace_checkbox() {
  let win = document.getElementById("main-window");
  let checkbox = document.getElementById(
    "customization-extra-drag-space-checkbox"
  );
  is(
    Services.prefs.getBoolPref(PREF_DRAG_SPACE),
    false,
    "Drag space is disabled initially."
  );
  ok(!checkbox.checked, "Checkbox state reflects disabled drag space.");

  let dragSpaceEnabled = BrowserTestUtils.waitForAttribute(
    "extradragspace",
    win,
    "true"
  );
  EventUtils.synthesizeMouseAtCenter(checkbox, {});
  await dragSpaceEnabled;
  is(
    Services.prefs.getBoolPref(PREF_DRAG_SPACE),
    true,
    "Drag space is enabled."
  );

  EventUtils.synthesizeMouseAtCenter(checkbox, {});
  await BrowserTestUtils.waitForCondition(
    () => !win.hasAttribute("extradragspace")
  );
  is(
    Services.prefs.getBoolPref(PREF_DRAG_SPACE),
    false,
    "Drag space is disabled."
  );
});

add_task(async function test_dragspace_checkbox_disable() {
  let dragSpaceCheckbox = document.getElementById(
    "customization-extra-drag-space-checkbox"
  );
  let titleBarCheckbox = document.getElementById(
    "customization-titlebar-visibility-checkbox"
  );

  ok(!titleBarCheckbox.checked, "Title bar is disabled initially.");
  ok(
    !dragSpaceCheckbox.hasAttribute("disabled"),
    "Drag space checkbox is enabled initially."
  );

  let checkboxDisabled = BrowserTestUtils.waitForAttribute(
    "disabled",
    dragSpaceCheckbox,
    "true"
  );
  EventUtils.synthesizeMouseAtCenter(titleBarCheckbox, {});
  await checkboxDisabled;
  info("Checkbox was disabled!");

  EventUtils.synthesizeMouseAtCenter(titleBarCheckbox, {});
  await BrowserTestUtils.waitForCondition(
    () => !dragSpaceCheckbox.hasAttribute("disabled")
  );
  info("Checkbox was enabled!");
});

add_task(async function test_dragspace_reset() {
  let win = document.getElementById("main-window");
  let checkbox = document.getElementById(
    "customization-extra-drag-space-checkbox"
  );
  is(
    Services.prefs.getBoolPref(PREF_DRAG_SPACE),
    false,
    "Drag space is disabled initially."
  );
  ok(!checkbox.checked, "Checkbox state reflects disabled drag space.");

  // Enable dragspace manually.
  let dragSpaceEnabled = BrowserTestUtils.waitForAttribute(
    "extradragspace",
    win,
    "true"
  );
  EventUtils.synthesizeMouseAtCenter(checkbox, {});
  await dragSpaceEnabled;
  is(
    Services.prefs.getBoolPref(PREF_DRAG_SPACE),
    true,
    "Drag space is enabled."
  );

  // Disable dragspace through reset.
  await gCustomizeMode.reset();
  await BrowserTestUtils.waitForCondition(
    () => !win.hasAttribute("extradragspace")
  );
  is(
    Services.prefs.getBoolPref(PREF_DRAG_SPACE),
    false,
    "Drag space is disabled."
  );

  // Undo reset and check that dragspace is enabled again.
  dragSpaceEnabled = BrowserTestUtils.waitForAttribute(
    "extradragspace",
    win,
    "true"
  );
  await gCustomizeMode.undoReset();
  await dragSpaceEnabled;
  is(
    Services.prefs.getBoolPref(PREF_DRAG_SPACE),
    true,
    "Drag space is enabled."
  );

  Services.prefs.clearUserPref(PREF_DRAG_SPACE);
});

add_task(async function cleanup() {
  await endCustomizing();

  Services.prefs.clearUserPref(PREF_DRAG_SPACE);
});
