/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console.html";

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);

  info("console.log with a string argument");
  let receievedMessages = waitForMessages({
    hud,
    messages: [{
      // Test that the output does not include quotes.
      text: "stringLog",
    }],
  });

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.wrappedJSObject.stringLog();
  });

  yield receievedMessages;

  info("evaluating a string constant");
  let jsterm = hud.jsterm;
  yield jsterm.execute("\"string constant\"");
  let msg = yield waitFor(() => findMessage(hud, "constant"));
  let body = msg.querySelector(".message-body");
  // On the other hand, a string constant result should be quoted.
  ok(body.textContent.includes("\"string constant\""), "found expected text");
});
