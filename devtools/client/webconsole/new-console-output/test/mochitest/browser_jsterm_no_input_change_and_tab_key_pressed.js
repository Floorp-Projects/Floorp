/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 734061.

const TEST_URI = "data:text/html,Testing jsterm focus";

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  is(hasFocus(input), true, "input has focus");
  EventUtils.synthesizeKey("VK_TAB", {});
  is(hasFocus(input), false, "focus moved away");

  // Test user changed something
  input.focus();
  EventUtils.synthesizeKey("A", {});
  EventUtils.synthesizeKey("VK_TAB", {});
  is(hasFocus(input), true, "input is still focused");

  // Test non empty input but not changed since last focus
  input.blur();
  input.focus();
  EventUtils.synthesizeKey("VK_RIGHT", {});
  EventUtils.synthesizeKey("VK_TAB", {});
  is(hasFocus(input), false, "focus moved away");
});
