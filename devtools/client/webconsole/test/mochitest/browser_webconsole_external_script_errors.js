/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 597136.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/" +
                 "test-external-script-errors.html";

add_task(async function() {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  const hud = await openNewTabAndConsole(TEST_URI);

  const onMessage = waitForMessage(hud, "bogus is not defined");
  BrowserTestUtils.synthesizeMouseAtCenter("button", {}, gBrowser.selectedBrowser);
  await onMessage;

  ok(true, "Received the expected message");
});
