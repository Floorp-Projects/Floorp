/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed. 
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Shader Editor is still waiting for a WebGL context to be created.");

/**
 * Tests if the shader editor leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

function ifWebGLSupported() {
  let { target, panel } = yield initShaderEditor(SIMPLE_CANVAS_URL);

  ok(target, "Should have a target available.");
  ok(panel, "Should have a panel available.");

  yield teardown(panel);
  finish();
}
