/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that we report JS exceptions in event handlers coming from
// network requests, like onreadystate for XHR. See bug 618078.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 618078";
const TEST_URI2 = "http://example.com/browser/devtools/client/webconsole/" +
                  "new-console-output/test/mochitest/test-network-exceptions.html";

add_task(async function () {
  let hud = await openNewTabAndConsole(TEST_URI);

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  let onMessage = waitForMessage(hud, "bug618078exception");
  await loadDocument(TEST_URI2);
  let { node } = await onMessage;
  ok(true, "Network exception logged as expected.");
  ok(node.classList.contains("error"), "Network exception is logged as error.");
});
