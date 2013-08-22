/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the autocompletion results contain the names of JSTerm helpers.

const TEST_URI = "data:text/html;charset=utf8,<p>test JSTerm Helpers autocomplete";

let testDriver;

function test() {
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

  // Test if 'i' gives 'inspect'
  input.value = "i";
  input.setSelectionRange(1, 1);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield;

  let newItems = popup.getItems().map(function(e) {return e.label;});
  ok(newItems.indexOf("inspect") > -1, "autocomplete results contain helper 'inspect'");
  
  // Test if 'window.' does not give 'inspect'.
  input.value = "window.";
  input.setSelectionRange(7, 7);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield;

  newItems = popup.getItems().map(function(e) {return e.label;});
  is(newItems.indexOf("inspect"), -1, "autocomplete results do not contain helper 'inspect'");


// Test if 'dump(i' gives 'inspect'
  input.value = "dump(i";
  input.setSelectionRange(6, 6);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield;

  newItems = popup.getItems().map(function(e) {return e.label;});
  ok(newItems.indexOf("inspect") > -1, "autocomplete results contain helper 'inspect'");

// Test if 'window.dump(i' gives 'inspect'
  input.value = "window.dump(i";
  input.setSelectionRange(13, 13);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, testNext);
  yield;

  newItems = popup.getItems().map(function(e) {return e.label;});
  ok(newItems.indexOf("inspect") > -1, "autocomplete results contain helper 'inspect'");

  testDriver = jsterm = input = popup = newItems = null;
  executeSoon(finishTest);
  yield;
}
