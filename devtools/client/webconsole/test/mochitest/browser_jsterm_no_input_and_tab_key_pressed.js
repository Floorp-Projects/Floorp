/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 583816.

const TEST_URI =
  "data:text/html,<meta charset=utf8>Testing jsterm with no input";

add_task(async function() {
  // For now, let's disable editor as we don't know what the final placement of the
  // open editor button (which may impact this test).
  await pushPref("devtools.webconsole.features.editor", false);

  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  info("Check that hitting Tab when input is empty insert blur the input");
  jsterm.focus();
  setInputValue(hud, "");
  EventUtils.synthesizeKey("KEY_Tab");
  is(getInputValue(hud), "", "inputnode is empty - matched");
  ok(!isInputFocused(hud), "input isn't focused anymore");

  info("Check that hitting Shift+Tab when input is empty blur the input");
  jsterm.focus();
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  is(getInputValue(hud), "", "inputnode is empty - matched");
  ok(!isInputFocused(hud), "input isn't focused anymore");
  ok(
    hasFocus(hud.ui.outputNode.querySelector(".filter-checkbox input")),
    `The "Persist Logs" checkbox is now focused`
  );

  info("Check that hitting Tab when input is not empty insert a tab");
  jsterm.focus();

  const testString = "window.Bug583816";
  await setInputValueForAutocompletion(hud, testString, 0);
  checkInputValueAndCursorPosition(
    hud,
    `|${testString}`,
    "cursor is at the start of the input"
  );

  EventUtils.synthesizeKey("KEY_Tab");
  checkInputValueAndCursorPosition(
    hud,
    `\t|${testString}`,
    "a tab char was added at the start of the input after hitting Tab"
  );
  ok(isInputFocused(hud), "input is still focused");

  info(
    "Check that hitting Shift+Tab when input is not empty removed leading tabs"
  );
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  checkInputValueAndCursorPosition(
    hud,
    `|${testString}`,
    "The tab char at the the start of the input was removed after hitting Shift+Tab"
  );
  ok(isInputFocused(hud), "input is still focused");
});
