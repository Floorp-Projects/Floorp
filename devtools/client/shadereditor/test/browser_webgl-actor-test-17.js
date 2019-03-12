/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the blackbox/unblackbox operations work as expected with
 * overlapping geometry.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(OVERLAPPING_GEOMETRY_CANVAS_URL);
  front.setup({ reload: true });

  const [firstProgramActor, secondProgramActor] = await getPrograms(front, 2);

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct before blackboxing.");

  await firstProgramActor.blackbox();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after blackboxing (1).");

  await firstProgramActor.unblackbox();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after unblackboxing (1).");

  await secondProgramActor.blackbox();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after blackboxing (2).");

  await secondProgramActor.unblackbox();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after unblackboxing (2).");

  await removeTab(target.tab);
  finish();
}
