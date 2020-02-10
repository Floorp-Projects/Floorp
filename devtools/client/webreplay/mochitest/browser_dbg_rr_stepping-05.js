/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test stepping in pretty-printed code.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_minified.html", {
    waitForRecording: true,
  });

  await selectSource(dbg, "minified.js");
  await prettyPrint(dbg);

  await dbg.actions.addEventListenerBreakpoints(["event.mouse.click"]);
  await dbg.actions.toggleEventLogging();

  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  await warpToMessage(hud, dbg, "click", 12);
  await stepInToLine(dbg, 2);
  await stepOutToLine(dbg, 12);
  await stepInToLine(dbg, 9);
  await stepOutToLine(dbg, 13);
  await stepInToLine(dbg, 5);
  await stepOutToLine(dbg, 14);

  await shutdownDebugger(dbg);
});
