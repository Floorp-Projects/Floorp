/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  zmgmoz <zmgmoz@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Test that user input that is not submitted in the command line input is not
// lost after navigating in history.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=817834

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 817834";

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  testEditedInputHistory(hud);
});

function testEditedInputHistory(HUD) {
  let jsterm = HUD.jsterm;
  let inputNode = jsterm.inputNode;
  ok(!inputNode.value, "inputNode.value is empty");
  is(inputNode.selectionStart, 0);
  is(inputNode.selectionEnd, 0);

  jsterm.setInputValue('"first item"');
  EventUtils.synthesizeKey("VK_UP", {});
  is(inputNode.value, '"first item"', "null test history up");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(inputNode.value, '"first item"', "null test history down");

  jsterm.execute();
  is(inputNode.value, "", "cleared input line after submit");

  jsterm.setInputValue('"editing input 1"');
  EventUtils.synthesizeKey("VK_UP", {});
  is(inputNode.value, '"first item"', "test history up");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(inputNode.value, '"editing input 1"',
    "test history down restores in-progress input");

  jsterm.setInputValue('"second item"');
  jsterm.execute();
  jsterm.setInputValue('"editing input 2"');
  EventUtils.synthesizeKey("VK_UP", {});
  is(inputNode.value, '"second item"', "test history up");
  EventUtils.synthesizeKey("VK_UP", {});
  is(inputNode.value, '"first item"', "test history up");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(inputNode.value, '"second item"', "test history down");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(inputNode.value, '"editing input 2"',
     "test history down restores new in-progress input again");
}
