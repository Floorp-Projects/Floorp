/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test debugging a page with iframes
 *  1. pause in the main thread
 *  2. pause in the iframe
 */
add_task(function* () {
  const dbg = yield initDebugger("doc-iframes.html");

  // test pausing in the main thread
  yield reload(dbg);
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "iframes.html", 8);

  // test pausing in the iframe
  yield resume(dbg);
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 8);

  // test pausing in the iframe
  yield resume(dbg);
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "debugger-statements.html", 12);
});
