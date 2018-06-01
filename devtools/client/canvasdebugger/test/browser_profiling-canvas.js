/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if functions inside a single animation frame are recorded and stored
 * for a canvas context profiling.
 */

async function ifTestingSupported() {
  const currentTime = window.performance.now();
  const { target, front } = await initCanvasDebuggerBackend(SIMPLE_CANVAS_URL);

  const navigated = once(target, "navigate");

  await front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  const snapshotActor = await front.recordAnimationFrame();
  ok(snapshotActor,
    "A snapshot actor was sent after recording.");

  const animationOverview = await snapshotActor.getOverview();
  ok(animationOverview,
    "An animation overview could be retrieved after recording.");

  const functionCalls = animationOverview.calls;
  ok(functionCalls,
    "An array of function call actors was sent after recording.");
  is(functionCalls.length, 8,
    "The number of function call actors is correct.");

  info("Check the timestamps of function calls");

  for (let i = 0; i < functionCalls.length - 1; i += 2) {
    ok(functionCalls[i].timestamp > 0, "The timestamp of the called function is larger than 0.");
    ok(functionCalls[i].timestamp < currentTime, "The timestamp has been minus the frame start time.");
    ok(functionCalls[i + 1].timestamp >= functionCalls[i].timestamp, "The timestamp of the called function is correct.");
  }

  await removeTab(target.tab);
  finish();
}
