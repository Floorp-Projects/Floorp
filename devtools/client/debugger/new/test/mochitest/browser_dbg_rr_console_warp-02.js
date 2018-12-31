/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


// This functionality was copied from devtools/client/webconsole/test/mochitest/head.js,
// since this test straddles both the web console and the debugger. I couldn't
// figure out how to load that script directly here.
function waitForThreadEvents(threadClient, eventName) {
  info(`Waiting for thread event '${eventName}' to fire.`);

  return new Promise(function(resolve, reject) {
    threadClient.addListener(eventName, function onEvent(eventName, ...args) {
      info(`Thread event '${eventName}' fired.`);
      threadClient.removeListener(eventName, onEvent);
      resolve.apply(resolve, args);
    });
  });
}


// Test basic console time warping functionality in web replay.
async function test() {
  waitForExplicitFinish();

  const dbg = await attachRecordingDebugger(
    "doc_rr_logs.html", 
    { waitForRecording: true }
  );

  const {tab, toolbox, threadClient} = dbg;
  const console = await getSplitConsole(dbg);
  const hud = console.hud;

  let message = await warpToMessage(hud, threadClient, "number: 1");
  ok(!message.classList.contains("paused-before"), "paused before message is not shown");

  await stepOverToLine(threadClient, 18);
  await reverseStepOverToLine(threadClient, 17);

  message = findMessage(hud, "number: 1")
  ok(message.classList.contains("paused-before"), "paused before message is shown");

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
