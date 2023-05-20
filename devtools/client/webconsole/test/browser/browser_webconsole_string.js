/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-console.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Test that console.log with a string argument does not include quotes");
  let receivedMessages = waitForMessageByType(hud, "stringLog", ".console-api");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.stringLog();
  });
  await receivedMessages;
  ok(true, "console.log result does not have quotes");

  info(
    "Test that console.log with empty string argument render <empty string>"
  );
  receivedMessages = waitForMessageByType(
    hud,
    "hello <empty string>",
    ".console-api"
  );

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    const name = "";
    content.wrappedJSObject.console.log("hello", name);
  });
  await receivedMessages;
  ok(true, "console.log empty string argument renders as expected");

  info(
    "Test that log with object containing an empty string property renders as expected"
  );
  receivedMessages = waitForMessageByType(
    hud,
    `Object { a: "" }`,
    ".console-api"
  );

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.wrappedJSObject.console.log({ a: "" });
  });
  await receivedMessages;
  ok(true, "object with empty string property renders as expected");

  info("evaluating a string constant");
  let msg = await executeAndWaitForResultMessage(
    hud,
    '"string\\nconstant"',
    "constant"
  );
  let body = msg.node.querySelector(".message-body");
  // On the other hand, a string constant result should be quoted, but
  // newlines should be let through.
  ok(
    body.textContent.includes('"string\nconstant"'),
    `found expected text - "${body.textContent}"`
  );

  info("evaluating an empty string constant");
  msg = await executeAndWaitForResultMessage(hud, '""', '""');
  body = msg.node.querySelector(".message-body");
  ok(body.textContent.includes('""'), `found expected text`);
});
