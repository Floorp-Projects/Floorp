/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the property provider, which is part of the code completion
// infrastructure.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testPropertyProvider, false);
}

function testPropertyProvider() {
  browser.removeEventListener("DOMContentLoaded", testPropertyProvider,
                              false);

  openConsole();

  var HUD = HUDService.getHudByWindow(content);
  var jsterm = HUD.jsterm;
  var context = jsterm.sandbox.window;
  var completion;

  // Test if the propertyProvider can be accessed from the jsterm object.
  ok (jsterm.propertyProvider !== undefined, "JSPropertyProvider is defined");

  completion = jsterm.propertyProvider(context, "thisIsNotDefined");
  is (completion.matches.length, 0, "no match for 'thisIsNotDefined");

  // This is a case the PropertyProvider can't handle. Should return null.
  completion = jsterm.propertyProvider(context, "window[1].acb");
  is (completion, null, "no match for 'window[1].acb");

  // A very advanced completion case.
  var strComplete =
    'function a() { }document;document.getElementById(window.locatio';
  completion = jsterm.propertyProvider(context, strComplete);
  ok(completion.matches.length == 2, "two matches found");
  ok(completion.matchProp == "locatio", "matching part is 'test'");
  ok(completion.matches[0] == "location", "the first match is 'location'");
  ok(completion.matches[1] == "locationbar", "the second match is 'locationbar'");
  context = completion = null;
  finishTest();
}

