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
add_task(async function() {
  const dbg = await initDebugger("doc-debugger-statements.html");

  await reload(dbg);
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  // resume
  clickElement(dbg, "resume");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  // step over
  await clickStepOver(dbg);
  assertPausedLocation(dbg);

  // step into
  await clickStepIn(dbg);
  assertPausedLocation(dbg);

  // step over
  await clickStepOver(dbg);
  assertPausedLocation(dbg);

  // step out
  await clickStepOut(dbg);
  assertPausedLocation(dbg);
});
