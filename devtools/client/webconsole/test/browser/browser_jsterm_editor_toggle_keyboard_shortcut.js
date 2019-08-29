/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that hitting Ctrl + B does toggle the editor mode.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1519105

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Test editor mode toggle keyboard shortcut";
const EDITOR_PREF = "devtools.webconsole.input.editor";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);

  // Start with the editor turned off
  await pushPref(EDITOR_PREF, false);
  let hud = await openNewTabAndConsole(TEST_URI);

  const INPUT_VALUE = "hello";
  setInputValue(hud, INPUT_VALUE);

  is(isEditorModeEnabled(hud), false, "The console isn't in editor mode");

  info("Enable the editor mode");
  await toggleLayout(hud);
  is(isEditorModeEnabled(hud), true, "Editor mode is enabled");
  is(getInputValue(hud), INPUT_VALUE, "The input value wasn't cleared");

  info("Close the console and reopen it");
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

  Services.prefs.clearUserPref(EDITOR_PREF);
});
