/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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

  info("resume");
  await clickResume(dbg);
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  info("step over");
  await clickStepOver(dbg);
  assertPausedLocation(dbg);

  info("step into");
  await clickStepIn(dbg);
  assertPausedLocation(dbg);

  info("step over");
  await clickStepOver(dbg);
  assertPausedLocation(dbg);

  info("step out");
  await clickStepOut(dbg);
  assertPausedLocation(dbg);
});

function clickButton(dbg, button) {
  const resumeFired = waitForDispatch(dbg.store, "COMMAND");
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
