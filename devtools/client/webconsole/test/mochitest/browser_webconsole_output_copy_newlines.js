/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that multiple messages are copied into the clipboard and that they are
// separated by new lines. See bug 916997.
const TEST_URI = "data:text/html,<meta charset=utf8>" +
                 "Test copy multiple messages to clipboard";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const messages = Array.from({length: 10}, (_, i) => `Message number ${i + 1}`);
  const lastMessage = [...messages].pop();
  const onMessage = waitForMessage(hud, lastMessage);
  ContentTask.spawn(gBrowser.selectedBrowser, messages, msgs => {
    msgs.forEach(msg => content.wrappedJSObject.console.log(msg));
  });
  const {node} = await onMessage;
  ok(node, "Messages were logged");

  // Select the whole output.
  const output = node.closest(".webconsole-output");
  selectNode(hud, output);

  info("Wait for the clipboard to contain the text corresponding to all the messages");
  await waitForClipboardPromise(
    () => {
      // The focus is on the JsTerm, so we need to blur it for the copy comand to work.
      output.ownerDocument.activeElement.blur();
      goDoCommand("cmd_copy");
    },
    data => data.trim() === messages.join("\n")
  );
});
