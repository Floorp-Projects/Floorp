/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that style sheets can be disabled and enabled.

// https rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(function* () {
  let { panel, ui } = yield openStyleEditorForURL(TESTCASE_URI);
  let editor = yield ui.editors[0].getSourceEditor();

  let summary = editor.summary;
  let enabledToggle = summary.querySelector(".stylesheet-enabled");
  ok(enabledToggle, "enabled toggle button exists");

  is(editor.styleSheet.disabled, false,
     "first stylesheet is initially enabled");

  is(summary.classList.contains("disabled"), false,
     "first stylesheet is initially enabled, UI does not have DISABLED class");

  info("Disabling the first stylesheet.");
  yield toggleEnabled(editor, enabledToggle, panel.panelWindow);

  is(editor.styleSheet.disabled, true, "first stylesheet is now disabled");
  is(summary.classList.contains("disabled"), true,
     "first stylesheet is now disabled, UI has DISABLED class");

  info("Enabling the first stylesheet again.");
  yield toggleEnabled(editor, enabledToggle, panel.panelWindow);

  is(editor.styleSheet.disabled, false,
     "first stylesheet is now enabled again");
  is(summary.classList.contains("disabled"), false,
     "first stylesheet is now enabled again, UI does not have DISABLED class");
});

function* toggleEnabled(editor, enabledToggle, panelWindow) {
  let changed = editor.once("property-change");

  info("Waiting for focus.");
  yield SimpleTest.promiseFocus(panelWindow);

  info("Clicking on the toggle.");
  EventUtils.synthesizeMouseAtCenter(enabledToggle, {}, panelWindow);

  info("Waiting for stylesheet to be disabled.");
  let property = yield changed;
  while (property !== "disabled") {
    info("Ignoring property-change for '" + property + "'.");
    property = yield editor.once("property-change");
  }
}
