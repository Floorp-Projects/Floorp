/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the cached autocomplete results are used when the new
// user input is a subset of the existing completion results.

const TEST_URI = "data:text/html;charset=utf8,<p>test cached autocompletion results";

let testDriver;

function test() {
  requestLongerTimeout(2);
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, function(hud) {
      testDriver = testCompletion(hud);
      testDriver.next();
    });
  }, true);
}

function testNext() {
  executeSoon(function() {
    testDriver.next();
  });
}

function testCompletion(hud) {
  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;
  let popup = jsterm.autocompletePopup;

  // Test if 'doc' gives 'document'
  input.value = "doc";
  input.setSelectionRange(3, 3);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  is(input.value, "doc", "'docu' completion (input.value)");
  is(jsterm.completeNode.value, "   ument", "'docu' completion (completeNode)");

  // Test typing 'window.'.
  input.value = "window.";
  input.setSelectionRange(7, 7);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  ok(popup.getItems().length > 0, "'window.' gave a list of suggestions")

  content.wrappedJSObject.docfoobar = true;

  // Test typing 'window.doc'.
  input.value = "window.doc";
  input.setSelectionRange(10, 10);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  let newItems = popup.getItems();
  ok(newItems.every(function(item) {
       return item.label != "docfoobar";
     }), "autocomplete cached results do not contain docfoobar. list has not been updated");

  // Test that backspace does not cause a request to the server
  input.value = "window.do";
  input.setSelectionRange(9, 9);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(newItems.every(function(item) {
       return item.label != "docfoobar";
     }), "autocomplete cached results do not contain docfoobar. list has not been updated");

  delete content.wrappedJSObject.docfoobar;

  // Test if 'window.getC' gives 'getComputedStyle'
  input.value = "window."
  input.setSelectionRange(7, 7);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;
  input.value = "window.getC";
  input.setSelectionRange(11, 11);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;
  newItems = popup.getItems();
  ok(!newItems.every(function(item) {
       return item.label != "getComputedStyle";
     }), "autocomplete results do contain getComputedStyle");

  // Test if 'dump(d' gives non-zero results
  input.value = "dump(d";
  input.setSelectionRange(6, 6);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  ok(popup.getItems().length > 0, "'dump(d' gives non-zero results");

  // Test that 'dump(window.)' works.
  input.value = "dump(window.)";
  input.setSelectionRange(12, 12);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  content.wrappedJSObject.docfoobar = true;

  // Make sure 'dump(window.doc)' does not contain 'docfoobar'.
  input.value = "dump(window.doc)";
  input.setSelectionRange(15, 15);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield undefined;

  newItems = popup.getItems();
  ok(newItems.every(function(item) {
       return item.label != "docfoobar";
     }), "autocomplete cached results do not contain docfoobar. list has not been updated");

  delete content.wrappedJSObject.docfoobar;

  testDriver = null;
  executeSoon(finishTest);
  yield undefined;
}
