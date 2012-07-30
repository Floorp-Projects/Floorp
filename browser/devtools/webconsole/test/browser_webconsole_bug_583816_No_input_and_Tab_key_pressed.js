/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testCompletion);
  }, true);
}

function testCompletion(hud) {
  var jsterm = hud.jsterm;
  var input = jsterm.inputNode;

  jsterm.setInputValue("");
  EventUtils.synthesizeKey("VK_TAB", {});
  is(jsterm.completeNode.value, "<- no result", "<- no result - matched");
  is(input.value, "", "inputnode is empty - matched")
  is(input.getAttribute("focused"), "true", "input is still focused");

  //Any thing which is not in property autocompleter
  jsterm.setInputValue("window.Bug583816");
  EventUtils.synthesizeKey("VK_TAB", {});
  is(jsterm.completeNode.value, "                <- no result", "completenode content - matched");
  is(input.value, "window.Bug583816", "inputnode content - matched");
  is(input.getAttribute("focused"), "true", "input is still focused");

  jsterm = input = null;
  finishTest();
}
