/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if multiple WebGL contexts are correctly handled.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(MULTIPLE_CONTEXTS_URL);
  front.setup();

  let firstProgramActor = yield once(front, "program-linked");
  let secondProgramActor = yield once(front, "program-linked");

  isnot(firstProgramActor, secondProgramActor,
    "Two distinct program actors were recevide from two separate contexts.");

  let firstVertexShader = yield firstProgramActor.getVertexShader();
  let firstFragmentShader = yield firstProgramActor.getFragmentShader();
  let secondVertexShader = yield secondProgramActor.getVertexShader();
  let secondFragmentShader = yield secondProgramActor.getFragmentShader();

  isnot(firstVertexShader, secondVertexShader,
    "The two programs should have distinct vertex shaders.");
  isnot(firstFragmentShader, secondFragmentShader,
    "The two programs should have distinct fragment shaders.");

  let firstVertSource = yield firstVertexShader.getText();
  let firstFragSource = yield firstFragmentShader.getText();
  let secondVertSource = yield secondVertexShader.getText();
  let secondFragSource = yield secondFragmentShader.getText();

  is(firstVertSource, secondVertSource,
    "The vertex shaders should have identical sources.");
  is(firstFragSource, secondFragSource,
    "The vertex shaders should have identical sources.");

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two canvases are correctly drawn.");

  yield firstProgramActor.highlight([1, 0, 0, 1]);
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The first canvas was correctly filled after highlighting.");

  yield secondProgramActor.highlight([0, 1, 0, 1]);
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 0, a: 255 }, true, "#canvas2");
  ok(true, "The second canvas was correctly filled after highlighting.");

  yield firstProgramActor.unhighlight();
  yield secondProgramActor.unhighlight();
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(debuggee, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two canvases were correctly filled after unhighlighting.");

  yield removeTab(target.tab);
  finish();
}
