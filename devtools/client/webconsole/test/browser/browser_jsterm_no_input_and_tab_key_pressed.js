/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 583816.

const TEST_URI =
  "data:text/html,<!DOCTYPE html><meta charset=utf8>Testing jsterm with no input";

add_task(async function () {
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
    hasFocus(
      hud.ui.outputNode.querySelector(".webconsole-input-openEditorButton")
    ),
    `The "Toggle Editor" button is now focused`
  );

  info("Check that hitting Shift+Tab again place the focus on the filter bar");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  ok(
    hasFocus(
      hud.ui.outputNode.querySelector(
        ".webconsole-console-settings-menu-button"
      )
    ),
    `The "Console Settings" button is now focused`
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
