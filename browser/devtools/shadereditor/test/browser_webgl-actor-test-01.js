/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a WebGL front can be created for a remote tab target.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);

  ok(target, "Should have a target available.");
  ok(debuggee, "Should have a debuggee available.");
  ok(front, "Should have a protocol front available.");

  yield removeTab(target.tab);
  finish();
}
