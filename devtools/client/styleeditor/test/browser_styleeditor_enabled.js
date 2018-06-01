/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that style sheets can be disabled and enabled.

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function() {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);
  const editor = await ui.editors[0].getSourceEditor();

  const summary = editor.summary;
  const enabledToggle = summary.querySelector(".stylesheet-enabled");
  ok(enabledToggle, "enabled toggle button exists");

  is(editor.styleSheet.disabled, false,
     "first stylesheet is initially enabled");

  is(summary.classList.contains("disabled"), false,
     "first stylesheet is initially enabled, UI does not have DISABLED class");

  info("Disabling the first stylesheet.");
  await toggleEnabled(editor, enabledToggle, panel.panelWindow);

  is(editor.styleSheet.disabled, true, "first stylesheet is now disabled");
  is(summary.classList.contains("disabled"), true,
     "first stylesheet is now disabled, UI has DISABLED class");

  info("Enabling the first stylesheet again.");
  await toggleEnabled(editor, enabledToggle, panel.panelWindow);

  is(editor.styleSheet.disabled, false,
     "first stylesheet is now enabled again");
  is(summary.classList.contains("disabled"), false,
     "first stylesheet is now enabled again, UI does not have DISABLED class");
});

async function toggleEnabled(editor, enabledToggle, panelWindow) {
  const changed = editor.once("property-change");

  info("Waiting for focus.");
  await SimpleTest.promiseFocus(panelWindow);

  info("Clicking on the toggle.");
  EventUtils.synthesizeMouseAtCenter(enabledToggle, {}, panelWindow);

  info("Waiting for stylesheet to be disabled.");
  let property = await changed;
  while (property !== "disabled") {
    info("Ignoring property-change for '" + property + "'.");
    property = await editor.once("property-change");
  }
}
