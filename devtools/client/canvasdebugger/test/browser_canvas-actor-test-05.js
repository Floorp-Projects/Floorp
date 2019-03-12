/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if draw calls inside a single animation frame generate and retrieve
 * the correct "end result" screenshot.
 */

async function ifTestingSupported() {
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
  ok(snapshotActor,
    "An animation overview could be retrieved after recording.");

  const screenshot = animationOverview.screenshot;
  ok(screenshot,
    "A screenshot was sent after recording.");

  is(screenshot.index, 6,
    "The screenshot's index is correct.");
  is(screenshot.width, 128,
    "The screenshot's width is correct.");
  is(screenshot.height, 128,
    "The screenshot's height is correct.");
  is(screenshot.flipped, false,
    "The screenshot's flipped flag is correct.");
  is([].find.call(Uint32(screenshot.pixels), e => e > 0), 4290822336,
    "The screenshot's pixels seem to not be completely transparent.");

  await removeTab(target.tab);
  finish();
}

function Uint32(src) {
  const charView = new Uint8Array(src);
  return new Uint32Array(charView.buffer);
}
