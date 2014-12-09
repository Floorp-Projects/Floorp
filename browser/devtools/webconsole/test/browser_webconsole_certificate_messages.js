/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console shows SHA-1 Certificate warnings

const TEST_BAD_URI = "https://sha1ee.example.com/browser/browser/devtools/webconsole/test/test-certificate-messages.html";
const TEST_GOOD_URI = "https://sha256ee.example.com/browser/browser/devtools/webconsole/test/test-certificate-messages.html";
const TRIGGER_MSG = "If you haven't seen ssl warnings yet, you won't";

let gHud = undefined;

function test() {
  registerCleanupFunction(function () {
    gHud = null;
  });

  addTab("data:text/html;charset=utf8,Web Console SHA-1 warning test");
  browser.addEventListener("load", function _onLoad() {
    browser.removeEventListener("load", _onLoad, true);
    openConsole(null, loadBadDocument);
  }, true);
}

function loadBadDocument(theHud) {
  gHud = theHud;
  browser.addEventListener("load", onBadLoad, true);
  content.location = TEST_BAD_URI;
}

function onBadLoad(aEvent) {
  browser.removeEventListener("load", onBadLoad, true);
  testForWarningMessage();
}

function loadGoodDocument(theHud) {
  gHud.jsterm.clearOutput()
  browser.addEventListener("load", onGoodLoad, true);
  content.location = TEST_GOOD_URI;
}

function onGoodLoad(aEvent) {
  browser.removeEventListener("load", onGoodLoad, true);
  testForNoWarning();
}

function testForWarningMessage() {
  let aOutputNode = gHud.outputNode;

  waitForSuccess({
      name: "SHA1 warning displayed successfully",
      validatorFn: function() {
        return gHud.outputNode.textContent.indexOf("SHA-1") > -1;
      },
      successFn: loadGoodDocument,
      failureFn: finishTest,
    });
}

function testForNoWarning() {
  let aOutputNode = gHud.outputNode;

  waitForSuccess({
      name: "SHA1 warning appropriately missed",
      validatorFn: function() {
        if (gHud.outputNode.textContent.indexOf(TRIGGER_MSG) > -1) {
          return gHud.outputNode.textContent.indexOf("SHA-1") == -1;
        }
      },
      successFn: finishTest,
      failureFn: finishTest,
    });
}
