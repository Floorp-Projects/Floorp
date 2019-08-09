/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test copy to clipboard on the console output. See Bug 587617.
const TEST_URI = "data:text/html,Test copy to clipboard on the console output";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const smokeMessage = "Hello world!";
  const onMessage = waitForMessage(hud, smokeMessage);
  ContentTask.spawn(gBrowser.selectedBrowser, smokeMessage, function(msg) {
    content.wrappedJSObject.console.log(msg);
  });
  const { node } = await onMessage;
  ok(true, "Message was logged");

  const selection = selectNode(hud, node);

  const selectionString = selection.toString().trim();
  is(
    selectionString,
    smokeMessage,
    `selection has expected "${smokeMessage}" value`
  );

  await waitForClipboardPromise(
    () => {
      // The focus is on the JsTerm, so we need to blur it for the copy comand to work.
      node.ownerDocument.activeElement.blur();
      goDoCommand("cmd_copy");
    },
    data => {
      return data.trim() === smokeMessage;
    }
  );
});
