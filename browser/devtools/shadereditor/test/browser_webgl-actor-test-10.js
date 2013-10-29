/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the WebGL context is correctly instrumented every time the
 * target navigates.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);

  let linked = once(front, "program-linked");
  yield front.setup();
  yield linked;
  ok(true, "Canvas was correctly instrumented on the first navigation.");

  let linked = once(front, "program-linked");
  yield reload(target);
  yield linked;
  ok(true, "Canvas was correctly instrumented on the second navigation.");

  let linked = once(front, "program-linked");
  yield reload(target);
  yield linked;
  ok(true, "Canvas was correctly instrumented on the third navigation.");

  let programActor = yield linked;
  let vertexShader = yield programActor.getVertexShader();
  let fragmentShader = yield programActor.getFragmentShader();

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  ok(true, "The top left pixel color was correct before highlighting.");

  yield programActor.highlight([0, 0, 1, 1]);
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  ok(true, "The top left pixel color is correct after highlighting.");

  yield programActor.unhighlight();
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  ok(true, "The top left pixel color is correct after unhighlighting.");

  yield removeTab(target.tab);
  finish();
}
