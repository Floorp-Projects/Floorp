/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests basic pretty-printing functionality.

add_task(async function() {
  const dbg = await initDebugger("doc-minified.html", "math.min.js");

  await selectSource(dbg, "math.min.js", 2);
  clickElement(dbg, "prettyPrintButton");

  await waitForSelectedSource(dbg, "math.min.js:formatted");
  const ppSrc = findSource(dbg, "math.min.js:formatted");

  ok(ppSrc, "Pretty-printed source exists");

  // this is not implemented yet
  // assertHighlightLocation(dbg, "math.min.js:formatted", 18);
  // await selectSource(dbg, "math.min.js")
  await addBreakpoint(dbg, ppSrc, 18);

  invokeInTab("arithmetic");
  await waitForPaused(dbg);

  assertPausedLocation(dbg);

  await stepOver(dbg);

  assertPausedLocation(dbg);

  await resume(dbg);

  // The pretty-print button should go away in the pretty-printed
  // source.
  ok(!findElement(dbg, "prettyPrintButton"), "Pretty Print Button is hidden");

  await selectSource(dbg, "math.min.js");
  ok(findElement(dbg, "prettyPrintButton"), "Pretty Print Button is visible");
});
