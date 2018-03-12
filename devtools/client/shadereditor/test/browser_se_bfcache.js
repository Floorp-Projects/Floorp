/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor works with bfcache.
 */
function* ifWebGLSupported() {
  let { target, panel } = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, $, EVENTS, ShadersListView, ShadersEditorsView } = panel.panelWin;

  // Attach frame scripts if in e10s to perform
  // history navigation via the content
  loadFrameScripts();

  let reloaded = reload(target);
  let firstProgram = yield once(gFront, "program-linked");
  yield reloaded;

  let navigated = navigate(target, MULTIPLE_CONTEXTS_URL);
  let [secondProgram, thirdProgram] = yield getPrograms(gFront, 2);
  yield navigated;

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  yield navigateInHistory(target, "back", "will-navigate");
  yield once(panel.panelWin, EVENTS.PROGRAMS_ADDED);
  yield once(panel.panelWin, EVENTS.SOURCES_SHOWN);

  is($("#content").hidden, false,
    "The tool's content should not be hidden.");
  is(ShadersListView.itemCount, 1,
    "The shaders list contains one entry after navigating back.");
  is(ShadersListView.selectedIndex, 0,
    "The shaders list has a correct selection after navigating back.");

  is(vsEditor.getText().indexOf("gl_Position"), 170,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 97,
    "The fragment shader editor contains the correct text.");

  yield navigateInHistory(target, "forward", "will-navigate");
  yield once(panel.panelWin, EVENTS.PROGRAMS_ADDED);
  yield once(panel.panelWin, EVENTS.SOURCES_SHOWN);

  is($("#content").hidden, false,
    "The tool's content should not be hidden.");
  is(ShadersListView.itemCount, 2,
    "The shaders list contains two entries after navigating forward.");
  is(ShadersListView.selectedIndex, 0,
    "The shaders list has a correct selection after navigating forward.");

  is(vsEditor.getText().indexOf("gl_Position"), 100,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 89,
    "The fragment shader editor contains the correct text.");

  yield teardown(panel);
  finish();
}
