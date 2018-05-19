/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function clickButton(dbg, button) {
  const resumeFired = waitForDispatch(dbg, "COMMAND");
  clickElement(dbg, button);
  return resumeFired;
}

function clickReplayButton(dbg, button) {
  const replayFired = waitForDispatch(dbg, "TRAVEL_TO");
  clickElement(dbg, button);
  return replayFired;
}

async function clickStepOver(dbg) {
  await clickButton(dbg, "stepOver");
  return waitForPaused(dbg);
}

async function clickStepBack(dbg) {
  await clickReplayButton(dbg, "replayPrevious");
  return waitForPaused(dbg);
}

async function clickStepForward(dbg) {
  await clickReplayButton(dbg, "replayNext");
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

function assertHistoryPosition(dbg, position) {
  const {
    selectors: { getHistoryPosition, getHistoryFrame },
    getState
  } = dbg;

  ok(
    getHistoryPosition(getState()) === position - 1,
    "has correct position in history"
  );
}

/**
 * Test debugger replay buttons
 *  1. pause
 *  2. step back
 *  3. step Forward
 *  4. resume
 */
add_task(async function() {
  await pushPref("devtools.debugger.features.replay", true);

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

  // step back
  await clickStepBack(dbg);
  assertPausedLocation(dbg);
  assertHistoryPosition(dbg, 4);

  // step back
  await clickStepBack(dbg);
  assertPausedLocation(dbg);
  assertHistoryPosition(dbg, 3);

  // step back
  await clickStepBack(dbg);
  assertPausedLocation(dbg);
  assertHistoryPosition(dbg, 2);

  // step back
  await clickStepBack(dbg);
  assertPausedLocation(dbg);
  assertHistoryPosition(dbg, 1);

  // step forward
  await clickStepForward(dbg);
  assertPausedLocation(dbg);
  assertHistoryPosition(dbg, 2);

  // step forward
  await clickStepForward(dbg);
  assertPausedLocation(dbg);
  assertHistoryPosition(dbg, 3);

  // resume
  await clickResume(dbg);
  assertHistoryPosition(dbg, 0);
});
