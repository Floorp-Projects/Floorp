/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the highlight/unhighlight and blackbox/unblackbox operations on
 * program actors work as expected.
 */

function ifWebGLSupported() {
  let { target, front } = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = yield once(front, "program-linked");
  let vertexShader = yield programActor.getVertexShader();
  let fragmentShader = yield programActor.getFragmentShader();

  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  yield checkShaderSource("The shader sources are correct before highlighting.");
  ok(true, "The corner pixel colors are correct before highlighting.");

  yield programActor.highlight([0, 1, 0, 1]);
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  yield checkShaderSource("The shader sources are preserved after highlighting.");
  ok(true, "The corner pixel colors are correct after highlighting.");

  yield programActor.unhighlight();
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  yield checkShaderSource("The shader sources are correct after unhighlighting.");
  ok(true, "The corner pixel colors are correct after unhighlighting.");

  yield programActor.blackbox();
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield checkShaderSource("The shader sources are preserved after blackboxing.");
  ok(true, "The corner pixel colors are correct after blackboxing.");

  yield programActor.unblackbox();
  yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  yield checkShaderSource("The shader sources are correct after unblackboxing.");
  ok(true, "The corner pixel colors are correct after unblackboxing.");

  function checkShaderSource(aMessage) {
    return Task.spawn(function() {
      let newVertexShader = yield programActor.getVertexShader();
      let newFragmentShader = yield programActor.getFragmentShader();
      is(vertexShader, newVertexShader,
        "The same vertex shader actor was retrieved.");
      is(fragmentShader, newFragmentShader,
        "The same fragment shader actor was retrieved.");

      let vertSource = yield newVertexShader.getText();
      let fragSource = yield newFragmentShader.getText();
      ok(vertSource.contains("I'm special!") &&
         fragSource.contains("I'm also special!"), aMessage);
    });
  }

  yield removeTab(target.tab);
  finish();
}
