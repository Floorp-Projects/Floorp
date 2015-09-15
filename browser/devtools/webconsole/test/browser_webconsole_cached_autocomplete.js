/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the cached autocomplete results are used when the new
// user input is a subset of the existing completion results.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test cached autocompletion " +
                 "results";

var jsterm;

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  jsterm = hud.jsterm;
  let input = jsterm.inputNode;
  let popup = jsterm.autocompletePopup;

  // Test if 'doc' gives 'document'
  input.value = "doc";
  input.setSelectionRange(3, 3);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  is(input.value, "doc", "'docu' completion (input.value)");
  is(jsterm.completeNode.value, "   ument", "'docu' completion (completeNode)");

  // Test typing 'window.'.
  input.value = "window.";
  input.setSelectionRange(7, 7);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  ok(popup.getItems().length > 0, "'window.' gave a list of suggestions");

  yield jsterm.execute("window.docfoobar = true");

  // Test typing 'window.doc'.
  input.value = "window.doc";
  input.setSelectionRange(10, 10);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  let newItems = popup.getItems();
  ok(newItems.every(function(item) {
    return item.label != "docfoobar";
  }), "autocomplete cached results do not contain docfoobar. list has not " +
      "been updated");

  // Test that backspace does not cause a request to the server
  input.value = "window.do";
  input.setSelectionRange(9, 9);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  newItems = popup.getItems();
  ok(newItems.every(function(item) {
    return item.label != "docfoobar";
  }), "autocomplete cached results do not contain docfoobar. list has not " +
      "been updated");

  yield jsterm.execute("delete window.docfoobar");

  // Test if 'window.getC' gives 'getComputedStyle'
  input.value = "window.";
  input.setSelectionRange(7, 7);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  input.value = "window.getC";
  input.setSelectionRange(11, 11);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  newItems = popup.getItems();
  ok(!newItems.every(function(item) {
       return item.label != "getComputedStyle";
     }), "autocomplete results do contain getComputedStyle");

  // Test if 'dump(d' gives non-zero results
  input.value = "dump(d";
  input.setSelectionRange(6, 6);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  ok(popup.getItems().length > 0, "'dump(d' gives non-zero results");

  // Test that 'dump(window.)' works.
  input.value = "dump(window.)";
  input.setSelectionRange(12, 12);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  yield jsterm.execute("window.docfoobar = true");

  // Make sure 'dump(window.doc)' does not contain 'docfoobar'.
  input.value = "dump(window.doc)";
  input.setSelectionRange(15, 15);
  yield complete(jsterm.COMPLETE_HINT_ONLY);

  newItems = popup.getItems();
  ok(newItems.every(function(item) {
    return item.label != "docfoobar";
  }), "autocomplete cached results do not contain docfoobar. list has not " +
      "been updated");

  yield jsterm.execute("delete window.docfoobar");

  jsterm = null;
});

function complete(type) {
  let updated = jsterm.once("autocomplete-updated");
  jsterm.complete(type);
  return updated;
}
