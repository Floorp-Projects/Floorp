/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that we report JS exceptions in event handlers coming from
// network requests, like onreadystate for XHR. See bug 618078.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 618078";
const TEST_URI2 = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/test-bug-618078-network-exceptions.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI2);

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug618078exception",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  });
});
