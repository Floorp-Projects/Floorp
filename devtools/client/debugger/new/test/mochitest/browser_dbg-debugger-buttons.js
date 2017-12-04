/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function clickButton(dbg, button) {
  const resumeFired = waitForDispatch(dbg, "COMMAND");
  clickElement(dbg, button);
  return resumeFired;
}

async function clickStepOver(dbg) {
  await clickButton(dbg, "stepOver");
  return waitForPaused(dbg);
}

async function clickStepIn(dbg) {
  await clickButton(dbg, "stepIn");
  return waitForPaused(dbg);
}

async function clickStepOut(dbg) {
  await clickButton(dbg, "stepOut");
  return waitForPaused(dbg);
}

async function clickResume(dbg) {
  return clickButton(dbg, "resume");
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
  await waitForLoadedSource(dbg, "debugger-statements.html");
  assertPausedLocation(dbg);

  // resume
  await clickResume(dbg);
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
