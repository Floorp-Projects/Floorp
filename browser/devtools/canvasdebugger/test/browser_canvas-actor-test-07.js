/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if screenshots for non-draw calls can still be retrieved properly,
 * by deferring the the most recent previous draw-call.
 */

function* ifTestingSupported() {
  let { target, front } = yield initCanvasDebuggerBackend(SIMPLE_CANVAS_URL);

  let navigated = once(target, "navigate");

  yield front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  yield navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  let snapshotActor = yield front.recordAnimationFrame();
  let animationOverview = yield snapshotActor.getOverview();

  let functionCalls = animationOverview.calls;
  ok(functionCalls,
    "An array of function call actors was sent after recording.");
  is(functionCalls.length, 8,
    "The number of function call actors is correct.");

  let firstNonDrawCall = yield functionCalls[1].getDetails();
  let secondNonDrawCall = yield functionCalls[3].getDetails();
  let lastNonDrawCall = yield functionCalls[7].getDetails();

  is(firstNonDrawCall.name, "fillStyle",
    "The first non-draw function's name is correct.");
  is(secondNonDrawCall.name, "fillStyle",
    "The second non-draw function's name is correct.");
  is(lastNonDrawCall.name, "requestAnimationFrame",
    "The last non-draw function's name is correct.");

  let firstScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[1]);
  let secondScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[3]);
  let lastScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[7]);

  ok(firstScreenshot,
    "A screenshot was successfully retrieved for the first non-draw function.");
  ok(secondScreenshot,
    "A screenshot was successfully retrieved for the second non-draw function.");
  ok(lastScreenshot,
    "A screenshot was successfully retrieved for the last non-draw function.");

  let firstActualScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[0]);
  ok(sameArray(firstScreenshot.pixels, firstActualScreenshot.pixels),
    "The screenshot for the first non-draw function is correct.");
  is(firstScreenshot.width, 128,
    "The screenshot for the first non-draw function has the correct width.");
  is(firstScreenshot.height, 128,
    "The screenshot for the first non-draw function has the correct height.");

  let secondActualScreenshot =  yield snapshotActor.generateScreenshotFor(functionCalls[2]);
  ok(sameArray(secondScreenshot.pixels, secondActualScreenshot.pixels),
    "The screenshot for the second non-draw function is correct.");
  is(secondScreenshot.width, 128,
    "The screenshot for the second non-draw function has the correct width.");
  is(secondScreenshot.height, 128,
    "The screenshot for the second non-draw function has the correct height.");

  let lastActualScreenshot = yield snapshotActor.generateScreenshotFor(functionCalls[6]);
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

  yield removeTab(target.tab);
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
