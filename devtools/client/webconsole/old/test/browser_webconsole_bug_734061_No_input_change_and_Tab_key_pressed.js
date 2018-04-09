/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/old/" +
                 "test/browser/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  is(input.getAttribute("focused"), "true", "input has focus");
  EventUtils.synthesizeKey("KEY_Tab");
  is(input.getAttribute("focused"), "", "focus moved away");

  // Test user changed something
  input.focus();
  EventUtils.sendString("a");
  EventUtils.synthesizeKey("KEY_Tab");
  is(input.getAttribute("focused"), "true", "input is still focused");

  // Test non empty input but not changed since last focus
  input.blur();
  input.focus();
  EventUtils.synthesizeKey("KEY_ArrowRight");
  EventUtils.synthesizeKey("KEY_Tab");
  is(input.getAttribute("focused"), "", "input moved away");
});
