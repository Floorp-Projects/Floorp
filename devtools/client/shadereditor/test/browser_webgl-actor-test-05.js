/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the source contents can be retrieved from the vertex and fragment
 * shader actors.
 */

async function ifWebGLSupported() {
  let { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = await once(front, "program-linked");
  let vertexShader = await programActor.getVertexShader();
  let fragmentShader = await programActor.getFragmentShader();

  let vertSource = await vertexShader.getText();
  ok(vertSource.includes("gl_Position"),
    "The correct vertex shader source was retrieved.");

  let fragSource = await fragmentShader.getText();
  ok(fragSource.includes("gl_FragColor"),
    "The correct fragment shader source was retrieved.");

  await removeTab(target.tab);
  finish();
}
