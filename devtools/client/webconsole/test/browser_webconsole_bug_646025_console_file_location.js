/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that console logging methods display the method location along with
// the output in the console.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console file location " +
                 "display test";
const TEST_URI2 = "http://example.com/browser/devtools/client/" +
                 "webconsole/test/" +
                 "test-bug-646025-console-file-location.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI2);

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "message for level log",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      source: { url: "test-file-location.js", line: 6 },
    },
    {
      text: "message for level info",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_INFO,
      source: { url: "test-file-location.js", line: 7 },
    },
    {
      text: "message for level warn",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_WARNING,
      source: { url: "test-file-location.js", line: 8 },
    },
    {
      text: "message for level error",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_ERROR,
      source: { url: "test-file-location.js", line: 9 },
    },
    {
      text: "message for level debug",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      source: { url: "test-file-location.js", line: 10 },
    }],
  });
});
