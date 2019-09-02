/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic logpoint functionality in web replay. When logpoints are added,
// new messages should appear in the correct order and allow time warping.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  await selectSource(dbg, "doc_rr_basic.html");
  await addBreakpoint(dbg, "doc_rr_basic.html", 21, undefined, {
    logValue: `"Logpoint Number " + number`,
  });
  await addBreakpoint(dbg, "doc_rr_basic.html", 6, undefined, {
    logValue: `"Logpoint Beginning"`,
  });
  await addBreakpoint(dbg, "doc_rr_basic.html", 8, undefined, {
    logValue: `"Logpoint Ending"`,
  });

  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  const messages = await waitForMessageCount(hud, "Logpoint", 12);

  ok(
    !findMessages(hud, "Loading"),
    "Loading messages should have been removed"
  );

  ok(messages[0].textContent.includes("Beginning"));
  for (let i = 1; i <= 10; i++) {
    ok(messages[i].textContent.includes("Number " + i));
  }
  ok(messages[11].textContent.includes("Ending"));

  await warpToMessage(hud, dbg, "Number 5");

  await checkEvaluateInTopFrame(dbg, "number", 5);
  await reverseStepOverToLine(dbg, 20);

  await shutdownDebugger(dbg);
});
