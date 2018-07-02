/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function skipPausing(dbg) {
  clickElementWithSelector(dbg, ".command-bar-skip-pausing");
  return waitForState(dbg, state => dbg.selectors.getSkipPausing(state))
}

add_task(async function() {
  let dbg = await initDebugger("doc-scripts.html");
  await addBreakpoint(dbg, "simple3", 2);

  await skipPausing(dbg);
  const res = await invokeInTab("simple")
  is(res, 3, "simple() successfully completed")
});
