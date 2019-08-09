/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that hitting Ctrl (or Cmd on OSX) + Enter does execute the input
// and Enter does not when in editor mode.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1519314

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Web Console test for bug 1519314";

add_task(async function() {
  await pushPref("devtools.webconsole.features.editor", true);
  await pushPref("devtools.webconsole.input.editor", true);
  await performEditorEnabledTests();
});

add_task(async function() {
  await pushPref("devtools.webconsole.input.editor", false);
  await performEditorDisabledTests();
});

const first_expression = `x = 10`;
const second_expression = `x + 1`;

/**
 * Simulates typing of the two expressions above in the console
 * for the two test cases below.
 */
function simulateConsoleInput() {
  EventUtils.sendString(first_expression);
  EventUtils.sendKey("return");
  EventUtils.sendString(second_expression);
}

async function performEditorEnabledTests() {
  const hud = await openNewTabAndConsole(TEST_URI);

  simulateConsoleInput();

  is(
    getInputValue(hud),
    `${first_expression}\n${second_expression}`,
    "text input after pressing the return key is present"
  );

  const { visibleMessages } = hud.ui.wrapper.getStore().getState().messages;
  is(
    visibleMessages.length,
    0,
    "input expressions should not have been executed"
  );

  let onMessage = waitForMessage(hud, "11", ".result");
  EventUtils.synthesizeKey("KEY_Enter", {
    [Services.appinfo.OS === "Darwin" ? "metaKey" : "ctrlKey"]: true,
  });
  await onMessage;
  ok(true, "Input was executed on Ctrl/Cmd + Enter");

  setInputValue(hud, "function x() {");
  onMessage = waitForMessage(hud, "SyntaxError");
  EventUtils.synthesizeKey("KEY_Enter", {
    [Services.appinfo.OS === "Darwin" ? "metaKey" : "ctrlKey"]: true,
  });
  await onMessage;
  ok(true, "The expression was evaluated, even if it wasn't well-formed");
}

async function performEditorDisabledTests() {
  const hud = await openNewTabAndConsole(TEST_URI);

  simulateConsoleInput();
  // execute the 2nd expression which should have been entered but not executed
  EventUtils.sendKey("return");

  let msg = await waitFor(() => findMessage(hud, "10"));
  ok(msg, "found evaluation result of 1st expression");

  msg = await waitFor(() => findMessage(hud, "11"));
  ok(msg, "found evaluation result of 2nd expression");

  is(getInputValue(hud), "", "input line is cleared after execution");
}
