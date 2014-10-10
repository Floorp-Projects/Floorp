/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if editing a vertex and a fragment shader works properly.
 */

function ifWebGLSupported() {
  let { target, panel } = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, $, EVENTS, ShadersEditorsView } = panel.panelWin;

  reload(target);
  yield promise.all([
    once(gFront, "program-linked"),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]);

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  is(vsEditor.getText().indexOf("gl_Position"), 170,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 97,
    "The fragment shader editor contains the correct text.");

  is($("#vs-editor-label").hasAttribute("selected"), false,
    "The vertex shader editor shouldn't be initially selected.");
  is($("#fs-editor-label").hasAttribute("selected"), false,
    "The vertex shader editor shouldn't be initially selected.");

  yield ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(gFront, { x: 128, y: 128 }, { r: 191, g: 64, b: 0, a: 255 }, true);
  yield ensurePixelIs(gFront, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);

  vsEditor.focus();

  is($("#vs-editor-label").hasAttribute("selected"), true,
    "The vertex shader editor should now be selected.");
  is($("#fs-editor-label").hasAttribute("selected"), false,
    "The vertex shader editor shouldn't still not be selected.");

  vsEditor.replaceText("2.0", { line: 7, ch: 44 }, { line: 7, ch: 47 });
  yield once(panel.panelWin, EVENTS.SHADER_COMPILED);

  ok(true, "Vertex shader was changed.");

  yield ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(gFront, { x: 128, y: 128 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(gFront, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);

  ok(true, "The vertex shader was recompiled successfully.");

  fsEditor.focus();

  is($("#vs-editor-label").hasAttribute("selected"), false,
    "The vertex shader editor should now be deselected.");
  is($("#fs-editor-label").hasAttribute("selected"), true,
    "The vertex shader editor should now be selected.");

  fsEditor.replaceText("0.5", { line: 5, ch: 44 }, { line: 5, ch: 47 });
  yield once(panel.panelWin, EVENTS.SHADER_COMPILED);

  ok(true, "Fragment shader was changed.");

  yield ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(gFront, { x: 128, y: 128 }, { r: 255, g: 0, b: 0, a: 127 }, true);
  yield ensurePixelIs(gFront, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);

  ok(true, "The fragment shader was recompiled successfully.");

  yield teardown(panel);
  finish();
}
