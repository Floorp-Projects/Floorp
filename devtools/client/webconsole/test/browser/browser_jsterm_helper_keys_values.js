/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html,<!DOCTYPE html>Test <code>keys()</code> & <code>values()</code> jsterm helper";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForResultMessage(
    hud,
    "keys({a: 2, b:1})",
    `Array [ "a", "b" ]`
  );
  ok(message, "`keys()` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "values({a: 2, b:1})",
    "Array [ 2, 1 ]"
  );
  ok(message, "`values()` worked");

  message = await executeAndWaitForResultMessage(hud, "keys(window)", "Array");
  ok(message, "`keys(window)` worked");
});
