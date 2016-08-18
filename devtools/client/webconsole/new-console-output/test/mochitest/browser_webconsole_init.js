/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console.html";

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  let hud = toolbox.getCurrentPanel().hud;
  let {ui} = hud;

  ok(ui.jsterm, "jsterm exists");
  ok(ui.newConsoleOutput, "newConsoleOutput exists");

  // @TODO: fix proptype errors
  let receievedMessages = waitForMessages({
    hud,
    messages: [{
      text: '0',
    }, {
      text: '1',
    }, {
      text: '2',
    }],
  });

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.doLogs(3);
  });

  yield receievedMessages;
});

/**
 * Wait for messages in the web console output, resolving once they are receieved.
 *
 * @param object options
 *        - hud: the webconsole
 *        - messages: Array[Object]. An array of messages to match. Current supported options:
 *            - text: Exact text match in .message-body
 */
function waitForMessages({ hud, messages }) {
  return new Promise(resolve => {
    let numMatched = 0;
    let receivedLog = hud.ui.on("new-messages", function messagesReceieved(e, newMessage) {
      for (let message of messages) {
        if (message.matched) {
          continue;
        }

        if (newMessage.node.querySelector(".message-body").textContent == message.text) {
          numMatched++;
          message.matched = true;
          info("Matched a message with text: " + message.text + ", still waiting for " + (messages.length - numMatched) + " messages");
        }

        if (numMatched === messages.length) {
          hud.ui.off("new-messages", messagesReceieved);
          resolve();
          return;
        }
      }
    });
  });
}