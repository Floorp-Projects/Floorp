/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_REPLACED_API_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-replaced-api.html";

function test() {
  waitForExplicitFinish();

  // First test that the warning does not appear on a normal page (about:blank)
  addTab("about:blank");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    testOpenWebConsole(false);
  }, true);
}

function testWarningPresent() {
  // Then test that the warning does appear on a page that replaces the API
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    testOpenWebConsole(true);
  }, true);
  browser.contentWindow.location = TEST_REPLACED_API_URI;
}

function testOpenWebConsole(shouldWarn) {
  openConsole(null, function(hud) {
    waitForSuccess({
      name: (shouldWarn ? "no " : "") + "API replacement warning",
      validatorFn: function()
      {
        let pos = hud.outputNode.textContent.indexOf("disabled by");
        return shouldWarn ? pos > -1 : pos == -1;
      },
      successFn: function() {
        if (shouldWarn) {
          finishTest();
        }
        else {
          closeConsole(null, testWarningPresent);
        }
      },
      failureFn: finishTest,
    });
  });
}
