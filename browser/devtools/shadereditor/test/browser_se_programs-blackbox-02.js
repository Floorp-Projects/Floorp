/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if blackboxing a program works properly in tandem with blended
 * overlapping geometry.
 */

function ifWebGLSupported() {
  let [target, debuggee, panel] = yield initShaderEditor(BLENDED_GEOMETRY_CANVAS_URL);
  let { gFront, EVENTS, ShadersListView, ShadersEditorsView } = panel.panelWin;

  reload(target);
  let [firstProgramActor, secondProgramActor] = yield promise.all([
    getPrograms(gFront, 2),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]).then(([programs]) => programs);

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The canvas was correctly drawn.");

  getBlackBoxCheckbox(panel, 0).click();

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 0, b: 0, a: 127 }, true);
  ok(true, "The first program was correctly blackboxed.");

  getBlackBoxCheckbox(panel, 0).click();
  getBlackBoxCheckbox(panel, 1).click();

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  ok(true, "The second program was correctly blackboxed.");

  getBlackBoxCheckbox(panel, 1).click();

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The two programs were correctly unblackboxed.");

  getBlackBoxCheckbox(panel, 0).click();
  getBlackBoxCheckbox(panel, 1).click();

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  ok(true, "The two programs were correctly blackboxed again.");

  getBlackBoxCheckbox(panel, 0).click();
  getBlackBoxCheckbox(panel, 1).click();

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The two programs were correctly unblackboxed again.");

  yield teardown(panel);
  finish();
}

function getBlackBoxCheckbox(aPanel, aIndex) {
  return aPanel.panelWin.document.querySelectorAll(
    ".side-menu-widget-item-checkbox")[aIndex];
}
