/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor works with bfcache.
 */

function ifWebGLSupported() {
  let [target, debuggee, panel] = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, $, EVENTS, ShadersListView, ShadersEditorsView } = panel.panelWin;

  let reloaded = reload(target);
  yield once(gFront, "program-linked");
  yield reloaded;

  let navigated = navigate(target, MULTIPLE_CONTEXTS_URL);
  yield getPrograms(gFront, 2);
  yield navigated;

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  yield navigateInHistory(target, "back", "will-navigate");
  yield waitForSources();

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
  yield waitForSources();

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

  function waitForSources() {
    let deferred = promise.defer();
    let win = panel.panelWin;
    // These events must fire in this in order and the second event fires
    // synchronously after the first event, so we can't use Promise.all and the
    // `once` helper.
    win.once(win.EVENTS.PROGRAMS_ADDED, () => {
      win.once(win.EVENTS.SOURCES_SHOWN, () => {
        deferred.resolve();
      });
    });
    return deferred.promise;
  }
}
