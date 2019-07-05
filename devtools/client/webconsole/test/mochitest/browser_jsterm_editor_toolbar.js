/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the editor toolbar works as expected when in editor mode.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>Test editor toolbar";

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

  info("Test that the toolbar is not displayed when in editor mode");
  let toolbar = getEditorToolbar(hud);
  is(toolbar, null, "The toolbar isn't displayed when not in editor mode");
  await closeToolbox();

  await pushPref("devtools.webconsole.input.editor", true);
  hud = await openConsole(tab);

  info("Test that the toolbar is displayed when in editor mode");
  toolbar = getEditorToolbar(hud);
  ok(toolbar, "The toolbar is displayed when in editor mode");

  info("Test that the toolbar has the expected items");
  const runButton = toolbar.querySelector(
    ".webconsole-editor-toolbar-executeButton"
  );
  is(runButton.textContent, "Run", "The button has the expected text");
  const keyShortcut =
    (Services.appinfo.OS === "Darwin" ? "Cmd" : "Ctrl") + " + Enter";
  is(
    runButton.getAttribute("title"),
    `Run expression (${keyShortcut}). This won’t clear the input.`
  );

  info("Test that clicking on the Run button works as expected");
  // Setting the input value.
  const input = "`${1 + 1} = 2`";
  setInputValue(hud, input);

  runButton.click();
  await waitFor(() => findMessage(hud, input));
  await waitFor(() => findMessage(hud, "2 = 2", ".message.result"));
  ok(true, "The expression and its result are displayed in the output");
  ok(
    isInputFocused(hud),
    "input is still focused after clicking the Run button"
  );
}

function getEditorToolbar(hud) {
  return hud.ui.outputNode.querySelector(".webconsole-editor-toolbar");
}
