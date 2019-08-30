/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test fixes for some simple stepping bugs.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  await addBreakpoint(dbg, "doc_rr_basic.html", 22);
  await rewindToLine(dbg, 22);
  await stepInToLine(dbg, 25);
  await stepOverToLine(dbg, 26);
  await stepOverToLine(dbg, 27);
  await reverseStepOverToLine(dbg, 26);
  await stepInToLine(dbg, 30);
  await stepOverToLine(dbg, 31);
  await stepOverToLine(dbg, 32);

  // Check that the scopes pane shows the value of the local variable.
  for (let i = 1; ; i++) {
    if (getScopeLabel(dbg, i) == "c") {
      is("NaN", getScopeValue(dbg, i));
      break;
    }
  }

  await stepOverToLine(dbg, 33);
  await reverseStepOverToLine(dbg, 32);
  await stepOutToLine(dbg, 27);
  await reverseStepOverToLine(dbg, 26);
  await reverseStepOverToLine(dbg, 25);

  await shutdownDebugger(dbg);
});
