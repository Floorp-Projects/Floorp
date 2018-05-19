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
add_task(async function() {
  const dbg = await initDebugger("doc-exceptions.html");

  log("1. test skipping an uncaught exception");
  await uncaughtException();
  ok(!isPaused(dbg));

  log("2. Test pausing on an uncaught exception");
  await togglePauseOnExceptions(dbg, true, true);
  uncaughtException();
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);
  await waitForActive(dbg);

  log("3. Test pausing on a caught Error");
  caughtException();
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  log("3.b Test pausing in the catch statement");
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);

  log("4. Test skipping a caught error");
  await togglePauseOnExceptions(dbg, true, false);
  caughtException();

  log("4.b Test pausing in the catch statement");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  await resume(dbg);
});
