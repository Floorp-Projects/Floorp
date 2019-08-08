/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that user input that is not submitted in the command line input is not
// lost after navigating in history.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=817834

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 817834";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  ok(!getInputValue(hud), "console input is empty");
  checkInputCursorPosition(hud, 0, "Cursor is at expected position");

  setInputValue(hud, '"first item"');
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(getInputValue(hud), '"first item"', "null test history up");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(getInputValue(hud), '"first item"', "null test history down");

  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => findMessage(hud, "first item", ".result"));
  is(getInputValue(hud), "", "cleared input line after submit");

  setInputValue(hud, '"editing input 1"');
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(getInputValue(hud), '"first item"', "test history up");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    getInputValue(hud),
    '"editing input 1"',
    "test history down restores in-progress input"
  );

  setInputValue(hud, '"second item"');
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => findMessage(hud, "second item", ".result"));

  setInputValue(hud, '"editing input 2"');
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(getInputValue(hud), '"second item"', "test history up");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(getInputValue(hud), '"first item"', "test history up");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(getInputValue(hud), '"second item"', "test history down");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    getInputValue(hud),
    '"editing input 2"',
    "test history down restores new in-progress input again"
  );

  // Appending the same value again should not impact the history.
  // Let's also use some spaces around to check that the input value
  // is properly trimmed.
  setInputValue(hud, '"second item"');
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => findMessages(hud, "second item", ".result").length == 2);

  setInputValue(hud, '"second item"    ');
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(() => findMessages(hud, "second item", ".result").length == 3);

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    getInputValue(hud),
    '"second item"',
    "test history up reaches duplicated entry just once"
  );
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    getInputValue(hud),
    '"first item"',
    "test history up reaches the previous value"
  );
});
