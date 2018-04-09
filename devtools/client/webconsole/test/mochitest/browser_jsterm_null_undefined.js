/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html,Test evaluating null and undefined";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  // Check that an evaluated null produces "null". See Bug 650780.
  let onMessage = waitForMessage(hud, `null`);
  jsterm.execute("null");
  let message = await onMessage;
  ok(message, "`null` returned the expected value");

  onMessage = waitForMessage(hud, "undefined");
  jsterm.execute("undefined");
  message = await onMessage;
  ok(message, "`undefined` returned the expected value");
});
