/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
  Tests Pausing on exception
  1. skip an uncaught exception
  2. pause on an uncaught exception
  3. pause on a caught error
  4. skip a caught error
*/

"use strict";

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
// This is step 9
PromiseTestUtils.allowMatchingRejectionsGlobally(/doesntExists is not defined/);

add_task(async function () {
  const dbg = await initDebugger("doc-exceptions.html", "exceptions.js");
  const source = findSource(dbg, "exceptions.js");

  info("1. test skipping an uncaught exception");
  await uncaughtException();
  assertNotPaused(dbg);

  info("2. Test pausing on an uncaught exception");
  await togglePauseOnExceptions(dbg, true, true);
  uncaughtException();
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);

  const whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(whyPaused, `Paused on exception\nunreachable`);

  await resume(dbg);

  info("2.b Test throwing the same uncaught exception pauses again");
  await togglePauseOnExceptions(dbg, true, true);
  uncaughtException();
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  info("3. Test pausing on a caught Error");
  caughtException();
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 7);

  info("3.b Test pausing in the catch statement");
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 9);
  await resume(dbg);

  info("4. Test skipping a caught error");
  await togglePauseOnExceptions(dbg, true, false);
  caughtException();

  info("4.b Test pausing in the catch statement");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 9);
  await resume(dbg);

  await togglePauseOnExceptions(dbg, true, true);

  info("5. Only pause once when throwing deep in the stack");
  invokeInTab("deepError");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 16);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 22);
  await resume(dbg);

  info("6. Only pause once on an exception when pausing in a finally block");
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

  info("7. Only pause once on an exception when it is rethrown from a catch");
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

  info("8. Pause on each exception thrown while unwinding");
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

  info("9. Pause in throwing new Function argument");
  const onNewSource = waitForDispatch(dbg.store, "ADD_SOURCES");
  invokeInTab("throwInNewFunctionArgument");
  await waitForPaused(dbg);
  const { sources } = await onNewSource;
  is(sources.length, 1, "Got a unique source related to new Function source");
  const newFunctionSource = sources[0];
  is(
    newFunctionSource.url,
    null,
    "This new source looks like the new function one as it has no url"
  );
  assertPausedAtSourceAndLine(dbg, newFunctionSource.id, 1, 0);
  assertTextContentOnLine(dbg, 1, "function anonymous(f=doesntExists()");
  await resume(dbg);
});

function uncaughtException() {
  return invokeInTab("uncaughtException").catch(() => {});
}

function caughtException() {
  return invokeInTab("caughtException");
}
