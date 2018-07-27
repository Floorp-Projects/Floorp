/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure that dom errors, with error numbers outside of the range
// of valid js.msg errors, don't cause crashes (See Bug 1270721).

const TEST_URI = "data:text/html,Test error documentation";

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

  const text = "TypeError: 'redirect' member of RequestInit 'foo' is not a valid value " +
               "for enumeration RequestRedirect";
  const onErrorMessage =  waitForMessage(hud, text, ".message.error");
  jsterm.execute("new Request('',{redirect:'foo'})");
  await onErrorMessage;
  ok(true, "Error message displayed as expected, without crashing the console.");
}
