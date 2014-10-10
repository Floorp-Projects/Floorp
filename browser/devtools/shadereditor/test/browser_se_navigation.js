/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests target navigations are handled correctly in the UI.
 */

function ifWebGLSupported() {
  let { target, panel } = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, $, EVENTS, ShadersListView, ShadersEditorsView } = panel.panelWin;

  reload(target);
  yield promise.all([
    once(gFront, "program-linked"),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]);

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should be hidden after linking.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for a WebGL context' notice should be visible after linking.");
  is($("#content").hidden, false,
    "The tool's content should not be hidden anymore.");

  is(ShadersListView.itemCount, 1,
    "The shaders list contains one entry.");
  is(ShadersListView.selectedItem, ShadersListView.items[0],
    "The shaders list has a correct item selected.");
  is(ShadersListView.selectedIndex, 0,
    "The shaders list has a correct index selected.");

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  is(vsEditor.getText().indexOf("gl_Position"), 170,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 97,
    "The fragment shader editor contains the correct text.");

  let navigating = once(target, "will-navigate");
  let navigated = once(target, "will-navigate");
  navigate(target, "about:blank");

  yield navigating;
  yield once(panel.panelWin, EVENTS.UI_RESET);

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should be hidden while navigating.");
  is($("#waiting-notice").hidden, false,
    "The 'waiting for a WebGL context' notice should be visible while navigating.");
  is($("#content").hidden, true,
    "The tool's content should be hidden now that there's no WebGL content.");

  is(ShadersListView.itemCount, 0,
    "The shaders list should be empty.");
  is(ShadersListView.selectedItem, null,
    "The shaders list has no correct item.");
  is(ShadersListView.selectedIndex, -1,
    "The shaders list has a negative index.");

  yield ShadersEditorsView._getEditor("vs").then(() => {
    ok(false, "The promise for a vertex shader editor should be rejected.");
  }, () => {
    ok(true, "The vertex shader editors wasn't initialized.");
  });

  yield ShadersEditorsView._getEditor("fs").then(() => {
    ok(false, "The promise for a fragment shader editor should be rejected.");
  }, () => {
    ok(true, "The fragment shader editors wasn't initialized.");
  });

  yield navigated;

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should still be hidden after navigating.");
  is($("#waiting-notice").hidden, false,
    "The 'waiting for a WebGL context' notice should still be visible after navigating.");
  is($("#content").hidden, true,
    "The tool's content should be still hidden since there's no WebGL content.");

  yield teardown(panel);
  finish();
}
