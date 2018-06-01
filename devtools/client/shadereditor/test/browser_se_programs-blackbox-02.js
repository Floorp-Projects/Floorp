/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if blackboxing a program works properly in tandem with blended
 * overlapping geometry.
 */

async function ifWebGLSupported() {
  const { target, debuggee, panel } = await initShaderEditor(BLENDED_GEOMETRY_CANVAS_URL);
  const { gFront, EVENTS, ShadersListView, ShadersEditorsView } = panel.panelWin;

  reload(target);
  const [[firstProgramActor, secondProgramActor]] = await promise.all([
    getPrograms(gFront, 2),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]);

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  await ensurePixelIs(gFront, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The canvas was correctly drawn.");

  getBlackBoxCheckbox(panel, 0).click();

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(gFront, { x: 64, y: 64 }, { r: 0, g: 0, b: 0, a: 127 }, true);
  ok(true, "The first program was correctly blackboxed.");

  getBlackBoxCheckbox(panel, 0).click();
  getBlackBoxCheckbox(panel, 1).click();

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  await ensurePixelIs(gFront, { x: 64, y: 64 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  ok(true, "The second program was correctly blackboxed.");

  getBlackBoxCheckbox(panel, 1).click();

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  await ensurePixelIs(gFront, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The two programs were correctly unblackboxed.");

  getBlackBoxCheckbox(panel, 0).click();
  getBlackBoxCheckbox(panel, 1).click();

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(gFront, { x: 64, y: 64 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  ok(true, "The two programs were correctly blackboxed again.");

  getBlackBoxCheckbox(panel, 0).click();
  getBlackBoxCheckbox(panel, 1).click();

  await ensurePixelIs(gFront, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  await ensurePixelIs(gFront, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The two programs were correctly unblackboxed again.");

  await teardown(panel);
  finish();
}

function getBlackBoxCheckbox(aPanel, aIndex) {
  return aPanel.panelWin.document.querySelectorAll(
    ".side-menu-widget-item-checkbox")[aIndex];
}
