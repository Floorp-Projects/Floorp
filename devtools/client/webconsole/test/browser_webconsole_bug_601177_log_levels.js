/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 601177: log levels";
const TEST_URI2 = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/test-bug-601177-log-levels.html";

add_task(function* () {
  Services.prefs.setBoolPref("javascript.options.strict", true);

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  yield testLogLevels(hud);

  Services.prefs.clearUserPref("javascript.options.strict");
});

function testLogLevels(hud) {
  content.location = TEST_URI2;

  info("waiting for messages");

  return waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "test-bug-601177-log-levels.html",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_LOG,
      },
      {
        text: "test-bug-601177-log-levels.js",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_LOG,
      },
      {
        text: "test-image.png",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_LOG,
      },
      {
        text: "foobar-known-to-fail.png",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_ERROR,
      },
      {
        text: "foobarBug601177exception",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        text: "undefinedPropertyBug601177",
        category: CATEGORY_JS,
        severity: SEVERITY_WARNING,
      },
      {
        text: "foobarBug601177strictError",
        category: CATEGORY_JS,
        severity: SEVERITY_WARNING,
      },
    ],
  });
}
