/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check instanceof correctness. See Bug 599940.
const TEST_URI = "data:text/html,Test <code>instanceof</code> evaluation";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  let onMessage = waitForMessage(hud, "true");
  jsterm.execute("[] instanceof Array");
  let message = await onMessage;
  ok(message, "`instanceof Array` is correct");

  onMessage = waitForMessage(hud, "true");
  jsterm.execute("({}) instanceof Object");
  message = await onMessage;
  ok(message, "`instanceof Object` is correct");

  onMessage = waitForMessage(hud, "false");
  jsterm.execute("({}) instanceof Array");
  message = await onMessage;
  ok(message, "`instanceof Array` has expected result");
});
