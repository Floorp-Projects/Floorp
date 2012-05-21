/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the HUD can be accessed via the HUD references in the HUD
// service.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testHUDGetters, false);
}

function testHUDGetters() {
  browser.removeEventListener("DOMContentLoaded", testHUDGetters, false);

  openConsole();

  var HUD = HUDService.getHudByWindow(content);
  var jsterm = HUD.jsterm;
  var klass = jsterm.inputNode.getAttribute("class");
  ok(klass == "jsterm-input-node", "We have the input node.");

  var hudconsole = HUD.console;
  is(typeof hudconsole, "object", "HUD.console is an object");
  is(typeof hudconsole.log, "function", "HUD.console.log is a function");
  is(typeof hudconsole.info, "function", "HUD.console.info is a function");

  finishTest();
}

