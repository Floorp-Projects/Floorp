/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { ui } = hud;

  ok(ui.jsterm, "jsterm exists");
  ok(ui.wrapper, "wrapper exists");

  const receievedMessages = waitForMessages({
    hud,
    messages: [{ text: "19" }],
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.doLogs(20);
  });

  await receievedMessages;

  const outputContainer = ui.outputNode.querySelector(".webconsole-output");
  is(
    outputContainer.querySelectorAll(".message.console-api").length,
    20,
    "Correct number of messages appear"
  );
  is(
    outputContainer.scrollWidth,
    outputContainer.clientWidth,
    "No horizontal overflow"
  );
});
