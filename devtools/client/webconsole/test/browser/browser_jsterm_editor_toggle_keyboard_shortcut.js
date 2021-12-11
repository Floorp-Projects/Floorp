/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that hitting Ctrl + B does toggle the editor mode.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1519105

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Test editor mode toggle keyboard shortcut";
const EDITOR_PREF = "devtools.webconsole.input.editor";

// See Bug 1631529
requestLongerTimeout(2);

add_task(async function() {
  // Start with the editor turned off
  await pushPref(EDITOR_PREF, false);
  let hud = await openNewTabAndConsole(TEST_URI);

  const INPUT_VALUE = "`hello`";
  setInputValue(hud, INPUT_VALUE);

  is(isEditorModeEnabled(hud), false, "The console isn't in editor mode");

  info("Enable the editor mode");
  await toggleLayout(hud);
  is(isEditorModeEnabled(hud), true, "Editor mode is enabled");
  is(getInputValue(hud), INPUT_VALUE, "The input value wasn't cleared");

  info("Close the console and reopen it");
  // Wait for eager evaluation result so we don't have a pending call to the server.
  await waitForEagerEvaluationResult(hud, `"hello"`);
  await closeConsole();
  hud = await openConsole();
  is(isEditorModeEnabled(hud), true, "Editor mode is still enabled");
  setInputValue(hud, INPUT_VALUE);

  info("Disable the editor mode");
  await toggleLayout(hud);
  is(isEditorModeEnabled(hud), false, "Editor was disabled");
  is(getInputValue(hud), INPUT_VALUE, "The input value wasn't cleared");

  info("Enable the editor mode again");
  await toggleLayout(hud);
  is(isEditorModeEnabled(hud), true, "Editor mode was enabled again");
  is(getInputValue(hud), INPUT_VALUE, "The input value wasn't cleared");

  info("Close popup on switching editor modes");
  const popup = hud.jsterm.autocompletePopup;
  await setInputValueForAutocompletion(hud, "a");
  ok(popup.isOpen, "Auto complete popup is shown");
  const onPopupClosed = popup.once("popup-closed");
  await toggleLayout(hud);
  await onPopupClosed;
  ok(!popup.isOpen, "Auto complete popup is hidden on switching editor modes.");

  // Wait for eager evaluation result so we don't have a pending call to the server.
  await waitForEagerEvaluationResult(hud, /ReferenceError/);

  Services.prefs.clearUserPref(EDITOR_PREF);
});
