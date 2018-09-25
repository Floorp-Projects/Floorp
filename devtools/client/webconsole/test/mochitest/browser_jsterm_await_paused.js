/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that top-level await expression work as expected when debugger is paused.

"use strict";

const TEST_URI =
  `data:text/html;charset=utf-8,Web Console test top-level await when debugger paused`;

add_task(async function() {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);

  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  const pauseExpression = `(() => {
    var foo = "bar";
    /* Will pause the script and open the debugger panel */
    debugger;
  })()`;
  jsterm.execute(pauseExpression);

  // wait for the debugger to be opened and paused.
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);
  const dbg = await waitFor(() => toolbox.getPanel("jsdebugger"));
  await waitFor(() => dbg._selectors.isPaused(dbg._getState()));
  await toolbox.toggleSplitConsole();

  const onMessage = waitForMessage(hud, "res: bar");
  const awaitExpression = `await new Promise(res => {
    setTimeout(() => res("res: " + foo), 1000);
  })`;
  await jsterm.execute(awaitExpression);

  // Click on the resume button to not be paused anymore.
  dbg.panelWin.document.querySelector("button.resume").click();

  await onMessage;
  const messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  const messagesText = Array.from(messages).map(n => n.textContent);
  const expectedMessages = [
    pauseExpression,
    awaitExpression,
    // The result of pauseExpression
    "undefined",
    "res: bar",
  ];
  is(JSON.stringify(messagesText, null, 2), JSON.stringify(expectedMessages, null, 2),
    "The output contains the the expected messages, in the expected order");
}
