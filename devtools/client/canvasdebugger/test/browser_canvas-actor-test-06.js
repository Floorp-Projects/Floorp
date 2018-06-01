/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if screenshots for arbitrary draw calls are generated properly.
 */

async function ifTestingSupported() {
  const { target, front } = await initCanvasDebuggerBackend(SIMPLE_CANVAS_TRANSPARENT_URL);

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

  is(functionCalls[0].name, "clearRect",
    "The first called function's name is correct.");
  is(functionCalls[2].name, "fillRect",
    "The second called function's name is correct.");
  is(functionCalls[4].name, "fillRect",
    "The third called function's name is correct.");
  is(functionCalls[6].name, "fillRect",
    "The fourth called function's name is correct.");

  const firstDrawCallScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[0]);
  const secondDrawCallScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[2]);
  const thirdDrawCallScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[4]);
  const fourthDrawCallScreenshot = await snapshotActor.generateScreenshotFor(functionCalls[6]);

  ok(firstDrawCallScreenshot,
    "The first draw call has a screenshot attached.");
  is(firstDrawCallScreenshot.index, 0,
    "The first draw call has the correct screenshot index.");
  is(firstDrawCallScreenshot.width, 128,
    "The first draw call has the correct screenshot width.");
  is(firstDrawCallScreenshot.height, 128,
    "The first draw call has the correct screenshot height.");
  is([].find.call(Uint32(firstDrawCallScreenshot.pixels), e => e > 0), undefined,
    "The first draw call's screenshot's pixels seems to be completely transparent.");

  ok(secondDrawCallScreenshot,
    "The second draw call has a screenshot attached.");
  is(secondDrawCallScreenshot.index, 2,
    "The second draw call has the correct screenshot index.");
  is(secondDrawCallScreenshot.width, 128,
    "The second draw call has the correct screenshot width.");
  is(secondDrawCallScreenshot.height, 128,
    "The second draw call has the correct screenshot height.");
  is([].find.call(Uint32(firstDrawCallScreenshot.pixels), e => e > 0), undefined,
    "The second draw call's screenshot's pixels seems to be completely transparent.");

  ok(thirdDrawCallScreenshot,
    "The third draw call has a screenshot attached.");
  is(thirdDrawCallScreenshot.index, 4,
    "The third draw call has the correct screenshot index.");
  is(thirdDrawCallScreenshot.width, 128,
    "The third draw call has the correct screenshot width.");
  is(thirdDrawCallScreenshot.height, 128,
    "The third draw call has the correct screenshot height.");
  is([].find.call(Uint32(thirdDrawCallScreenshot.pixels), e => e > 0), 2160001024,
    "The third draw call's screenshot's pixels seems to not be completely transparent.");

  ok(fourthDrawCallScreenshot,
    "The fourth draw call has a screenshot attached.");
  is(fourthDrawCallScreenshot.index, 6,
    "The fourth draw call has the correct screenshot index.");
  is(fourthDrawCallScreenshot.width, 128,
    "The fourth draw call has the correct screenshot width.");
  is(fourthDrawCallScreenshot.height, 128,
    "The fourth draw call has the correct screenshot height.");
  is([].find.call(Uint32(fourthDrawCallScreenshot.pixels), e => e > 0), 2147483839,
    "The fourth draw call's screenshot's pixels seems to not be completely transparent.");

  isnot(firstDrawCallScreenshot.pixels, secondDrawCallScreenshot.pixels,
    "The screenshots taken on consecutive draw calls are different (1).");
  isnot(secondDrawCallScreenshot.pixels, thirdDrawCallScreenshot.pixels,
    "The screenshots taken on consecutive draw calls are different (2).");
  isnot(thirdDrawCallScreenshot.pixels, fourthDrawCallScreenshot.pixels,
    "The screenshots taken on consecutive draw calls are different (3).");

  await removeTab(target.tab);
  finish();
}

function Uint32(src) {
  const charView = new Uint8Array(src);
  return new Uint32Array(charView.buffer);
}
