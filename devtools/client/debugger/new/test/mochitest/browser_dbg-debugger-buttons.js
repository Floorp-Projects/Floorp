/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function clickStepOver(dbg) {
  clickElement(dbg, "stepOver");
  return waitForPaused(dbg);
}

function clickStepIn(dbg) {
  clickElement(dbg, "stepIn");
  return waitForPaused(dbg);
}

function clickStepOut(dbg) {
  clickElement(dbg, "stepOut");
  return waitForPaused(dbg);
}

/**
 * Test debugger buttons
 *  1. resume
 *  2. stepOver
 *  3. stepIn
 *  4. stepOver to the end of a function
 *  5. stepUp at the end of a function
 */
add_task(function* () {
  const dbg = yield initDebugger("doc-debugger-statements.html");

  yield reload(dbg);
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 8);

  // resume
  clickElement(dbg, "resume");
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 12);

  // step over
  yield clickStepOver(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 13);

  // step into
  yield clickStepIn(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 18);

  // step over
  yield clickStepOver(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 20);

  // step out
  yield clickStepOut(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 20);
});
