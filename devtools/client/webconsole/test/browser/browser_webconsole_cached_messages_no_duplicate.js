/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to see if we don't get duplicated messages (cached and "live").
// See Bug 1578138 for more information.

"use strict";

// Log 1 message every 50ms, until we reach 50 messages.
const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8><script>
    var i = 0;
    var intervalId = setInterval(() => {
      if (i >= 50) {
        clearInterval(intervalId);
        intervalId = null;
        return;
      }
      console.log("startup message " + (++i));
    }, 50);
  </script>`;

add_task(async function () {
  info("Add a tab and open the console");
  const tab = await addTab(TEST_URI, { waitForLoad: false });
  const hud = await openConsole(tab);

  info("wait until all the messages are displayed");
  await waitFor(
    () =>
      findConsoleAPIMessage(hud, "message 1") &&
      findConsoleAPIMessage(hud, "message 50")
  );

  is(
    (await findAllMessagesVirtualized(hud)).length,
    50,
    "We have the expected number of messages"
  );
});
