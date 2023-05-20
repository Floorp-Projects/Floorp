/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that messages are properly updated when the log limit is reached.

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Web Console test for " +
  "Old messages are removed after passing devtools.hud.loglimit";

add_task(async function () {
  await pushPref("devtools.hud.loglimit", 140);
  const hud = await openNewTabAndConsole(TEST_URI);
  await clearOutput(hud);

  let onMessage = waitForMessageByType(
    hud,
    "test message [149]",
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    for (let i = 0; i < 150; i++) {
      content.console.log(`test message [${i}]`);
    }
  });
  await onMessage;

  ok(
    !(await findMessageVirtualizedByType({
      hud,
      text: "test message [0]",
      typeSelector: ".console-api",
    })),
    "Message 0 has been pruned"
  );
  ok(
    !(await findMessageVirtualizedByType({
      hud,
      text: "test message [9]",
      typeSelector: ".console-api",
    })),
    "Message 9 has been pruned"
  );
  ok(
    await findMessageVirtualizedByType({
      hud,
      text: "test message [10]",
      typeSelector: ".console-api",
    }),
    "Message 10 is still displayed"
  );
  is(
    (await findAllMessagesVirtualized(hud)).length,
    140,
    "Number of displayed messages is correct"
  );

  onMessage = waitForMessageByType(hud, "hello world", ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.console.log("hello world");
  });
  await onMessage;

  ok(
    !(await findMessageVirtualizedByType({
      hud,
      text: "test message [10]",
      typeSelector: ".console-api",
    })),
    "Message 10 has been pruned"
  );
  ok(
    await findMessageVirtualizedByType({
      hud,
      text: "test message [11]",
      typeSelector: ".console-api",
    }),
    "Message 11 is still displayed"
  );
  is(
    (await findAllMessagesVirtualized(hud)).length,
    140,
    "Number of displayed messages is still correct"
  );
});
