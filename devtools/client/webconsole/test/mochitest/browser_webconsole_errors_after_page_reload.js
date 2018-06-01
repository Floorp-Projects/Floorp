/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that errors still show up in the Web Console after a page reload.
// See bug 580030: the error handler fails silently after page reload.
// https://bugzilla.mozilla.org/show_bug.cgi?id=580030

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-error.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Reload the content window");
  const onNavigate = hud.target.once("navigate");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.location.reload();
  });
  await onNavigate;
  info("Target navigated");

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  const onMessage = waitForMessage(hud, "fooBazBaz is not defined");
  BrowserTestUtils.synthesizeMouseAtCenter("button", {}, gBrowser.selectedBrowser);
  await onMessage;

  ok(true, "Received the expected error message");
});
