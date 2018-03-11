/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

// Test that the JS input field is focused when the user switches back to the
// web console from other tools, see bug 891581.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>hello";

add_task(async function () {
  await loadTab(TEST_URI);
  let hud = await openConsole();
  hud.jsterm.clearOutput();

  is(hud.jsterm.inputNode.hasAttribute("focused"), true,
     "inputNode should be focused");

  hud.ui.filterBox.focus();

  is(hud.ui.filterBox.hasAttribute("focused"), true,
     "filterBox should be focused");

  is(hud.jsterm.inputNode.hasAttribute("focused"), false,
     "inputNode shouldn't be focused");

  await openInspector();
  hud = await openConsole();

  is(hud.jsterm.inputNode.hasAttribute("focused"), true,
     "inputNode should be focused");
});
