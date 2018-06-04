/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that messages are properly updated when the log limit is reached.

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "Old messages are removed after passing devtools.hud.loglimit";

add_task(async function() {
  await pushPref("devtools.hud.loglimit", 140);
  const hud = await openNewTabAndConsole(TEST_URI);
  hud.ui.clearOutput();

  let onMessage = waitForMessage(hud, "test message [149]");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    for (let i = 0; i < 150; i++) {
      content.console.log(`test message [${i}]`);
    }
  });
  await onMessage;

  ok(!findMessage(hud, "test message [0]"), "Message 0 has been pruned");
  ok(!findMessage(hud, "test message [9]"), "Message 9 has been pruned");
  ok(findMessage(hud, "test message [10]"), "Message 10 is still displayed");
  is(findMessages(hud, "").length, 140, "Number of displayed messages is correct");

  onMessage = waitForMessage(hud, "hello world");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    content.console.log("hello world");
  });
  await onMessage;

  ok(!findMessage(hud, "test message [10]"), "Message 10 has been pruned");
  ok(findMessage(hud, "test message [11]"), "Message 11 is still displayed");
  is(findMessages(hud, "").length, 140, "Number of displayed messages is still correct");
});
