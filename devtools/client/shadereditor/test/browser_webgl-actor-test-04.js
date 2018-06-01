/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a program actor is sent when WebGL programs are linked,
 * and that the corresponding vertex and fragment actors can be retrieved.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  const programActor = await once(front, "program-linked");
  ok(programActor,
    "A program actor was sent along with the 'program-linked' notification.");

  const vertexShader = await programActor.getVertexShader();
  ok(programActor,
    "A vertex shader actor was retrieved from the program actor.");

  const fragmentShader = await programActor.getFragmentShader();
  ok(programActor,
    "A fragment shader actor was retrieved from the program actor.");

  await removeTab(target.tab);
  finish();
}
