/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests toggling the skip pausing button and
 * invoking functions without pausing.
 */

"use strict";

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "simple3.js");

  info("Adding a breakpoint should remove the skipped pausing state");
  await skipPausing(dbg);
  await waitForState(dbg, state => dbg.selectors.getSkipPausing());
  await addBreakpoint(dbg, "simple3.js", 2);
  await waitForState(dbg, state => !dbg.selectors.getSkipPausing());
  invokeInTab("simple");
  await waitForPaused(dbg);
  ok(true, "The breakpoint has been hit after a breakpoint was created");
  await resume(dbg);

  info("Toggling a breakpoint should remove the skipped pausing state");
  // First disable the breakpoint to ensure skip pausing gets turned off
  // during a disable
  await skipPausing(dbg);
  await disableBreakpoint(dbg, 0);
  await waitForState(dbg, state => !dbg.selectors.getSkipPausing());
  // Then re-enable the breakpoint to ensure skip pausing gets turned off
  // during an enable
  await skipPausing(dbg);
  await waitForState(dbg, state => dbg.selectors.getSkipPausing());
  toggleBreakpoint(dbg, 0);
  await waitForState(dbg, state => !dbg.selectors.getSkipPausing());
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");
  invokeInTab("simple");
  await waitForPaused(dbg);
  ok(true, "The breakpoint has been hit after skip pausing was disabled");
  await resume(dbg);

  info("Disabling a breakpoint should remove the skipped pausing state");
  await addBreakpoint(dbg, "simple3.js", 3);
  await skipPausing(dbg);
  await disableBreakpoint(dbg, 0);
  await waitForState(dbg, state => !dbg.selectors.getSkipPausing());
  invokeInTab("simple");
  await waitForPaused(dbg);
  ok(true, "The breakpoint has been hit after skip pausing was disabled again");
  await resume(dbg);

  info("Removing a breakpoint should remove the skipped pause state");
  toggleBreakpoint(dbg, 0);
  await skipPausing(dbg);
  const source = findSource(dbg, "simple3.js");
  removeBreakpoint(dbg, source.id, 3);
  const wait = waitForDispatch(dbg.store, "TOGGLE_SKIP_PAUSING");
  await waitForState(dbg, state => !dbg.selectors.getSkipPausing());
  await wait;
  invokeInTab("simple");
  await waitForPaused(dbg);
  // Unfortunately required as the test harness throws if preview doesn't
  // complete before the end of the test.
  await waitForDispatch(dbg.store, "ADD_INLINE_PREVIEW");
  ok(true, "Breakpoint is hit after a breakpoint was removed");
  await resume(dbg);
});

function skipPausing(dbg) {
  clickElementWithSelector(dbg, ".command-bar-skip-pausing");
  return waitForState(dbg, state => dbg.selectors.getSkipPausing());
}

function toggleBreakpoint(dbg, index) {
  const breakpoints = findAllElements(dbg, "breakpointItems");
  const bp = breakpoints[index];
  const input = bp.querySelector("input");
  input.click();
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}
