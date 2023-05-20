/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure that dom errors, with error numbers outside of the range
// of valid js.msg errors, don't cause crashes (See Bug 1270721).

const TEST_URI = "data:text/html,<!DOCTYPE html>Test error documentation";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const text =
    "TypeError: Request constructor: 'foo' (value of 'redirect' member of RequestInit) is not a valid value " +
    "for enumeration RequestRedirect";
  await executeAndWaitForErrorMessage(
    hud,
    "new Request('',{redirect:'foo'})",
    text
  );
  ok(
    true,
    "Error message displayed as expected, without crashing the console."
  );
});
