/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Blackboxing, then stepping past the beginning or end of a frame should act
// like a step-out.
add_task(async function() {
  info("Start debugger");
  const dbg = await attachRecordingDebugger("doc_rr_blackbox.html", {
    waitForRecording: true,
  });

  info("Step forward from in blackboxed source");
  await addBreakpoint(dbg, "blackbox.js", 3);
  await rewindToLine(dbg, 3, "blackbox.js");
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg, "BLACKBOX");
  await stepOverToLine(dbg, 20, "doc_rr_blackbox.html");

  info("Unblackbox");
  await selectSource(dbg, "blackbox.js");
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg, "BLACKBOX");

  info("Step backward from in blackboxed source");
  await rewindToLine(dbg, 3, "blackbox.js");
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg, "BLACKBOX");
  await reverseStepOverToLine(dbg, 15, "doc_rr_blackbox.html");

  info("Step forward when called from blackboxed source");
  await addBreakpoint(dbg, "doc_rr_blackbox.html", 17);
  await resumeToLine(dbg, 17, "doc_rr_blackbox.html");
  await stepOverToLine(dbg, 17, "doc_rr_blackbox.html");
  await stepOverToLine(dbg, 20, "doc_rr_blackbox.html");

  info("Step backward when called from blackboxed source");
  await rewindToLine(dbg, 17, "doc_rr_blackbox.html");
  await reverseStepOverToLine(dbg, 15, "doc_rr_blackbox.html");

  info("Unblackbox 2");
  await selectSource(dbg, "blackbox.js");
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg, "BLACKBOX");

  info("Finish");
  await shutdownDebugger(dbg);
});
