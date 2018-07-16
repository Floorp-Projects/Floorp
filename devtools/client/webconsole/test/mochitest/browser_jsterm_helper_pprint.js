/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,Test <code>pprint()</code> jsterm helper";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  let onMessage = waitForMessage(hud, `"  b: 2\n  a: 1"`);
  jsterm.execute("pprint({b:2, a:1})");
  let message = await onMessage;
  ok(message, "`pprint()` worked");

  // check that pprint(window) does not throw (see Bug 608358).
  onMessage = waitForMessage(hud, `window:`);
  jsterm.execute("pprint(window)");
  message = await onMessage;
  ok(message, "`pprint(window)` worked");

  // check that calling pprint with a string does not throw (See Bug 614561).
  onMessage = waitForMessage(hud, `"  0: \\"h\\"\n  1: \\"i\\""`);
  jsterm.execute("pprint('hi')");
  message = await onMessage;
  ok(message, "`pprint('hi')` worked");

  // check that pprint(function) shows function source (See Bug 618344).
  onMessage = waitForMessage(hud, `"function() { var someCanaryValue = 42; }`);
  jsterm.execute("pprint(function() { var someCanaryValue = 42; })");
  message = await onMessage;
  ok(message, "`pprint(function)` shows function source");
}
