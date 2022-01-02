/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that, when the user types an extraneous closing bracket, no error
// appears. See Bug 592442.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>test for bug 592442";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  try {
    await setInputValueForAutocompletion(hud, "document.getElementById)");
    ok(true, "no error was thrown when an extraneous bracket was inserted");
  } catch (ex) {
    ok(false, "an error was thrown when an extraneous bracket was inserted");
  }
});
