/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bugs 594497 and 619598.

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
               "bug 594497 and bug 619598";

const TEST_VALUES = [
  "document",
  "window",
  "document.body",
  "document;\nwindow;\ndocument.body",
  "document.location",
];

add_task(async function () {
  let hud = await openNewTabAndConsole(TEST_URI);
  let { jsterm } = hud;

  jsterm.focus();
  ok(!jsterm.getInputValue(), "jsterm.getInputValue() is empty");

  info("Execute each test value in the console");
  for (let value of TEST_VALUES) {
    jsterm.setInputValue(value);
    jsterm.execute();
  }

  performTests(jsterm);
});

function performTests(jsterm) {
  let { inputNode } = jsterm;
  let values = TEST_VALUES;

  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.getInputValue(), values[4],
     "VK_UP: jsterm.getInputValue() #4 is correct");

  ok(inputNode.selectionStart == values[4].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.getInputValue(), values[3],
     "VK_UP: jsterm.getInputValue() #3 is correct");

  ok(inputNode.selectionStart == values[3].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  inputNode.setSelectionRange(values[3].length - 2, values[3].length - 2);

  EventUtils.synthesizeKey("VK_UP", {});
  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.getInputValue(), values[3],
     "VK_UP two times: jsterm.getInputValue() #3 is correct");

  ok(inputNode.selectionStart == jsterm.getInputValue().indexOf("\n") &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.getInputValue(), values[3],
     "VK_UP again: jsterm.getInputValue() #3 is correct");

  ok(inputNode.selectionStart == 0 &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.getInputValue(), values[2],
     "VK_UP: jsterm.getInputValue() #2 is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.getInputValue(), values[1],
     "VK_UP: jsterm.getInputValue() #1 is correct");

  EventUtils.synthesizeKey("VK_UP", {});

  is(jsterm.getInputValue(), values[0],
     "VK_UP: jsterm.getInputValue() #0 is correct");

  ok(inputNode.selectionStart == values[0].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(jsterm.getInputValue(), values[1],
     "VK_DOWN: jsterm.getInputValue() #1 is correct");

  ok(inputNode.selectionStart == values[1].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(jsterm.getInputValue(), values[2],
     "VK_DOWN: jsterm.getInputValue() #2 is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(jsterm.getInputValue(), values[3],
     "VK_DOWN: jsterm.getInputValue() #3 is correct");

  ok(inputNode.selectionStart == values[3].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  inputNode.setSelectionRange(2, 2);

  EventUtils.synthesizeKey("VK_DOWN", {});
  EventUtils.synthesizeKey("VK_DOWN", {});

  is(jsterm.getInputValue(), values[3],
     "VK_DOWN two times: jsterm.getInputValue() #3 is correct");

  ok(inputNode.selectionStart > jsterm.getInputValue().lastIndexOf("\n") &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(jsterm.getInputValue(), values[3],
     "VK_DOWN again: jsterm.getInputValue() #3 is correct");

  ok(inputNode.selectionStart == values[3].length &&
     inputNode.selectionStart == inputNode.selectionEnd,
     "caret location is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  is(jsterm.getInputValue(), values[4],
    "VK_DOWN: jsterm.getInputValue() #4 is correct");

  EventUtils.synthesizeKey("VK_DOWN", {});

  ok(!jsterm.getInputValue(),
     "VK_DOWN: jsterm.getInputValue() is empty");
}
