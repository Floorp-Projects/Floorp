/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
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
