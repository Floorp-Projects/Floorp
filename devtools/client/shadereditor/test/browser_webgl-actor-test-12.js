/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the correct vertex and fragment shader sources are retrieved
 * regardless of the order in which they were compiled and attached.
 */

async function ifWebGLSupported() {
  let { target, front } = await initBackend(SHADER_ORDER_URL);
  front.setup({ reload: true });

  let programActor = await once(front, "program-linked");
  let vertexShader = await programActor.getVertexShader();
  let fragmentShader = await programActor.getFragmentShader();

  let vertSource = await vertexShader.getText();
  let fragSource = await fragmentShader.getText();

  ok(vertSource.includes("I'm a vertex shader!"),
    "The correct vertex shader text was retrieved.");
  ok(fragSource.includes("I'm a fragment shader!"),
    "The correct fragment shader text was retrieved.");

  await removeTab(target.tab);
  finish();
}
