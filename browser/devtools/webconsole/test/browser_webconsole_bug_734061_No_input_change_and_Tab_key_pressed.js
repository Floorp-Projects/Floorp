/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testInputChange);
  }, true);
}

function testInputChange(hud) {
  var jsterm = hud.jsterm;
  var input = jsterm.inputNode;

  is(input.getAttribute("focused"), "true", "input has focus");
  EventUtils.synthesizeKey("VK_TAB", {});
  is(input.getAttribute("focused"), "", "focus moved away");

  // Test user changed something
  input.focus();
  EventUtils.synthesizeKey("A", {});
  EventUtils.synthesizeKey("VK_TAB", {});
  is(input.getAttribute("focused"), "true", "input is still focused");

  // Test non empty input but not changed since last focus
  input.blur();
  input.focus();
  EventUtils.synthesizeKey("VK_RIGHT", {});
  EventUtils.synthesizeKey("VK_TAB", {});
  is(input.getAttribute("focused"), "", "input moved away");

  jsterm = input = null;
  finishTest();
}
