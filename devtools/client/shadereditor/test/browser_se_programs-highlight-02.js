/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if highlighting a program works properly in tandem with blended
 * overlapping geometry.
 */

async function ifWebGLSupported() {
  const { target, debuggee, panel } = await initShaderEditor(BLENDED_GEOMETRY_CANVAS_URL);
  const { front, EVENTS, shadersListView, shadersEditorsView } = panel;

  reload(target);
  const [[firstProgramActor, secondProgramActor]] = await promise.all([
    getPrograms(front, 2),
    once(panel, EVENTS.SOURCES_SHOWN)
  ]);

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The canvas was correctly drawn.");

  shadersListView._onProgramMouseOver({ target: getItemLabel(panel, 0) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 127, g: 0, b: 32, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 0, b: 32, a: 127 }, true);
  ok(true, "The first program was correctly highlighted.");

  shadersListView._onProgramMouseOut({ target: getItemLabel(panel, 0) });
  shadersListView._onProgramMouseOver({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 255, g: 0, b: 64, a: 255 }, true);
  ok(true, "The second program was correctly highlighted.");

  shadersListView._onProgramMouseOut({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 127, g: 127, b: 127, a: 255 }, true);
  await ensurePixelIs(front, { x: 64, y: 64 }, { r: 0, g: 127, b: 127, a: 127 }, true);
  ok(true, "The two programs were correctly unhighlighted.");

  await teardown(panel);
  finish();
}

function getItemLabel(aPanel, aIndex) {
  return aPanel.panelWin.document.querySelectorAll(
    ".side-menu-widget-item-contents")[aIndex];
}
