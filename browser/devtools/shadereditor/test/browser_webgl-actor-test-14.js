/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the rendering is updated when a uniform variable is
 * changed in one shader of a page with multiple WebGL contexts.
 */

function ifWebGLSupported() {
  let { target, front } = yield initBackend(MULTIPLE_CONTEXTS_URL);
  front.setup({ reload: true });

  let [firstProgramActor, secondProgramActor] = yield getPrograms(front, 2);

  let firstFragmentShader = yield firstProgramActor.getFragmentShader();
  let secondFragmentShader = yield secondProgramActor.getFragmentShader();

  let oldFragSource = yield firstFragmentShader.getText();
  let newFragSource = oldFragSource.replace("vec4(uColor", "vec4(0.25, 0.25, 0.25");
  let status = yield firstFragmentShader.compile(newFragSource);
  ok(!status,
    "The first new fragment shader source was compiled without errors.");

  yield front.waitForFrame();
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 64, g: 64, b: 64, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 64, g: 64, b: 64, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The first fragment shader was changed.");

  oldFragSource = yield secondFragmentShader.getText();
  newFragSource = oldFragSource.replace("vec4(uColor", "vec4(0.75, 0.75, 0.75");
  status = yield secondFragmentShader.compile(newFragSource);
  ok(!status,
    "The second new fragment shader source was compiled without errors.");

  yield front.waitForFrame();
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 64, g: 64, b: 64, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 64, g: 64, b: 64, a: 255 }, true, "#canvas1");
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 191, g: 191, b: 191, a: 255 }, true, "#canvas2");
  yield ensurePixelIs(front, { x: 127, y: 127 }, { r: 191, g: 191, b: 191, a: 255 }, true, "#canvas2");
  ok(true, "The second fragment shader was changed.");

  yield removeTab(target.tab);
  finish();
}
