/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function uncaughtException() {
  return invokeInTab("uncaughtException").catch(() => {});
}

function caughtException() {
  return invokeInTab("caughtException");
}

/*
  Tests Pausing on exception
  1. skip an uncaught exception
  2. pause on an uncaught exception
  3. pause on a caught error
  4. skip a caught error
*/
add_task(function* () {
  const dbg = yield initDebugger("doc-exceptions.html");

  // test skipping an uncaught exception
  yield uncaughtException();
  ok(!isPaused(dbg));

  // Test pausing on an uncaught exception
  yield togglePauseOnExceptions(dbg, true, false);
  uncaughtException();
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "exceptions.js", 2);
  yield resume(dbg);

  // Test pausing on a caught Error
  caughtException();
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "exceptions.js", 15);
  yield resume(dbg);

  // Test skipping a caught error
  yield togglePauseOnExceptions(dbg, true, true);
  caughtException();
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "exceptions.js", 17);
  yield resume(dbg);
});
