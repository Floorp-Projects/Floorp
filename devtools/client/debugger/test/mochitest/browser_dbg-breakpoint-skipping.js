/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function skipPausing(dbg) {
  clickElementWithSelector(dbg, ".command-bar-skip-pausing");
  return waitForState(dbg, state => dbg.selectors.getSkipPausing(state))
}

/*
 * Tests toggling the skip pausing button and 
 * invoking functions without pausing.
 */

add_task(async function() {
  let dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "simple3")
  await addBreakpoint(dbg, "simple3", 2);

  await skipPausing(dbg);
  let res = await invokeInTab("simple");
  is(res, 3, "simple() successfully completed");

  info("Reload and invoke again");
  reload(dbg, "simple3");
  res = await invokeInTab("simple");
  is(res, 3, "simple() successfully completed");
});
