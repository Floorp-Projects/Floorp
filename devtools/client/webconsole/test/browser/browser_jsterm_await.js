/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that top-level await expressions work as expected.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Web Console test top-level await";

add_task(async function() {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Evaluate a top-level await expression");
  const simpleAwait = `await new Promise(r => setTimeout(() => r(["await1"]), 500))`;
  await executeAndWaitForResultMessage(hud, simpleAwait, `Array [ "await1" ]`);

  // Check that the resulting promise of the async iife is not displayed.
  const messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  const messagesText = Array.from(messages)
    .map(n => n.textContent)
    .join(" - ");
  is(
    messagesText,
    `${simpleAwait} - Array [ "await1" ]`,
    "The output contains the the expected messages"
  );

  // Check that the timestamp of the result is accurate
  const {
    visibleMessages,
    mutableMessagesById,
  } = hud.ui.wrapper.getStore().getState().messages;
  const [commandId, resultId] = visibleMessages;
  const delta =
    mutableMessagesById.get(resultId).timeStamp -
    mutableMessagesById.get(commandId).timeStamp;
  ok(
    delta >= 500,
    `The result has a timestamp at least 500ms (${delta}ms) older than the command`
  );

  info("Check that assigning the result of a top-level await expression works");
  await executeAndWaitForResultMessage(
    hud,
    `x = await new Promise(r => setTimeout(() => r("await2"), 500))`,
    `await2`
  );

  let message = await executeAndWaitForResultMessage(
    hud,
    `"-" + x + "-"`,
    `"-await2-"`
  );
  ok(message.node, "`x` was assigned as expected");

  info("Check that a logged promise is still displayed as a promise");
  message = await executeAndWaitForResultMessage(
    hud,
    `new Promise(r => setTimeout(() => r(1), 1000))`,
    `Promise {`
  );
  ok(message, "Promise are displayed as expected");

  info("Check that then getters aren't called twice");
  message = await executeAndWaitForResultMessage(
    hud,
    // It's important to keep the last statement of the expression as it covers the original issue.
    // We could execute another expression to get `object.called`, but since we get a preview
    // of the object with an accurate `called` value, this is enough.
    `
    var obj = {
      called: 0,
      get then(){
        this.called++
      }
    };
    await obj`,
    `Object { called: 1, then: Getter }`
  );
});
