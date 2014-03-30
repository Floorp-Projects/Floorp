/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if draw calls inside a single animation frame generate and retrieve
 * the correct "end result" screenshot.
 */

function ifTestingSupported() {
  let [target, debuggee, front] = yield initCanavsDebuggerBackend(SIMPLE_CANVAS_URL);

  let navigated = once(target, "navigate");

  yield front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  yield navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  let snapshotActor = yield front.recordAnimationFrame();
  ok(snapshotActor,
    "A snapshot actor was sent after recording.");

  let animationOverview = yield snapshotActor.getOverview();
  ok(snapshotActor,
    "An animation overview could be retrieved after recording.");

  let screenshot = animationOverview.screenshot;
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
  is([].find.call(screenshot.pixels, e => e > 0), 4290822336,
    "The screenshot's pixels seem to not be completely transparent.");

  yield removeTab(target.tab);
  finish();
}
