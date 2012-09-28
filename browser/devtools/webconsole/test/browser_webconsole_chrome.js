/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that code completion works properly.

function test() {
  addTab("about:addons");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testChrome);
  }, true);
}

function testChrome(hud) {
  ok(hud, "we have a console");
  
  ok(hud.iframe, "we have the console iframe");

  let jsterm = hud.jsterm;
  ok(jsterm, "we have a jsterm");

  let input = jsterm.inputNode;
  ok(hud.outputNode, "we have an output node");

  // Test typing 'docu'.
  input.value = "docu";
  input.setSelectionRange(4, 4);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY, function() {
    is(jsterm.completeNode.value, "    ment", "'docu' completion");
    executeSoon(finishTest);
  });
}

