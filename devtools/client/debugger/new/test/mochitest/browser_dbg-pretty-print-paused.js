/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests pretty-printing a source that is currently paused.

add_task(async function() {
  const dbg = await initDebugger("doc-minified.html");

  await selectSource(dbg, "math.min.js");
  await addBreakpoint(dbg, "math.min.js", 2);

  invokeInTab("arithmetic");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  clickElement(dbg, "prettyPrintButton");
  await waitForDispatch(dbg, "SELECT_SOURCE");

  // this doesnt work yet
  // assertPausedLocation(dbg);

  await resume(dbg);
});
