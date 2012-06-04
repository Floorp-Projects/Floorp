/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-613013-console-api-iframe.html";

let TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aMessage, aTopic, aData)
  {
    if (aTopic == "console-api-log-event") {
      executeSoon(performTest);
    }
  }
};

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, tabLoad, true);

  openConsole(null, function(aHud) {
    hud = aHud;
    Services.obs.addObserver(TestObserver, "console-api-log-event", false);
    content.location.reload();
  });
}

function performTest() {
  Services.obs.removeObserver(TestObserver, "console-api-log-event");
  TestObserver = null;

  waitForSuccess({
    name: "console.log() message",
    validatorFn: function()
    {
      return hud.outputNode.textContent.indexOf("foobarBug613013") > -1;
    },
    successFn: finishTest,
    failureFn: finishTest,
  });
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}

