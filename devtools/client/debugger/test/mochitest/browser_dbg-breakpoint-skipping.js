/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
  const disabled = waitForDispatch(dbg, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}

/*
 * Tests toggling the skip pausing button and
 * invoking functions without pausing.
 */

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "simple3");
  await addBreakpoint(dbg, "simple3", 2);

  await skipPausing(dbg);
  let res = await invokeInTab("simple");
  is(res, 3, "simple() successfully completed");

  info("Reload and invoke again");
  await reload(dbg, "simple3");
  res = await invokeInTab("simple");
  is(res, 3, "simple() successfully completed");

  info("Adding a breakpoint disables skipPausing");
  await addBreakpoint(dbg, "simple3", 3);
  await waitForState(dbg, state => !state.skipPausing);

  info("Enabling a breakpoint disables skipPausing");
  await skipPausing(dbg);
  await disableBreakpoint(dbg, 0);
  await toggleBreakpoint(dbg, 0);
  await waitForState(dbg, state => !state.skipPausing);
});
