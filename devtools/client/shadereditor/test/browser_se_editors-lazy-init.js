/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if source editors are lazily initialized.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(SIMPLE_CANVAS_URL);
  const { front, shadersEditorsView } = panel;

  reload(target);
  await once(front, "program-linked");

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  ok(vsEditor, "A vertex shader editor was initialized.");
  ok(fsEditor, "A fragment shader editor was initialized.");

  isnot(vsEditor, fsEditor,
    "The vertex shader editor is distinct from the fragment shader editor.");

  const vsEditor2 = await shadersEditorsView._getEditor("vs");
  const fsEditor2 = await shadersEditorsView._getEditor("fs");

  is(vsEditor, vsEditor2,
    "The vertex shader editor instances are cached.");
  is(fsEditor, fsEditor2,
    "The fragment shader editor instances are cached.");

  await teardown(panel);
  finish();
}
