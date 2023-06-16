/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html,<!DOCTYPE html>Test evaluating null and undefined";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  // Check that an evaluated null produces "null". See Bug 650780.
  let message = await executeAndWaitForResultMessage(hud, "null", "null");
  ok(message, "`null` returned the expected value");

  message = await executeAndWaitForResultMessage(hud, "undefined", "undefined");
  ok(message, "`undefined` returned the expected value");
});
