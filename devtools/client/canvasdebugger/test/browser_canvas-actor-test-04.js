/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if draw calls inside a single animation frame generate and retrieve
 * the correct thumbnails.
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
  ok(animationOverview,
    "An animation overview could be retrieved after recording.");

  const thumbnails = animationOverview.thumbnails;
  ok(thumbnails,
    "An array of thumbnails was sent after recording.");
  is(thumbnails.length, 4,
    "The number of thumbnails is correct.");

  is(thumbnails[0].index, 0,
    "The first thumbnail's index is correct.");
  is(thumbnails[0].width, 50,
    "The first thumbnail's width is correct.");
  is(thumbnails[0].height, 50,
    "The first thumbnail's height is correct.");
  is(thumbnails[0].flipped, false,
    "The first thumbnail's flipped flag is correct.");
  is([].find.call(Uint32(thumbnails[0].pixels), e => e > 0), undefined,
    "The first thumbnail's pixels seem to be completely transparent.");

  is(thumbnails[1].index, 2,
    "The second thumbnail's index is correct.");
  is(thumbnails[1].width, 50,
    "The second thumbnail's width is correct.");
  is(thumbnails[1].height, 50,
    "The second thumbnail's height is correct.");
  is(thumbnails[1].flipped, false,
    "The second thumbnail's flipped flag is correct.");
  is([].find.call(Uint32(thumbnails[1].pixels), e => e > 0), 4290822336,
    "The second thumbnail's pixels seem to not be completely transparent.");

  is(thumbnails[2].index, 4,
    "The third thumbnail's index is correct.");
  is(thumbnails[2].width, 50,
    "The third thumbnail's width is correct.");
  is(thumbnails[2].height, 50,
    "The third thumbnail's height is correct.");
  is(thumbnails[2].flipped, false,
    "The third thumbnail's flipped flag is correct.");
  is([].find.call(Uint32(thumbnails[2].pixels), e => e > 0), 4290822336,
    "The third thumbnail's pixels seem to not be completely transparent.");

  is(thumbnails[3].index, 6,
    "The fourth thumbnail's index is correct.");
  is(thumbnails[3].width, 50,
    "The fourth thumbnail's width is correct.");
  is(thumbnails[3].height, 50,
    "The fourth thumbnail's height is correct.");
  is(thumbnails[3].flipped, false,
    "The fourth thumbnail's flipped flag is correct.");
  is([].find.call(Uint32(thumbnails[3].pixels), e => e > 0), 4290822336,
    "The fourth thumbnail's pixels seem to not be completely transparent.");

  await removeTab(target.tab);
  finish();
}

function Uint32(src) {
  const charView = new Uint8Array(src);
  return new Uint32Array(charView.buffer);
}
