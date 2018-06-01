/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if screenshots for non-draw calls can still be retrieved properly,
 * by deferring the the most recent previous draw-call.
 */

async function ifTestingSupported() {
  const { target, front } = await initCanvasDebuggerBackend(SIMPLE_CANVAS_URL);

  const navigated = once(target, "navigate");

  await front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  const snapshotActor = await front.recordAnimationFrame();
  const animationOverview = await snapshotActor.getOverview();

  const functionCalls = animationOverview.calls;
  ok(functionCalls,
    "An array of function call actors was sent after recording.");
  is(functionCalls.length, 8,
    "The number of function call actors is correct.");

  const firstNonDrawCall = await functionCalls[1].getDetails();
  const secondNonDrawCall = await functionCalls[3].getDetails();
  const lastNonDrawCall = await functionCalls[7].getDetails();

  is(firstNonDrawCall.name, "fillStyle",
    "The first non-draw function's name is correct.");
  is(secondNonDrawCall.name, "fillStyle",
    "The second non-draw function's name is correct.");
  is(lastNonDrawCall.name, "requestAnimationFrame",
    "The last non-draw function's name is correct.");

  const firstScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[1]);
  const secondScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[3]);
  const lastScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[7]);

  ok(firstScreenshot,
    "A screenshot was successfully retrieved for the first non-draw function.");
  ok(secondScreenshot,
    "A screenshot was successfully retrieved for the second non-draw function.");
  ok(lastScreenshot,
    "A screenshot was successfully retrieved for the last non-draw function.");

  const firstActualScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[0]);
  ok(sameArray(firstScreenshot.pixels, firstActualScreenshot.pixels),
    "The screenshot for the first non-draw function is correct.");
  is(firstScreenshot.width, 128,
    "The screenshot for the first non-draw function has the correct width.");
  is(firstScreenshot.height, 128,
    "The screenshot for the first non-draw function has the correct height.");

  const secondActualScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[2]);
  ok(sameArray(secondScreenshot.pixels, secondActualScreenshot.pixels),
    "The screenshot for the second non-draw function is correct.");
  is(secondScreenshot.width, 128,
    "The screenshot for the second non-draw function has the correct width.");
  is(secondScreenshot.height, 128,
    "The screenshot for the second non-draw function has the correct height.");

  const lastActualScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[6]);
  ok(sameArray(lastScreenshot.pixels, lastActualScreenshot.pixels),
    "The screenshot for the last non-draw function is correct.");
  is(lastScreenshot.width, 128,
    "The screenshot for the last non-draw function has the correct width.");
  is(lastScreenshot.height, 128,
    "The screenshot for the last non-draw function has the correct height.");

  ok(!sameArray(firstScreenshot.pixels, secondScreenshot.pixels),
    "The screenshots taken on consecutive draw calls are different (1).");
  ok(!sameArray(secondScreenshot.pixels, lastScreenshot.pixels),
    "The screenshots taken on consecutive draw calls are different (2).");

  await removeTab(target.tab);
  finish();
}

function sameArray(a, b) {
  if (a.length != b.length) {
    return false;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      return false;
    }
  }
  return true;
}
