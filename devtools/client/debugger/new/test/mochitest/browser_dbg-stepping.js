/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 // This test can be really slow on debug platforms
 requestLongerTimeout(3);

function assertSelectedFile(dbg, url) {
  const selectedLocation = dbg.selectors.getSelectedSource(dbg.getState());
  ok(selectedLocation, "The debugger should be at the paused location")
  const selectedUrl = selectedLocation.get("url");
  return ok( selectedUrl.includes(url), `The debugger should be paused at ${url}"`);
}

add_task(async function test() {
  const dbg = await initDebugger("big-sourcemap.html", "big-sourcemap");
  invokeInTab("hitDebugStatement");
  await waitForPaused(dbg);
  assertSelectedFile(dbg, "bundle.js");

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

  assertSelectedFile(dbg, "bundle.js");
  assertDebugLine(dbg, 42264);
  assertPausedLocation(dbg);
});
