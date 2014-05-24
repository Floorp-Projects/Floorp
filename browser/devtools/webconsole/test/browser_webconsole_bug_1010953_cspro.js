/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * ***** END LICENSE BLOCK ***** */

/* We are loading:
a script that is allowed by the CSP header but not by the CSPRO header
an image which is allowed by the CSPRO header but not by the CSP header.

So we expect a warning (image has been blocked) and a report
 (script should not load and was reported)

The expected console messages in the constants CSP_VIOLATION_MSG and CSP_REPORT_MSG are confirmed to be found in the console messages.
*/

const TEST_VIOLATION = "http://example.com/browser/browser/devtools/webconsole/test/test_bug_1010953_cspro.html";
const CSP_VIOLATION_MSG = 'Content Security Policy: The page\'s settings blocked the loading of a resource at http://some.example.com/test.png ("img-src http://example.com:80").';
const CSP_REPORT_MSG = 'Content Security Policy: The page\'s settings observed the loading of a resource at http://some.example.com/test_bug_1010953_cspro.js ("script-src http://example.com:80"). A CSP report is being sent.';


let hud = undefined;

function test() {
  addTab("data:text/html;charset=utf8,Web Console CSP report only test (bug 1010953)");
  browser.addEventListener("load", function _onLoad() {
    browser.removeEventListener("load", _onLoad, true);
    openConsole(null, loadDocument);
  }, true);
}

function loadDocument(theHud){
  hud = theHud;
  hud.jsterm.clearOutput()
  browser.addEventListener("load", onLoad, true);
  content.location = TEST_VIOLATION;
}

function onLoad(aEvent) {
  browser.removeEventListener("load", onLoad, true);
  testViolationMessage();
}

function testViolationMessage(){
  let aOutputNode = hud.outputNode;

  waitForSuccess({
      name: "Confirmed that CSP and CSP-Report-Only log different messages to the console.",
      validatorFn: function() {
        console.log(hud.outputNode.textContent);
        let success = false;
        success = hud.outputNode.textContent.indexOf(CSP_VIOLATION_MSG) > -1 &&
                  hud.outputNode.textContent.indexOf(CSP_REPORT_MSG) > -1;
        return success;
      },
      successFn: finishTest,
      failureFn: finishTest,
    });
}
