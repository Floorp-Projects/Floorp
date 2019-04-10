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
  const dbg = await initDebugger("doc-exceptions.html", "exceptions.js");
  const source = findSource(dbg, "exceptions.js");

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

  log("2.b Test throwing the same uncaught exception pauses again");
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

  await togglePauseOnExceptions(dbg, true, true);

  log("5. Only pause once when throwing deep in the stack");
  invokeInTab("deepError");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 16);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 22);
  await resume(dbg);

  log("6. Only pause once on an exception when pausing in a finally block");
  invokeInTab("deepErrorFinally");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 34);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 31);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 40);
  await resume(dbg);

  log("7. Only pause once on an exception when it is rethrown from a catch");
  invokeInTab("deepErrorCatch");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 53);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 49);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 59);
  await resume(dbg);

  log("8. Pause on each exception thrown while unwinding");
  invokeInTab("deepErrorThrowDifferent");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 71);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 68);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 77);
  await resume(dbg);
});
