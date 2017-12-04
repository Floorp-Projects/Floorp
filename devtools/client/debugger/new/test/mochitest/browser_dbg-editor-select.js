/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the editor highlights the correct location when the
// debugger pauses

add_task(async function() {
  // This test runs too slowly on linux debug. I'd like to figure out
  // which is the slowest part of this and make it run faster, but to
  // fix a frequent failure allow a longer timeout.
  requestLongerTimeout(2);

  const dbg = await initDebugger("doc-scripts.html");
  const { selectors: { getSelectedSource }, getState } = dbg;
  const simple1 = findSource(dbg, "simple1.js");
  const simple2 = findSource(dbg, "simple2.js");

  // Set the initial breakpoint.
  await addBreakpoint(dbg, simple1, 4);
  ok(!getSelectedSource(getState()), "No selected source");

  // Call the function that we set a breakpoint in.
  invokeInTab("main");
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "simple1");
  assertPausedLocation(dbg);

  // Step through to another file and make sure it's paused in the
  // right place.
  await stepIn(dbg);
  await waitForLoadedSource(dbg, "simple2");
  assertPausedLocation(dbg);

  // Step back out to the initial file.
  await stepOut(dbg);
  await stepOut(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);

  // Make sure that we can set a breakpoint on a line out of the
  // viewport, and that pausing there scrolls the editor to it.
  let longSrc = findSource(dbg, "long.js");
  await addBreakpoint(dbg, longSrc, 66);

  invokeInTab("testModel");
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "long.js");

  assertPausedLocation(dbg);
  ok(
    isVisibleInEditor(dbg, findElement(dbg, "breakpoint")),
    "Breakpoint is visible"
  );
});
