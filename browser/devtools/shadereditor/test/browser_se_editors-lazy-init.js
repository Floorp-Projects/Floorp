/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed. 
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Shader Editor is still waiting for a WebGL context to be created.");

/**
 * Tests if source editors are lazily initialized.
 */

function ifWebGLSupported() {
  let { target, panel } = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, ShadersEditorsView } = panel.panelWin;

  try {
    yield ShadersEditorsView._getEditor("vs");
    ok(false, "The promise for a vertex shader editor should be rejected.");
  } catch (e) {
    ok(true, "The vertex shader editors wasn't initialized.");
  }

  try {
    yield ShadersEditorsView._getEditor("fs");
    ok(false, "The promise for a fragment shader editor should be rejected.");
  } catch (e) {
    ok(true, "The fragment shader editors wasn't initialized.");
  }

  reload(target);
  yield once(gFront, "program-linked");

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  ok(vsEditor, "A vertex shader editor was initialized.");
  ok(fsEditor, "A fragment shader editor was initialized.");

  isnot(vsEditor, fsEditor,
    "The vertex shader editor is distinct from the fragment shader editor.");

  let vsEditor2 = yield ShadersEditorsView._getEditor("vs");
  let fsEditor2 = yield ShadersEditorsView._getEditor("fs");

  is(vsEditor, vsEditor2,
    "The vertex shader editor instances are cached.");
  is(fsEditor, fsEditor2,
    "The fragment shader editor instances are cached.");

  yield teardown(panel);
  finish();
}
