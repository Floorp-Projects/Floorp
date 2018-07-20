/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function removeBreakpoint(dbg) {
  rightClickElement(dbg, "breakpointItem", 3)
  selectMenuItem(dbg, 1);
}


// Tests to see if we can trigger a breakpoint action via the context menu
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "simple2");
  await waitForSelectedSource(dbg, "simple2");

  await addBreakpoint(dbg, "simple2", 3);
  await removeBreakpoint(dbg);

  await waitForState(dbg, state => dbg.selectors.getBreakpoints(state).size == 0);
  ok("successfully removed the breakpoint")
});
