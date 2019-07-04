/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that hitting Ctrl + E does toggle the editor mode.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1519105

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Test editor mode toggle keyboard shortcut";
const EDITOR_PREF = "devtools.webconsole.input.editor";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);
  await pushPref("devtools.webconsole.input.editor", true);
  // Run test with legacy JsTerm
  info("Test legacy JsTerm");
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTest();

  // And then run it with the CodeMirror-powered one.
  info("Test codeMirror JsTerm");
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTest();
});

async function performTest() {
  // Start with the editor turned off
  await pushPref(EDITOR_PREF, false);
  let hud = await openNewTabAndConsole(TEST_URI);

  is(isEditorModeEnabled(hud), false, "The console isn't in editor mode");

  info("Enable the editor mode");
  await toggleLayout(hud);
  is(isEditorModeEnabled(hud), true, "Editor mode is enabled");

  info("Close the console and reopen it");
  await closeConsole();
  hud = await openConsole();
  is(isEditorModeEnabled(hud), true, "Editor mode is still enabled");

  info("Disable the editor mode");
  await toggleLayout(hud);
  is(isEditorModeEnabled(hud), false, "Editor was disabled");

  info("Enable the editor mode again");
  await toggleLayout(hud);
  is(isEditorModeEnabled(hud), true, "Editor mode was enabled again");

  Services.prefs.clearUserPref(EDITOR_PREF);
}
