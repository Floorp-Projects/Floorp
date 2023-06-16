/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the user can execute only the code that is selected in the input, in editor
// mode.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1576563

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Web Console test for executing input selection";

add_task(async function () {
  await pushPref("devtools.webconsole.input.editor", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  const expression = `x = "first assignment";x;
    x = "second assignment";x;`;

  info("Evaluate the whole expression");
  setInputValue(hud, expression);

  let onResultMessage = waitForMessageByType(
    hud,
    "second assignment",
    ".result"
  );
  synthesizeKeyboardEvaluation();
  await onResultMessage;
  ok(true, "The whole expression is evaluated when there's no selection");

  info("Select the first line and evaluate");
  hud.ui.jsterm.editor.setSelection(
    { line: 0, ch: 0 },
    { line: 0, ch: expression.split("\n")[0].length }
  );
  onResultMessage = waitForMessageByType(hud, "first assignment", ".result");
  synthesizeKeyboardEvaluation();
  await onResultMessage;
  ok(true, "Only the expression on the first line was evaluated");

  info("Check that it also works when clicking on the Run button");
  onResultMessage = waitForMessageByType(hud, "first assignment", ".result");
  hud.ui.outputNode
    .querySelector(".webconsole-editor-toolbar-executeButton")
    .click();
  await onResultMessage;
  ok(
    true,
    "Only the expression on the first line was evaluated when clicking the Run button"
  );

  info("Check that this is disabled in inline mode");
  await toggleLayout(hud);
  hud.ui.jsterm.editor.setSelection(
    { line: 0, ch: 0 },
    { line: 0, ch: expression.split("\n")[0].length }
  );
  onResultMessage = waitForMessageByType(hud, "second assignment", ".result");
  synthesizeKeyboardEvaluation();
  await onResultMessage;
  ok(true, "The whole expression was evaluated in inline mode");
});

function synthesizeKeyboardEvaluation() {
  EventUtils.synthesizeKey("KEY_Enter", {
    [Services.appinfo.OS === "Darwin" ? "metaKey" : "ctrlKey"]: true,
  });
}
