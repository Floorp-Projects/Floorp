/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console CSP messages are displayed

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Web Console CSP violation test";
const TEST_VIOLATION = "https://example.com/browser/browser/devtools/" +
                       "webconsole/test/test_bug_770099_violation.html";
const CSP_VIOLATION_MSG = "Content Security Policy: The page's settings " +
                          "blocked the loading of a resource at " +
                          "http://some.example.com/test.png (\"default-src " +
                            "https://example.com\").";

var test = asyncTest(function* () {
  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput();

  let loaded = loadBrowser(browser);
  content.location = TEST_VIOLATION;
  yield loaded;

  yield waitForSuccess({
    name: "CSP policy URI warning displayed successfully",
    validator: function() {
      return hud.outputNode.textContent.indexOf(CSP_VIOLATION_MSG) > -1;
    }
  });
});
