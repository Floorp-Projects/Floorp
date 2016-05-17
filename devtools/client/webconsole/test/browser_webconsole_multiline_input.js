/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the console waits for more input instead of evaluating
// when valid, but incomplete, statements are present upon pressing enter
// -or- when the user ends a line with shift + enter.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

let SHOULD_ENTER_MULTILINE = [
  {input: "function foo() {" },
  {input: "var a = 1," },
  {input: "var a = 1;", shiftKey: true },
  {input: "function foo() { }", shiftKey: true },
  {input: "function" },
  {input: "(x) =>" },
  {input: "let b = {" },
  {input: "let a = [" },
  {input: "{" },
  {input: "{ bob: 3343," },
  {input: "function x(y=" },
  {input: "Array.from(" },
  // shift + enter creates a new line despite parse errors
  {input: "{2,}", shiftKey: true },
];
let SHOULD_EXECUTE = [
  {input: "function foo() { }" },
  {input: "var a = 1;" },
  {input: "function foo() { var a = 1; }" },
  {input: '"asdf"' },
  {input: "99 + 3" },
  {input: "1, 2, 3" },
  // errors
  {input: "function f(x) { let y = 1, }" },
  {input: "function f(x=,) {" },
  {input: "{2,}" },
];

add_task(function* () {
  let { tab, browser } = yield loadTab(TEST_URI);
  let hud = yield openConsole();
  let inputNode = hud.jsterm.inputNode;

  for (let test of SHOULD_ENTER_MULTILINE) {
    hud.jsterm.setInputValue(test.input);
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: test.shiftKey });
    let inputValue = hud.jsterm.getInputValue();
    is(inputNode.selectionStart, inputNode.selectionEnd,
       "selection is collapsed");
    is(inputNode.selectionStart, inputValue.length,
       "caret at end of multiline input");
    let inputWithNewline = test.input + "\n";
    is(inputValue, inputWithNewline, "Input value is correct");
  }

  for (let test of SHOULD_EXECUTE) {
    hud.jsterm.setInputValue(test.input);
    EventUtils.synthesizeKey("VK_RETURN", { shiftKey: test.shiftKey });
    let inputValue = hud.jsterm.getInputValue();
    is(inputNode.selectionStart, 0, "selection starts/ends at 0");
    is(inputNode.selectionEnd, 0, "selection starts/ends at 0");
    is(inputValue, "", "Input value is cleared");
  }

});
