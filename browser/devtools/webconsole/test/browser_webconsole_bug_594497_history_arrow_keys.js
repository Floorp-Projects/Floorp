/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

var inputNode, values;

var TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
               "bug 594497 and bug 619598";

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  setup(hud);
  performTests();

  inputNode = values = null;
});

function setup(HUD) {
  inputNode = HUD.jsterm.inputNode;

  inputNode.focus();

  ok(!inputNode.value, "inputNode.value is empty");

  values = ["document", "window", "document.body"];
  values.push(values.join(";\n"), "document.location");

  // Execute each of the values;
  for (let i = 0; i < values.length; i++) {
    HUD.jsterm.setInputValue(values[i]);
    HUD.jsterm.execute();
  }
}

function performTests() {
  EventUtils.synthesizeKey("VK_UP", {});

  is(inputNode.value, values[4],
     "VK_UP: inputNode.value #4 is correct");

  ok(inputNode.selectionStart == values[4].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(inputNode.value, values[3],
     "VK_UP: inputNode.value #3 is correct");

  ok(inputNode.selectionStart == values[3].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  inputNode.setSelectionRange(values[3].length - 2, values[3].length - 2);

  EventUtils.synthesizeKey("VK_UP", {});
  EventUtils.synthesizeKey("VK_UP", {});

  is(inputNode.value, values[3],
     "VK_UP two times: inputNode.value #3 is correct");

  ok(inputNode.selectionStart == inputNode.value.indexOf("\n") &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(inputNode.value, values[3],
     "VK_UP again: inputNode.value #3 is correct");

  ok(inputNode.selectionStart == 0 &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(inputNode.value, values[2],
     "VK_UP: inputNode.value #2 is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(inputNode.value, values[1],
     "VK_UP: inputNode.value #1 is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(inputNode.value, values[0],
     "VK_UP: inputNode.value #0 is correct");

  ok(inputNode.selectionStart == values[0].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(inputNode.value, values[1],
     "VK_DOWN: inputNode.value #1 is correct");

  ok(inputNode.selectionStart == values[1].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(inputNode.value, values[2],
     "VK_DOWN: inputNode.value #2 is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(inputNode.value, values[3],
     "VK_DOWN: inputNode.value #3 is correct");

  ok(inputNode.selectionStart == values[3].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  inputNode.setSelectionRange(2, 2);

  EventUtils.synthesizeKey("VK_DOWN", {});
  EventUtils.synthesizeKey("VK_DOWN", {});

  is(inputNode.value, values[3],
     "VK_DOWN two times: inputNode.value #3 is correct");

  ok(inputNode.selectionStart > inputNode.value.lastIndexOf("\n") &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(inputNode.value, values[3],
     "VK_DOWN again: inputNode.value #3 is correct");

  ok(inputNode.selectionStart == values[3].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(inputNode.value, values[4],
     "VK_DOWN: inputNode.value #4 is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  ok(!inputNode.value,
     "VK_DOWN: inputNode.value is empty");
}
