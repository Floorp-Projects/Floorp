/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html,Test <code>keys()</code> & <code>values()</code> jsterm helper";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  let onMessage = waitForMessage(hud, `Array [ "a", "b" ]`);
  jsterm.execute("keys({a: 2, b:1})");
  let message = await onMessage;
  ok(message, "`keys()` worked");

  onMessage = waitForMessage(hud, "Array [ 2, 1 ]");
  jsterm.execute("values({a: 2, b:1})");
  message = await onMessage;
  ok(message, "`values()` worked");

  onMessage = waitForMessage(hud, "Array");
  jsterm.execute("keys(window)");
  message = await onMessage;
  ok(message, "`keys(window)` worked");
}
