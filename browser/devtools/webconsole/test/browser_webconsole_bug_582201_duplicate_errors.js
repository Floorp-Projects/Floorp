/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that exceptions thrown by content don't show up twice in the Web
// Console.

const TEST_DUPLICATE_ERROR_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-duplicate-error.html";

function test() {
  expectUncaughtException();
  addTab(TEST_DUPLICATE_ERROR_URI);
  browser.addEventListener("DOMContentLoaded", testDuplicateErrors, false);
}

function testDuplicateErrors() {
  browser.removeEventListener("DOMContentLoaded", testDuplicateErrors,
                              false);
  openConsole(null, function(hud) {
    hud.jsterm.clearOutput();

    Services.console.registerListener(consoleObserver);

    expectUncaughtException();
    content.location.reload();
  });
}

var consoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function (aMessage)
  {
    // we ignore errors we don't care about
    if (!(aMessage instanceof Ci.nsIScriptError) ||
      aMessage.category != "content javascript") {
      return;
    }

    Services.console.unregisterListener(this);

    outputNode = HUDService.getHudByWindow(content).outputNode;

    waitForSuccess({
      name: "fooDuplicateError1 error displayed",
      validatorFn: function()
      {
        return outputNode.textContent.indexOf("fooDuplicateError1") > -1;
      },
      successFn: function()
      {
        let text = outputNode.textContent;
        let error1pos = text.indexOf("fooDuplicateError1");
        ok(error1pos > -1, "found fooDuplicateError1");
        if (error1pos > -1) {
          ok(text.indexOf("fooDuplicateError1", error1pos + 1) == -1,
            "no duplicate for fooDuplicateError1");
        }

        findLogEntry("test-duplicate-error.html");

        finishTest();
      },
      failureFn: finishTest,
    });
  }
};
