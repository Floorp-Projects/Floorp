/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the source contents can be retrieved from the vertex and fragment
 * shader actors.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  const programActor = await once(front, "program-linked");
  const vertexShader = await programActor.getVertexShader();
  const fragmentShader = await programActor.getFragmentShader();

  const vertSource = await vertexShader.getText();
  ok(vertSource.includes("gl_Position"),
    "The correct vertex shader source was retrieved.");

  const fragSource = await fragmentShader.getText();
  ok(fragSource.includes("gl_FragColor"),
    "The correct fragment shader source was retrieved.");

  await removeTab(target.tab);
  finish();
}
