/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if compile or linkage errors are emitted when a shader source
 * gets malformed after being edited.
 */

function ifWebGLSupported() {
  let [target, debuggee, panel] = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, EVENTS, ShadersEditorsView } = panel.panelWin;

  reload(target);
  yield once(gFront, "program-linked");

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  vsEditor.replaceText("vec3", { line: 7, ch: 22 }, { line: 7, ch: 26 });
  let error = yield once(panel.panelWin, EVENTS.SHADER_COMPILED);

  ok(error,
    "The new vertex shader source was compiled with errors.");
  is(error.compile, "",
    "The compilation status should be empty.");
  isnot(error.link, "",
    "The linkage status should not be empty.");
  is(error.link.split("ERROR").length - 1, 2,
    "The linkage status contains two errors.");
  ok(error.link.contains("ERROR: 0:8: 'constructor'"),
    "A constructor error is contained in the linkage status.");
  ok(error.link.contains("ERROR: 0:8: 'assign'"),
    "An assignment error is contained in the linkage status.");

  fsEditor.replaceText("vec4", { line: 2, ch: 14 }, { line: 2, ch: 18 });
  let error = yield once(panel.panelWin, EVENTS.SHADER_COMPILED);

  ok(error,
    "The new fragment shader source was compiled with errors.");
  is(error.compile, "",
    "The compilation status should be empty.");
  isnot(error.link, "",
    "The linkage status should not be empty.");
  is(error.link.split("ERROR").length - 1, 1,
    "The linkage status contains one error.");
  ok(error.link.contains("ERROR: 0:6: 'constructor'"),
    "A constructor error is contained in the linkage status.");

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);

  vsEditor.replaceText("vec4", { line: 7, ch: 22 }, { line: 7, ch: 26 });
  let error = yield once(panel.panelWin, EVENTS.SHADER_COMPILED);
  ok(!error, "The new vertex shader source was compiled successfully.");

  fsEditor.replaceText("vec3", { line: 2, ch: 14 }, { line: 2, ch: 18 });
  let error = yield once(panel.panelWin, EVENTS.SHADER_COMPILED);
  ok(!error, "The new fragment shader source was compiled successfully.");

  yield ensurePixelIs(debuggee, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  yield ensurePixelIs(debuggee, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);

  yield teardown(panel);
  finish();
}

function once(aTarget, aEvent) {
  let deferred = promise.defer();
  aTarget.once(aEvent, (aName, aData) => deferred.resolve(aData));
  return deferred.promise;
}
