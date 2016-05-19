/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that a file with an unsupported CSP directive ('reflected-xss filter')
// displays the appropriate message to the console.

"use strict";

const EXPECTED_RESULT = "Not supporting directive \u2018reflected-xss\u2019. " +
                        "Directive and values will be ignored.";
const TEST_FILE = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/test_bug1045902_console_csp_ignore_reflected_xss_" +
                  "message.html";

var hud = undefined;

var TEST_URI = "data:text/html;charset=utf8,Web Console CSP ignoring " +
               "reflected XSS (bug 1045902)";

add_task(function* () {
  let { browser } = yield loadTab(TEST_URI);

  hud = yield openConsole();

  yield loadDocument(browser);
  yield testViolationMessage();

  hud = null;
});

function loadDocument(browser) {
  hud.jsterm.clearOutput();
  browser.loadURI(TEST_FILE);
  return BrowserTestUtils.browserLoaded(browser);
}

function testViolationMessage() {
  let aOutputNode = hud.outputNode;

  return waitForSuccess({
    name: "Confirming that CSP logs messages to the console when " +
          "\u2018reflected-xss\u2019 directive is used!",
    validator: function () {
      console.log(aOutputNode.textContent);
      let success = false;
      success = aOutputNode.textContent.indexOf(EXPECTED_RESULT) > -1;
      return success;
    }
  });
}
