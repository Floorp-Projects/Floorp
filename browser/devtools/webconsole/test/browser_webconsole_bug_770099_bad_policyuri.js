/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console CSP messages are displayed

const TEST_BAD_POLICY_URI = "https://example.com/browser/browser/devtools/webconsole/test/test_bug_770099_bad_policy_uri.html";

let hud = undefined;

function test() {
  addTab("data:text/html;charset=utf8,Web Console CSP bad policy URI test");
  browser.addEventListener("load", function _onLoad() {
    browser.removeEventListener("load", _onLoad, true);
    openConsole(null, loadDocument);
  }, true);
}

function loadDocument(theHud) {
  hud = theHud;
  hud.jsterm.clearOutput();
  browser.addEventListener("load", onLoad, true);
  content.location = TEST_BAD_POLICY_URI;
}

function onLoad(aEvent) {
  browser.removeEventListener("load", onLoad, true);

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "can't fetch policy",
      category: CATEGORY_SECURITY,
      severity: SEVERITY_ERROR,
    }],
  }).then(finishTest);
}
