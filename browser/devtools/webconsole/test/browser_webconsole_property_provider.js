/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the property provider, which is part of the code completion
// infrastructure.

const TEST_URI = "data:text/html;charset=utf8,<p>test the JS property provider";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", testPropertyProvider, true);
}

function testPropertyProvider() {
  browser.removeEventListener("load", testPropertyProvider, true);

  let tmp = {};
  Cu.import("resource://gre/modules/devtools/WebConsoleUtils.jsm", tmp);
  let JSPropertyProvider = tmp.JSPropertyProvider;
  tmp = null;

  let completion = JSPropertyProvider(content, "thisIsNotDefined");
  is (completion.matches.length, 0, "no match for 'thisIsNotDefined");

  // This is a case the PropertyProvider can't handle. Should return null.
  completion = JSPropertyProvider(content, "window[1].acb");
  is (completion, null, "no match for 'window[1].acb");

  // A very advanced completion case.
  var strComplete =
    'function a() { }document;document.getElementById(window.locatio';
  completion = JSPropertyProvider(content, strComplete);
  ok(completion.matches.length == 2, "two matches found");
  ok(completion.matchProp == "locatio", "matching part is 'test'");
  ok(completion.matches[0] == "location", "the first match is 'location'");
  ok(completion.matches[1] == "locationbar", "the second match is 'locationbar'");

  finishTest();
}

