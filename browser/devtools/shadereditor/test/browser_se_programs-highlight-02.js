/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if highlighting a program works properly in tandem with blended
 * overlapping geometry.
 */

function ifWebGLSupported() {
  let [target, debuggee, panel] = yield initShaderEditor(BLENDED_GEOMETRY_CANVAS_URL);
  let { gFront, EVENTS, ShadersListView, ShadersEditorsView } = panel.panelWin;

  reload(target);
  let [[firstProgramActor, secondProgramActor]] = yield promise.all([
    getPrograms(gFront, 2),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]);

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The canvas was correctly drawn.");

  ShadersListView._onProgramMouseEnter({ target: getItemLabel(panel, 0) });

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 0, b: 32, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 0, b: 32, a: 127 }, true);
  ok(true, "The first program was correctly highlighted.");

  ShadersListView._onProgramMouseLeave({ target: getItemLabel(panel, 0) });
  ShadersListView._onProgramMouseEnter({ target: getItemLabel(panel, 1) });

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 255, g: 0, b: 64, a: 255 }, true);
  ok(true, "The second program was correctly highlighted.");

  ShadersListView._onProgramMouseLeave({ target: getItemLabel(panel, 1) });

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The two programs were correctly unhighlighted.");

  yield teardown(panel);
  finish();
}

function getItemLabel(aPanel, aIndex) {
  return aPanel.panelWin.document.querySelectorAll(
    ".side-menu-widget-item-contents")[aIndex];
}
