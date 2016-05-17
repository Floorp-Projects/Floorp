/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a program actor is sent when WebGL programs are linked,
 * and that the corresponding vertex and fragment actors can be retrieved.
 */

function* ifWebGLSupported() {
  let { target, front } = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = yield once(front, "program-linked");
  ok(programActor,
    "A program actor was sent along with the 'program-linked' notification.");

  let vertexShader = yield programActor.getVertexShader();
  ok(programActor,
    "A vertex shader actor was retrieved from the program actor.");

  let fragmentShader = yield programActor.getFragmentShader();
  ok(programActor,
    "A fragment shader actor was retrieved from the program actor.");

  yield removeTab(target.tab);
  finish();
}
