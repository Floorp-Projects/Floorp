/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that cached messages from nested iframes are displayed in the
// Web/Browser Console.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-iframes.html";

const expectedMessages = ["main file", "blah", "iframe 2", "iframe 3"];

const expectedDupedMessage = "iframe 1";

add_task(async function() {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  let hud = await openNewTabAndConsole(TEST_URI);

  await testMessages(hud);
  await closeConsole();
  info("web console closed");

  hud = await BrowserConsoleManager.toggleBrowserConsole();
  await testBrowserConsole(hud);
  await closeConsole();
});

async function testMessages(hud) {
  for (const message of expectedMessages) {
    info(`checking that the message "${message}" exists`);
    await waitFor(() => findMessage(hud, message));
  }

  info("first messages matched");

  const messages = await findMessages(hud, expectedDupedMessage);
  is(messages.length, 2, `${expectedDupedMessage} is present twice`);
}

async function testBrowserConsole(hud) {
  ok(hud, "browser console opened");

  // TODO: The browser console doesn't show page's console.log statements
  // in e10s windows. See Bug 1241289.
  if (Services.appinfo.browserTabsRemoteAutostart) {
    todo(false, "Bug 1241289");
    return;
  }

  await testMessages(hud);
}
