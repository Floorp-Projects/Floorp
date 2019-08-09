/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See Bug 597756. Check that errors are still displayed in the console after
// reloading a page.

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-reopen-closed-tab.html";

add_task(async function() {
  // If we persist log, the test might be successful even if only the first
  // error log is shown.
  pushPref("devtools.webconsole.persistlog", false);

  info("Open console and refresh tab.");

  expectUncaughtExceptionNoE10s();
  let hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  expectUncaughtExceptionNoE10s();
  await refreshTab();
  await waitForError(hud);

  // Close and reopen
  await closeConsole();

  expectUncaughtExceptionNoE10s();
  gBrowser.removeCurrentTab();
  hud = await openNewTabAndConsole(TEST_URI);

  expectUncaughtExceptionNoE10s();
  await refreshTab();
  await waitForError(hud);
});

async function waitForError(hud) {
  info("Wait for error message");
  await waitFor(() => findMessage(hud, "fooBug597756_error", ".message.error"));
  ok(true, "error message displayed");
}

function expectUncaughtExceptionNoE10s() {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }
}
