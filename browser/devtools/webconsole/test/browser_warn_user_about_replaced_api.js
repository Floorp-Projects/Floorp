/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_REPLACED_API_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-replaced-api.html";
const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/testscript.js";

function test() {
  waitForExplicitFinish();

  const PREF = "devtools.webconsole.persistlog";
  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(() => Services.prefs.clearUserPref(PREF));

  // First test that the warning does not appear on a page that doesn't override
  // the window.console object.
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testWarningNotPresent);
  }, true);

  function testWarningNotPresent(hud)
  {
    is(hud.outputNode.textContent.indexOf("logging API"), -1,
       "no warning displayed");

    // Bug 862024: make sure the warning doesn't show after page reload.
    info("reload " + TEST_URI);
    executeSoon(() => content.location.reload());

    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "testscript.js",
        category: CATEGORY_NETWORK,
      }],
    }).then(() => executeSoon(() => {
      is(hud.outputNode.textContent.indexOf("logging API"), -1,
         "no warning displayed");

      closeConsole(null, loadTestPage);
    }));
  }

  function loadTestPage()
  {
    info("load test " + TEST_REPLACED_API_URI);
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      openConsole(null, testWarningPresent);
    }, true);
    content.location = TEST_REPLACED_API_URI;
  }

  function testWarningPresent(hud)
  {
    info("wait for the warning to show");
    let warning = {
      webconsole: hud,
      messages: [{
        text: /logging API .+ disabled by a script/,
        category: CATEGORY_JS,
        severity: SEVERITY_WARNING,
      }],
    };

    waitForMessages(warning).then(() => {
      hud.jsterm.clearOutput();

      executeSoon(() => {
        info("reload the test page and wait for the warning to show");
        waitForMessages(warning).then(finishTest);
        content.location.reload();
      });
    });
  }
}
