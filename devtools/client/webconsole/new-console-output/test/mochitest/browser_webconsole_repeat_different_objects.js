/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure messages are not considered repeated when console.log()
// is invoked with different objects, see bug 865288.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "new-console-output/test/mochitest/test-repeated-messages.html";

add_task(async function () {
  let hud = await openNewTabAndConsole(TEST_URI);
  hud.jsterm.clearOutput();

  let onMessages = waitForMessages({
    hud,
    messages: [
      { text: "abba" },
      { text: "abba" },
      { text: "abba" },
    ],
  });

  hud.jsterm.execute("window.testConsoleObjects()");

  info("waiting for 3 console.log objects, with the exact same text content");
  let messages = await onMessages;

  is(messages.length, 3, "3 message elements");
});
