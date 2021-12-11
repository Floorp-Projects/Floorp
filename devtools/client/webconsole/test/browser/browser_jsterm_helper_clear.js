/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,Test <code>clear()</code> jsterm helper";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const onMessage = waitForMessage(hud, "message");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("message");
  });
  await onMessage;

  const onCleared = waitFor(
    () => hud.ui.outputNode.querySelector(".message") === null
  );
  execute(hud, "clear()");
  await onCleared;
  ok(true, "Console was cleared");
});
