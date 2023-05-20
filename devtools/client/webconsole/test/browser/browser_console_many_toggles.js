/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we safely close and reopen the Browser Console.

"use strict";

add_task(async function () {
  // Enable the multiprocess mode as it is more likely to break on startup
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const promises = [];
  for (let i = 0; i < 5; i++) {
    info("Open the Browser Console");
    promises.push(BrowserConsoleManager.toggleBrowserConsole());

    // Use different pause time between opening and closing
    await wait(i * 100);

    info("Close the Browser Console");
    promises.push(BrowserConsoleManager.closeBrowserConsole());

    // Use different pause time between opening and closing
    await wait(i * 100);
  }

  info("Wait for all opening/closing promises");
  // Ignore any exception here, we expect some as we are racing opening versus destruction
  await Promise.allSettled(promises);

  // The browser console may end up being opened or closed because of usage of "toggle"
  // Ensure having a console opened to verify it works
  let hud = BrowserConsoleManager.getBrowserConsole();
  if (!hud) {
    info("Reopen the browser console a last time");
    hud = await BrowserConsoleManager.toggleBrowserConsole();
  }

  info("Log a message and ensure it is visible and the console mostly works");
  console.log("message from mochitest");
  await checkUniqueMessageExists(hud, "message from mochitest", ".console-api");

  // Clear the messages in order to be able to run this test more than once
  // and clear the message we just logged
  await clearOutput(hud);

  info("Ensure closing the Browser Console at the end of the test");
  await BrowserConsoleManager.closeBrowserConsole();

  ok(
    !BrowserConsoleManager.getBrowserConsole(),
    "No browser console opened at the end of test"
  );
});
