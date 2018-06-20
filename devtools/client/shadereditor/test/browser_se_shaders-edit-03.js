/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if editing a vertex and a fragment shader would permanently store
 * their new source on the backend and reshow it in the frontend when required.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(MULTIPLE_CONTEXTS_URL);
  const { front, EVENTS, shadersListView, shadersEditorsView } = panel;

  reload(target);

  await promise.all([
    once(front, "program-linked"),
    once(front, "program-linked")
  ]);

  await once(panel, EVENTS.SOURCES_SHOWN);

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  is(shadersListView.selectedIndex, 0,
    "The first program is currently selected.");
  is(vsEditor.getText().indexOf("1);"), 136,
    "The vertex shader editor contains the correct initial text (1).");
  is(fsEditor.getText().indexOf("1);"), 117,
    "The fragment shader editor contains the correct initial text (1).");
  is(vsEditor.getText().indexOf("2.);"), -1,
    "The vertex shader editor contains the correct initial text (2).");
  is(fsEditor.getText().indexOf(".0);"), -1,
    "The fragment shader editor contains the correct initial text (2).");

  vsEditor.replaceText("2.", { line: 5, ch: 44 }, { line: 5, ch: 45 });
  await once(panel, EVENTS.SHADER_COMPILED);

  fsEditor.replaceText(".0", { line: 5, ch: 35 }, { line: 5, ch: 37 });
  await once(panel, EVENTS.SHADER_COMPILED);

  ok(true, "Vertex and fragment shaders were changed.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 32, y: 32 }, { r: 255, g: 255, b: 0, a: 0 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 255, g: 255, b: 0, a: 0 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 32, y: 32 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");

  ok(true, "The vertex and fragment shaders were recompiled successfully.");

  EventUtils.sendMouseEvent({ type: "mousedown" }, shadersListView.items[1].target);
  await once(panel, EVENTS.SOURCES_SHOWN);

  is(shadersListView.selectedIndex, 1,
    "The second program is currently selected.");
  is(vsEditor.getText().indexOf("1);"), 136,
    "The vertex shader editor contains the correct text (1).");
  is(fsEditor.getText().indexOf("1);"), 117,
    "The fragment shader editor contains the correct text (1).");
  is(vsEditor.getText().indexOf("2.);"), -1,
    "The vertex shader editor contains the correct text (2).");
  is(fsEditor.getText().indexOf(".0);"), -1,
    "The fragment shader editor contains the correct text (2).");

  EventUtils.sendMouseEvent({ type: "mousedown" }, shadersListView.items[0].target);
  await once(panel, EVENTS.SOURCES_SHOWN);

  is(shadersListView.selectedIndex, 0,
    "The first program is currently selected again.");
  is(vsEditor.getText().indexOf("1);"), -1,
    "The vertex shader editor contains the correct text (3).");
  is(fsEditor.getText().indexOf("1);"), -1,
    "The fragment shader editor contains the correct text (3).");
  is(vsEditor.getText().indexOf("2.);"), 136,
    "The vertex shader editor contains the correct text (4).");
  is(fsEditor.getText().indexOf(".0);"), 116,
    "The fragment shader editor contains the correct text (4).");

  await teardown(panel);
  finish();
}
