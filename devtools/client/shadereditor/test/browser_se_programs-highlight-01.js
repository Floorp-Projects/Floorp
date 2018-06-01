/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if highlighting a program works properly.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(MULTIPLE_CONTEXTS_URL);
  const { gFront, EVENTS, ShadersListView, ShadersEditorsView } = panel.panelWin;

  once(panel.panelWin, EVENTS.SHADER_COMPILED).then(() => {
    ok(false, "No shaders should be publicly compiled during this test.");
  });

  reload(target);
  const [[firstProgramActor, secondProgramActor]] = await promise.all([
    getPrograms(gFront, 2),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]);

  const vsEditor = await ShadersEditorsView._getEditor("vs");
  const fsEditor = await ShadersEditorsView._getEditor("fs");

  vsEditor.once("change", () => {
    ok(false, "The vertex shader source was unexpectedly changed.");
  });
  fsEditor.once("change", () => {
    ok(false, "The fragment shader source was unexpectedly changed.");
  });
  once(panel.panelWin, EVENTS.SOURCES_SHOWN).then(() => {
    ok(false, "No sources should be changed form this point onward.");
  });

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");

  ShadersListView._onProgramMouseOver({ target: getItemLabel(panel, 0) });

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The first program was correctly highlighted.");

  ShadersListView._onProgramMouseOut({ target: getItemLabel(panel, 0) });
  ShadersListView._onProgramMouseOver({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 0, b: 64, a: 255 }, true, "#canvas2");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 0, g: 0, b: 64, a: 255 }, true, "#canvas2");
  ok(true, "The second program was correctly highlighted.");

  ShadersListView._onProgramMouseOut({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two programs were correctly unhighlighted.");

  ShadersListView._onProgramMouseOver({ target: getBlackBoxCheckbox(panel, 0) });

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two programs were left unchanged after hovering a blackbox checkbox.");

  ShadersListView._onProgramMouseOut({ target: getBlackBoxCheckbox(panel, 0) });

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(gFront, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two programs were left unchanged after unhovering a blackbox checkbox.");

  await teardown(panel);
  finish();
}

function getItemLabel(aPanel, aIndex) {
  return aPanel.panelWin.document.querySelectorAll(
    ".side-menu-widget-item-contents")[aIndex];
}

function getBlackBoxCheckbox(aPanel, aIndex) {
  return aPanel.panelWin.document.querySelectorAll(
    ".side-menu-widget-item-checkbox")[aIndex];
}
