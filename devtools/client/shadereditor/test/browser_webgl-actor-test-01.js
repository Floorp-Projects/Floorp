/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a WebGL front can be created for a remote tab target.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);

  ok(target, "Should have a target available.");
  ok(front, "Should have a protocol front available.");

  await removeTab(target.tab);
  finish();
}
