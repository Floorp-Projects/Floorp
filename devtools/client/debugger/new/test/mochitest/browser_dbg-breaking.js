/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the breakpoints are hit in various situations.

add_task(function* () {
  const dbg = yield initDebugger("doc-scripts.html", "scripts.html");
  const { selectors: { getSelectedSource }, getState } = dbg;

  // Make sure we can set a top-level breakpoint and it will be hit on
  // reload.
  yield addBreakpoint(dbg, "scripts.html", 18);
  reload(dbg);
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "scripts.html", 18);
  yield resume(dbg);

  const paused = waitForPaused(dbg);

  // Create an eval script that pauses itself.
  invokeInTab("doEval");

  yield paused;
  yield resume(dbg);
  const source = getSelectedSource(getState()).toJS();
  // TODO: The url of an eval source should be null.
  ok(source.url.indexOf("SOURCE") === 0, "It is an eval source");

  yield addBreakpoint(dbg, source, 5);
  invokeInTab("evaledFunc");
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, source, 5);
});
