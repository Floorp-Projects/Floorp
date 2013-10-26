/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the highlight/unhighlight operations on program actors
 * work as expected.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup();

  let programActor = yield once(front, "program-linked");
  let vertexShader = yield programActor.getVertexShader();
  let fragmentShader = yield programActor.getFragmentShader();

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  yield checkShaderSource("The shader sources are correct before highlighting.");
  ok(true, "The top left pixel color was correct before highlighting.");

  yield programActor.highlight([0, 0, 1, 1]);
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 511, y: 511 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  yield checkShaderSource("The shader sources are preserved after highlighting.");
  ok(true, "The top left pixel color is correct after highlighting.");

  yield programActor.unhighlight();
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  yield checkShaderSource("The shader sources are correct after unhighlighting.");
  ok(true, "The top left pixel color is correct after unhighlighting.");

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
