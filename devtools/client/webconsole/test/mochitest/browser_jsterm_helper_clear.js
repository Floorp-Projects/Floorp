/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,Test <code>clear()</code> jsterm helper";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const onMessage = waitForMessage(hud, "message");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.console.log("message");
  });
  await onMessage;

  const onCleared = waitFor(() =>
    hud.jsterm.outputNode.querySelector(".message") === null);
  hud.jsterm.execute("clear()");
  await onCleared;
  ok(true, "Console was cleared");
}
