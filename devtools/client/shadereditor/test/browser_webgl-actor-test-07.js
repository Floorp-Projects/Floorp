/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that vertex and fragment shader sources can be changed.
 */

function* ifWebGLSupported() {
  let { target, front } = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = yield once(front, "program-linked");
  let vertexShader = yield programActor.getVertexShader();
  let fragmentShader = yield programActor.getFragmentShader();

  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 128, y: 128 }, { r: 191, g: 64, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);

  let vertSource = yield vertexShader.getText();
  let fragSource = yield fragmentShader.getText();
  ok(!vertSource.includes("2.0"),
    "The vertex shader source is correct before changing it.");
  ok(!fragSource.includes("0.5"),
    "The fragment shader source is correct before changing it.");

  let newVertSource = vertSource.replace("1.0", "2.0");
  let status = yield vertexShader.compile(newVertSource);
  ok(!status,
    "The new vertex shader source was compiled without errors.");

  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 128, y: 128 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);

  vertSource = yield vertexShader.getText();
  fragSource = yield fragmentShader.getText();
  ok(vertSource.includes("2.0"),
    "The vertex shader source is correct after changing it.");
  ok(!fragSource.includes("0.5"),
    "The fragment shader source is correct after changing the vertex shader.");

  let newFragSource = fragSource.replace("1.0", "0.5");
  status = yield fragmentShader.compile(newFragSource);
  ok(!status,
    "The new fragment shader source was compiled without errors.");

  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 128, y: 128 }, { r: 255, g: 0, b: 0, a: 127 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);

  vertSource = yield vertexShader.getText();
  fragSource = yield fragmentShader.getText();
  ok(vertSource.includes("2.0"),
    "The vertex shader source is correct after changing the fragment shader.");
  ok(fragSource.includes("0.5"),
    "The fragment shader source is correct after changing it.");

  yield removeTab(target.tab);
  finish();
}
