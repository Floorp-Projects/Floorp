/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that when the input overflows, inserting a tab doesn't not impact the
// scroll position. See Bug 1578283.

"use strict";

const TEST_URI = "data:text/html,<!DOCTYPE html><meta charset=utf8>";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const cmScroller = hud.ui.outputNode.querySelector(".CodeMirror-scroll");

  info("Fill in the input with a hundred lines to make it overflow");
  await setInputValue(hud, "x;\n".repeat(100));

  ok(hasVerticalOverflow(cmScroller), "input overflows");

  info("Put the cursor at the very beginning");
  hud.jsterm.editor.setCursor({
    line: 0,
    ch: 0,
  });
  is(cmScroller.scrollTop, 0, "input is scrolled all the way up");

  info("Move the cursor one line down and hit Tab");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Tab");
  checkInputValueAndCursorPosition(
    hud,
    `x;\n\t|x;\n${"x;\n".repeat(98)}`,
    "a tab char was added at the start of the second line after hitting Tab"
  );
  is(
    cmScroller.scrollTop,
    0,
    "Scroll position wasn't affected by new char addition"
  );
});
