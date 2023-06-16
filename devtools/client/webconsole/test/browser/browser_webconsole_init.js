/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { ui } = hud;

  ok(ui.jsterm, "jsterm exists");
  ok(ui.wrapper, "wrapper exists");

  const receievedMessages = waitForMessageByType(hud, "19", ".console-api");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.doLogs(20);
  });

  await receievedMessages;

  const outputContainer = ui.outputNode.querySelector(".webconsole-output");
  is(
    (await findMessagesVirtualizedByType({ hud, typeSelector: ".console-api" }))
      .length,
    20,
    "Correct number of messages appear"
  );
  is(
    outputContainer.scrollWidth,
    outputContainer.clientWidth,
    "No horizontal overflow"
  );
});
