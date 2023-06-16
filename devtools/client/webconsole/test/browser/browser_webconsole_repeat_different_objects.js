/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure messages are not considered repeated when console.log()
// is invoked with different objects, see bug 865288.

"use strict";

const TEST_URI = "data:text/html,<!DOCTYPE html>Test repeated objects";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const onMessages = waitForMessagesByType({
    hud,
    messages: [
      {
        text: "abba",
        typeSelector: ".console-api",
      },
      {
        text: "abba",
        typeSelector: ".console-api",
      },
      {
        text: "abba",
        typeSelector: ".console-api",
      },
    ],
  });

  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    for (let i = 0; i < 3; i++) {
      const o = { id: "abba" };
      content.console.log("abba", o);
    }
  });

  info("waiting for 3 console.log objects, with the exact same text content");
  const messages = await onMessages;
  is(messages.length, 3, "There are 3 messages, as expected.");
});
