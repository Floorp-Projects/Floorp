/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that network log messages bring up the network panel.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-618078-network-exceptions.html";

let testEnded = false;

let TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aSubject)
  {
    if (testEnded || !(aSubject instanceof Ci.nsIScriptError)) {
      return;
    }

    is(aSubject.category, "content javascript", "error category");

    testEnded = true;
    if (aSubject.category == "content javascript") {
      executeSoon(checkOutput);
    }
    else {
      executeSoon(finishTest);
    }
  }
};

function checkOutput()
{
  waitForSuccess({
    name: "exception message",
    validatorFn: function()
    {
      return hud.outputNode.textContent.indexOf("bug618078exception") > -1;
    },
    successFn: finishTest,
    failureFn: finishTest,
  });
}

function testEnd()
{
  Services.console.unregisterListener(TestObserver);
}

function test()
{
  addTab("data:text/html;charset=utf-8,Web Console test for bug 618078");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function(aHud) {
      hud = aHud;
      Services.console.registerListener(TestObserver);
      registerCleanupFunction(testEnd);

      executeSoon(function() {
        expectUncaughtException();
        content.location = TEST_URI;
      });
    });
  }, true);
}

