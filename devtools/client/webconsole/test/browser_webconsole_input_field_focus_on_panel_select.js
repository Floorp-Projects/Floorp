/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the JS input field is focused when the user switches back to the
// web console from other tools, see bug 891581.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>hello";

add_task(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  is(hud.jsterm.inputNode.hasAttribute("focused"), true,
     "inputNode should be focused");

  hud.ui.filterBox.focus();

  is(hud.ui.filterBox.hasAttribute("focused"), true,
     "filterBox should be focused");

  is(hud.jsterm.inputNode.hasAttribute("focused"), false,
     "inputNode shouldn't be focused");

  yield openDebugger();
  hud = yield openConsole();

  is(hud.jsterm.inputNode.hasAttribute("focused"), true,
     "inputNode should be focused");
});
