/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if blackboxing a program works properly.
 */

async function ifWebGLSupported() {
  const { target, debuggee, panel } = await initShaderEditor(MULTIPLE_CONTEXTS_URL);
  const { front, EVENTS, shadersListView, shadersEditorsView } = panel;

  once(panel, EVENTS.SHADER_COMPILED).then(() => {
    ok(false, "No shaders should be publicly compiled during this test.");
  });

  reload(target);
  const [[firstProgramActor, secondProgramActor]] = await promise.all([
    getPrograms(front, 2),
    once(panel, EVENTS.SOURCES_SHOWN)
  ]);

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  vsEditor.once("change", () => {
    ok(false, "The vertex shader source was unexpectedly changed.");
  });
  fsEditor.once("change", () => {
    ok(false, "The fragment shader source was unexpectedly changed.");
  });
  once(panel, EVENTS.SOURCES_SHOWN).then(() => {
    ok(false, "No sources should be changed form this point onward.");
  });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");

  ok(!shadersListView.selectedAttachment.isBlackBoxed,
    "The first program should not be blackboxed yet.");
  is(getBlackBoxCheckbox(panel, 0).checked, true,
    "The first blackbox checkbox should be initially checked.");
  ok(!shadersListView.attachments[1].isBlackBoxed,
    "The second program should not be blackboxed yet.");
  is(getBlackBoxCheckbox(panel, 1).checked, true,
    "The second blackbox checkbox should be initially checked.");

  getBlackBoxCheckbox(panel, 0).click();

  ok(shadersListView.selectedAttachment.isBlackBoxed,
    "The first program should now be blackboxed.");
  is(getBlackBoxCheckbox(panel, 0).checked, false,
    "The first blackbox checkbox should now be unchecked.");
  ok(!shadersListView.attachments[1].isBlackBoxed,
    "The second program should still not be blackboxed.");
  is(getBlackBoxCheckbox(panel, 1).checked, true,
    "The second blackbox checkbox should still be checked.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The first program was correctly blackboxed.");

  getBlackBoxCheckbox(panel, 1).click();

  ok(shadersListView.selectedAttachment.isBlackBoxed,
    "The first program should still be blackboxed.");
  is(getBlackBoxCheckbox(panel, 0).checked, false,
    "The first blackbox checkbox should still be unchecked.");
  ok(shadersListView.attachments[1].isBlackBoxed,
    "The second program should now be blackboxed.");
  is(getBlackBoxCheckbox(panel, 1).checked, false,
    "The second blackbox checkbox should now be unchecked.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  ok(true, "The second program was correctly blackboxed.");

  shadersListView._onProgramMouseOver({ target: getItemLabel(panel, 0) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  ok(true, "Highlighting shouldn't work while blackboxed (1).");

  shadersListView._onProgramMouseOut({ target: getItemLabel(panel, 0) });
  shadersListView._onProgramMouseOver({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  ok(true, "Highlighting shouldn't work while blackboxed (2).");

  shadersListView._onProgramMouseOut({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 0, a: 255 }, true, "#canvas2");
  ok(true, "Highlighting shouldn't work while blackboxed (3).");

  getBlackBoxCheckbox(panel, 0).click();
  getBlackBoxCheckbox(panel, 1).click();

  ok(!shadersListView.selectedAttachment.isBlackBoxed,
    "The first program should now be unblackboxed.");
  is(getBlackBoxCheckbox(panel, 0).checked, true,
    "The first blackbox checkbox should now be rechecked.");
  ok(!shadersListView.attachments[1].isBlackBoxed,
    "The second program should now be unblackboxed.");
  is(getBlackBoxCheckbox(panel, 1).checked, true,
    "The second blackbox checkbox should now be rechecked.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two programs were correctly unblackboxed.");

  shadersListView._onProgramMouseOver({ target: getItemLabel(panel, 0) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 0, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The first program was correctly highlighted.");

  shadersListView._onProgramMouseOut({ target: getItemLabel(panel, 0) });
  shadersListView._onProgramMouseOver({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 64, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 0, b: 64, a: 255 }, true, "#canvas2");
  ok(true, "The second program was correctly highlighted.");

  shadersListView._onProgramMouseOut({ target: getItemLabel(panel, 1) });

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 255, g: 255, b: 0, a: 255 }, true, "#canvas1");
  await ensurePixelIs(front, { x: 127, y: 127 }, { r: 0, g: 255, b: 255, a: 255 }, true, "#canvas2");
  ok(true, "The two programs were correctly unhighlighted.");

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
