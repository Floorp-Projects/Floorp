/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that cached messages from nested iframes are displayed in the
// Web/Browser Console.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-iframes.html";

const expectedMessages = [
  ["main file", ".console-api"],
  ["blah", ".error"],
  ["iframe 2", ".console-api"],
  ["iframe 3", ".console-api"],
];

// This log comes from test-iframe1.html, which is included from test-console-iframes.html
// __and__ from test-iframe3.html as well, so we should see it twice.
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

  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  // Enable Fission browser console to see the logged content object
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "browser console opened");
  await testMessages(hud);

  // clear the browser console.
  await clearOutput(hud);
  await waitForTick();
  await safeCloseBrowserConsole();
});

async function testMessages(hud) {
  for (const [message, selector] of expectedMessages) {
    info(`checking that the message "${message}" exists`);
    await waitFor(() => findMessageByType(hud, message, selector));
  }

  ok(true, "Found expected unique messages");

  await waitFor(
    () => findConsoleAPIMessages(hud, expectedDupedMessage).length == 2
  );
  ok(true, `${expectedDupedMessage} is present twice`);
}
