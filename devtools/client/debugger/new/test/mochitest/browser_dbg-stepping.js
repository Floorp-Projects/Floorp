/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test can be really slow on debug platforms
requestLongerTimeout(5);

add_task(async function test() {
  const dbg = await initDebugger("big-sourcemap.html", "big-sourcemap");
  invokeInTab("hitDebugStatement");
  await waitForPaused(dbg, "bundle.js");

  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);

  assertDebugLine(dbg, 42264);
  assertPausedLocation(dbg);
});
