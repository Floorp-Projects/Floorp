/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the correct vertex and fragment shader sources are retrieved
 * regardless of the order in which they were compiled and attached.
 */

function ifWebGLSupported() {
  let { target, front } = yield initBackend(SHADER_ORDER_URL);
  front.setup({ reload: true });

  let programActor = yield once(front, "program-linked");
  let vertexShader = yield programActor.getVertexShader();
  let fragmentShader = yield programActor.getFragmentShader();

  let vertSource = yield vertexShader.getText();
  let fragSource = yield fragmentShader.getText();

  ok(vertSource.includes("I'm a vertex shader!"),
    "The correct vertex shader text was retrieved.");
  ok(fragSource.includes("I'm a fragment shader!"),
    "The correct fragment shader text was retrieved.");

  yield removeTab(target.tab);
  finish();
}
