/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function skipPausing(dbg) {
  clickElementWithSelector(dbg, ".command-bar-skip-pausing");
  return waitForState(dbg, state => dbg.selectors.getSkipPausing());
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
  reload(dbg, "simple3");
  res = await invokeInTab("simple");
  is(res, 3, "simple() successfully completed");
});
