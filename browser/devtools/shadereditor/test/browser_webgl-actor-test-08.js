/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the rendering is updated when a varying variable is
 * changed in one shader.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = yield once(front, "program-linked");
  let vertexShader = yield programActor.getVertexShader();
  let fragmentShader = yield programActor.getFragmentShader();

  let oldVertSource = yield vertexShader.getText();
  let newVertSource = oldVertSource.replace("= aVertexColor", "= vec3(0, 0, 1)");
  let status = yield vertexShader.compile(newVertSource);
  ok(!status,
    "The new vertex shader source was compiled without errors.");

  yield waitForFrame(debuggee);
  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 128, y: 128 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 511, y: 511 }, { r: 0, g: 0, b: 255, a: 255 }, true);

  let vertSource = yield vertexShader.getText();
  let fragSource = yield fragmentShader.getText();
  ok(vertSource.contains("vFragmentColor = vec3(0, 0, 1);"),
    "The vertex shader source is correct after changing it.");
  ok(fragSource.contains("gl_FragColor = vec4(vFragmentColor, 1.0);"),
    "The fragment shader source is correct after changing the vertex shader.");

  yield removeTab(target.tab);
  finish();
}
