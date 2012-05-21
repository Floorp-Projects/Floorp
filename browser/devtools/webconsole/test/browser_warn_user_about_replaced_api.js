/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_REPLACED_API_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-replaced-api.html";

function test() {
  waitForExplicitFinish();

  // First test that the warning does not appear on a normal page (about:blank)
  addTab("about:blank");
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);
    testOpenWebConsole(false);
    executeSoon(testWarningPresent);
  }, true);
}

function testWarningPresent() {
  // Then test that the warning does appear on a page that replaces the API
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);
    testOpenWebConsole(true);
    finishTest();
  }, true);
  browser.contentWindow.location = TEST_REPLACED_API_URI;
}

function testOpenWebConsole(shouldWarn) {
  openConsole();

  hud = HUDService.getHudByWindow(content);
  ok(hud, "WebConsole was opened");

  let msg = (shouldWarn ? "found" : "didn't find") + " API replacement warning";
  testLogEntry(hud.outputNode, "disabled", msg, false, !shouldWarn);
}
