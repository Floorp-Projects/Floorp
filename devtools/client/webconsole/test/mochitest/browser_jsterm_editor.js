/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the editor is displayed as expected.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>Test editor";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();

  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  await pushPref("devtools.webconsole.input.editor", false);

  const tab = await addTab(TEST_URI);
  let hud = await openConsole(tab);

  info("Test that the editor mode is disabled when the pref is set to false");
  is(
    isEditorModeEnabled(hud),
    false,
    "Editor is disabled when pref is set to false"
  );

  await closeConsole();

  info(
    "Test that wrapper does have the jsterm-editor class when editor is enabled"
  );
  await pushPref("devtools.webconsole.input.editor", true);
  hud = await openConsole(tab);
  is(
    isEditorModeEnabled(hud),
    true,
    "Editor is enabled when pref is set to true"
  );
}
