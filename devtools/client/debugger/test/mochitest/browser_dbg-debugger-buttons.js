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

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-debugger-statements.html");

  let onReloaded = reload(dbg, "doc-debugger-statements.html");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    11
  );

  info("resume");
  await clickResume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    16
  );

  info("step over");
  await clickStepOver(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    17
  );

  info("step into");
  await clickStepIn(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    22
  );

  info("step over");
  await clickStepOver(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    24
  );

  info("step out");
  await clickStepOut(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    18
  );

  await resume(dbg);

  info("Wait for reload to complete after resume");
  await onReloaded;

  info("Toggle debugger statement off");
  const toggleCheckbox = findElementWithSelector(
    dbg,
    ".breakpoints-debugger-statement input"
  );
  toggleCheckbox.click();
  await waitFor(() => !toggleCheckbox.checked);

  await reload(dbg, "doc-debugger-statements.html");
  assertNotPaused(dbg);

  info("Re-enable debugger statement");
  toggleCheckbox.click();
  await waitFor(() => toggleCheckbox.checked);

  onReloaded = reload(dbg, "doc-debugger-statements.html");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    11
  );
  await resume(dbg);
  await waitForPaused(dbg);
  await resume(dbg);
  await onReloaded;
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
