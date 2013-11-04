/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the blackbox/unblackbox operations work as expected with
 * overlapping geometry.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(OVERLAPPING_GEOMETRY_CANVAS_URL);
  front.setup({ reload: true });

  let firstProgramActor = yield once(front, "program-linked");
  let secondProgramActor = yield once(front, "program-linked");

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct before blackboxing.");

  yield firstProgramActor.blackbox();
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after blackboxing (1).");

  yield firstProgramActor.unblackbox();
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after unblackboxing (1).");

  yield secondProgramActor.blackbox();
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after blackboxing (2).");

  yield secondProgramActor.unblackbox();
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true);
  ok(true, "The corner vs. center pixel colors are correct after unblackboxing (2).");

  yield removeTab(target.tab);
  finish();
}
