/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests async stepping will:
// 1. step over await statements
// 2. step into async functions
// 3. step out of async functions
add_task(async function test() {
  Services.prefs.setBoolPref("devtools.debugger.features.async-stepping", true);
  const dbg = await initDebugger("doc-async.html", "async");

  await selectSource(dbg, "async");
  await addBreakpoint(dbg, "async", 8);
  invokeInTab("main");

  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 8);

  await stepOver(dbg);
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 9);
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(
    "devtools.debugger.features.async-stepping",
    false
  );
});
