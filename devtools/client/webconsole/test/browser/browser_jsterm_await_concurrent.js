/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that multiple concurrent top-level await expressions work as expected.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Web Console test top-level await";

add_task(async function() {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  hud.ui.clearOutput();
  const delays = [3000, 500, 9000, 6000];
  const inputs = delays.map(
    delay => `await new Promise(
    r => setTimeout(() => r("await-concurrent-" + ${delay}), ${delay}))`
  );

  // Let's wait for the message that sould be displayed last.
  const onMessage = waitForMessage(
    hud,
    "await-concurrent-9000",
    ".message.result"
  );
  for (const input of inputs) {
    execute(hud, input);
  }
  await onMessage;

  const messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  const messagesText = Array.from(messages).map(n => n.textContent);
  const expectedMessages = [
    ...inputs,
    `"await-concurrent-500"`,
    `"await-concurrent-3000"`,
    `"await-concurrent-6000"`,
    `"await-concurrent-9000"`,
  ];
  is(
    JSON.stringify(messagesText, null, 2),
    JSON.stringify(expectedMessages, null, 2),
    "The output contains the the expected messages, in the expected order"
  );
});
