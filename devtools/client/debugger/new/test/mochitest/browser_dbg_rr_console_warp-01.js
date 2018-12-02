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

  const dbg = await attatchRecordingDebugger(
    "doc_rr_error.html", 
    { waitForRecording: true }
  );

  const {tab, toolbox, threadClient} = dbg;
  const console = await getSplitConsole(dbg);
  const hud = console.hud;

  await warpToMessage(hud, threadClient, "Number 5");
  await threadClient.interrupt();

  await checkEvaluateInTopFrame(threadClient, "number", 5);

  // Initially we are paused inside the 'new Error()' call on line 19. The
  // first reverse step takes us to the start of that line.
  await reverseStepOverToLine(threadClient, 19);
  await reverseStepOverToLine(threadClient, 18);
  await setBreakpoint(threadClient, "doc_rr_error.html", 12);
  await rewindToLine(threadClient, 12);
  await checkEvaluateInTopFrame(threadClient, "number", 4);
  await resumeToLine(threadClient, 12);
  await checkEvaluateInTopFrame(threadClient, "number", 5);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
