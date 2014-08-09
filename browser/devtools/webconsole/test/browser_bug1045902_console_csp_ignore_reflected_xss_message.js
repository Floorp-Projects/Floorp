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

let TEST_URI = "data:text/html;charset=utf8,Web Console CSP ignoring reflected XSS (bug 1045902)";

let test = asyncTest(function* () {
  let { browser } = yield loadTab(TEST_URI);

  hud = yield openConsole();

  yield loadDocument(browser);
  yield testViolationMessage();

  hud = null;
});


function loadDocument(browser) {
  let deferred = promise.defer();

  hud.jsterm.clearOutput()
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    deferred.resolve();
  }, true);
  content.location = TEST_FILE;

  return deferred.promise;
}

function testViolationMessage() {
  let deferred = promise.defer();
  let aOutputNode = hud.outputNode;

  return waitForSuccess({
      name: "Confirming that CSP logs messages to the console when 'reflected-xss' directive is used!",
      validator: function() {
        console.log(hud.outputNode.textContent);
        let success = false;
        success = hud.outputNode.textContent.indexOf(EXPECTED_RESULT) > -1;
        return success;
      }
    });
}
