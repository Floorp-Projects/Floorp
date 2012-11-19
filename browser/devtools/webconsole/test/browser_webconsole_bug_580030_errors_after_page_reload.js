/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that errors still show up in the Web Console after a page reload.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-error.html";

function test() {
  expectUncaughtException();
  addTab(TEST_URI);
  browser.addEventListener("load", onLoad, true);
}

// see bug 580030: the error handler fails silently after page reload.
// https://bugzilla.mozilla.org/show_bug.cgi?id=580030
function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, onLoad, true);

  openConsole(null, function(hud) {
    hud.jsterm.clearOutput();
    browser.addEventListener("load", testErrorsAfterPageReload, true);
    content.location.reload();
  });
}

function testErrorsAfterPageReload(aEvent) {
  browser.removeEventListener(aEvent.type, testErrorsAfterPageReload, true);

  // dispatch a click event to the button in the test page and listen for
  // errors.

  Services.console.registerListener(consoleObserver);

  let button = content.document.querySelector("button").wrappedJSObject;
  ok(button, "button found");
  EventUtils.sendMouseEvent({type: "click"}, button, content.wrappedJSObject);
}

var consoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aMessage)
  {
    // Ignore errors we don't care about.
    if (!(aMessage instanceof Ci.nsIScriptError) ||
      aMessage.category != "content javascript") {
      return;
    }

    Services.console.unregisterListener(this);

    let outputNode = HUDService.getHudByWindow(content).outputNode;

    waitForSuccess({
      name: "error message after page reload",
      validatorFn: function()
      {
        return outputNode.textContent.indexOf("fooBazBaz") > -1;
      },
      successFn: finishTest,
      failureFn: finishTest,
    });
  }
};

