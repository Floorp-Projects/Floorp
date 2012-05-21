/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that code completion works properly.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testCompletion, false);
}

function testCompletion() {
  browser.removeEventListener("DOMContentLoaded", testCompletion, false);

  openConsole();

  var jsterm = HUDService.getHudByWindow(content).jsterm;
  var input = jsterm.inputNode;

  // Test typing 'docu'.
  input.value = "docu";
  input.setSelectionRange(4, 4);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  is(input.value, "docu", "'docu' completion");
  is(jsterm.completeNode.value, "    ment", "'docu' completion");

  // Test typing 'docu' and press tab.
  input.value = "docu";
  input.setSelectionRange(4, 4);
  jsterm.complete(jsterm.COMPLETE_FORWARD);
  is(input.value, "document", "'docu' tab completion");
  is(input.selectionStart, 8, "start selection is alright");
  is(input.selectionEnd, 8, "end selection is alright");
  is(jsterm.completeNode.value.replace(/ /g, ""), "", "'docu' completed");

  // Test typing 'document.getElem'.
  input.value = "document.getElem";
  input.setSelectionRange(16, 16);
  jsterm.complete(jsterm.COMPLETE_FORWARD);
  is(input.value, "document.getElem", "'document.getElem' completion");
  is(jsterm.completeNode.value, "                entById", "'document.getElem' completion");

  // Test pressing tab another time.
  jsterm.complete(jsterm.COMPLETE_FORWARD);
  is(input.value, "document.getElem", "'document.getElem' completion");
  is(jsterm.completeNode.value, "                entsByClassName", "'document.getElem' another tab completion");

  // Test pressing shift_tab.
  jsterm.complete(jsterm.COMPLETE_BACKWARD);
  is(input.value, "document.getElem", "'document.getElem' untab completion");
  is(jsterm.completeNode.value, "                entById", "'document.getElem' completion");

  jsterm.clearOutput();
  jsterm.history.splice(0, jsterm.history.length);   // workaround for bug 592552

  input.value = "docu";
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  is(jsterm.completeNode.value, "    ment", "'docu' completion");
  jsterm.execute();
  is(jsterm.completeNode.value, "", "clear completion on execute()");

  // Test multi-line completion works
  input.value =                 "console.log('one');\nconsol";
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  is(jsterm.completeNode.value, "                   \n      e", "multi-line completion");

  jsterm = input = null;
  finishTest();
}

