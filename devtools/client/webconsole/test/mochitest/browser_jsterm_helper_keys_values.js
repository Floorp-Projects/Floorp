/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html,Test <code>keys()</code> & <code>values()</code> jsterm helper";

add_task(async function() {
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForMessage(
    hud,
    "keys({a: 2, b:1})",
    `Array [ "a", "b" ]`,
    ".result"
  );
  ok(message, "`keys()` worked");

  message = await executeAndWaitForMessage(
    hud,
    "values({a: 2, b:1})",
    "Array [ 2, 1 ]",
    ".result"
  );
  ok(message, "`values()` worked");

  message = await executeAndWaitForMessage(
    hud,
    "keys(window)",
    "Array",
    ".result"
  );
  ok(message, "`keys(window)` worked");
}
