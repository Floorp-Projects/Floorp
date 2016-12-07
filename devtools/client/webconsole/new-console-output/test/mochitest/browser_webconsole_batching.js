/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check adding console calls as batch keep the order of the message.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-batching.html";
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  const messageNumber = 100;
  yield testSimpleBatchLogging(hud, messageNumber);
  yield testBatchLoggingAndClear(hud, messageNumber);
});

function* testSimpleBatchLogging(hud, messageNumber) {
  yield ContentTask.spawn(gBrowser.selectedBrowser, messageNumber,
    function (numMessages) {
      content.wrappedJSObject.batchLog(numMessages);
    }
  );

  for (let i = 0; i < messageNumber; i++) {
    let node = yield waitFor(() => findMessageAtIndex(hud, i, i));
    is(node.textContent, i.toString(), `message at index "${i}" is the expected one`);
  }
}

function* testBatchLoggingAndClear(hud, messageNumber) {
  yield ContentTask.spawn(gBrowser.selectedBrowser, messageNumber,
    function (numMessages) {
      content.wrappedJSObject.batchLogAndClear(numMessages);
    }
  );
  yield waitFor(() => findMessage(hud, l10n.getStr("consoleCleared")));
  ok(true, "console cleared message is displayed");

  // Passing the text argument as an empty string will returns all the message,
  // whatever their content is.
  const messages = findMessages(hud, "");
  is(messages.length, 1, "console was cleared as expected");
}

function findMessageAtIndex(hud, text, index) {
  const selector = `.message:nth-of-type(${index + 1}) .message-body`;
  return findMessage(hud, text, selector);
}
