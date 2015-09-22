/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  hud.jsterm.execute("console.log('a log message')");

  let [result] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "a log message",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
  });

  let msg = [...result.matched][0];
  ok(msg.getAttribute("id"), "log message has an ID");
});
