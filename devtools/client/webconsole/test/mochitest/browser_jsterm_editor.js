/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the editor is displayed as expected.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>Test editor";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);
  await pushPref("devtools.webconsole.input.editor", false);

  const tab = await addTab(TEST_URI);
  let hud = await openConsole(tab);

  info("Test that the editor mode is disabled when the pref is set to false");
  is(
    isEditorModeEnabled(hud),
    false,
    "Editor is disabled when pref is set to false"
  );
  let openEditorButton = getInlineOpenEditorButton(hud);
  ok(openEditorButton, "button is rendered in the inline input");
  let rect = openEditorButton.getBoundingClientRect();
  ok(rect.width > 0 && rect.height > 0, "Button is visible");

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
  openEditorButton = getInlineOpenEditorButton(hud);
  rect = openEditorButton.getBoundingClientRect();
  ok(rect.width === 0 && rect.height === 0, "Button is hidden in editor mode");

  await toggleLayout(hud);
  getInlineOpenEditorButton(hud).click();
  await waitFor(() => isEditorModeEnabled(hud));
  ok("Editor is open when clicking on the button");
});

function getInlineOpenEditorButton(hud) {
  return hud.ui.outputNode.querySelector(".webconsole-input-openEditorButton");
}
