/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests pretty-printing a source that is currently paused.

add_task(function* () {
  const dbg = yield initDebugger("doc-minified.html");

  yield selectSource(dbg, "math.min.js");
  yield addBreakpoint(dbg, "math.min.js", 2);

  invokeInTab("arithmetic");
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "math.min.js", 2);

  clickElement(dbg, "prettyPrintButton");
  yield waitForDispatch(dbg, "TOGGLE_PRETTY_PRINT");

  assertPausedLocation(dbg, "math.min.js:formatted", 18);

  yield resume(dbg);
});
