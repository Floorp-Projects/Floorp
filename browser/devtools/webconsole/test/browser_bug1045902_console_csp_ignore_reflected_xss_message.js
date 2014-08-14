/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Description of the test:
 * We are loading a file with the following CSP:
 *     'reflected-xss filter'
 * This directive is not supported, hence we confirm that
 * the according message is displayed in the web console.
 */

const EXPECTED_RESULT = "Not supporting directive 'reflected-xss'. Directive and values will be ignored.";
const TEST_FILE = "http://example.com/browser/browser/devtools/webconsole/test/" +
                  "test_bug1045902_console_csp_ignore_reflected_xss_message.html";

let hud = undefined;

function test() {
  addTab("data:text/html;charset=utf8,Web Console CSP ignoring reflected XSS (bug 1045902)");
  browser.addEventListener("load", function _onLoad() {
    browser.removeEventListener("load", _onLoad, true);
    openConsole(null, loadDocument);
  }, true);
}

function loadDocument(theHud) {
  hud = theHud;
  hud.jsterm.clearOutput()
  browser.addEventListener("load", onLoad, true);
  content.location = TEST_FILE;
}

function onLoad(aEvent) {
  browser.removeEventListener("load", onLoad, true);
  testViolationMessage();
}

function testViolationMessage() {
  let aOutputNode = hud.outputNode;

  waitForSuccess({
      name: "Confirming that CSP logs messages to the console when 'reflected-xss' directive is used!",
      validatorFn: function() {
        console.log(hud.outputNode.textContent);
        let success = false;
        success = hud.outputNode.textContent.indexOf(EXPECTED_RESULT) > -1;
        return success;
      },
      successFn: finishTest,
      failureFn: finishTest,
    });
}
