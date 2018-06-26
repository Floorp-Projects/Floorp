/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if compile or linkage errors are emitted when a shader source
 * gets malformed after being edited.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(SIMPLE_CANVAS_URL);
  const { front, EVENTS, shadersEditorsView } = panel;

  reload(target);
  await promise.all([
    once(front, "program-linked"),
    once(panel, EVENTS.SOURCES_SHOWN)
  ]);

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  vsEditor.replaceText("vec3", { line: 7, ch: 22 }, { line: 7, ch: 26 });
  let error = await panel.once(EVENTS.SHADER_COMPILED);

  ok(error,
    "The new vertex shader source was compiled with errors.");

  // The implementation has the choice to defer all compile-time errors to link time.
  let infoLog = (error.compile != "") ? error.compile : error.link;

  isnot(infoLog, "",
    "The one of the compile or link info logs should not be empty.");
  is(infoLog.split("ERROR").length - 1, 3,
    "The info log status contains three errors.");
  ok(infoLog.includes("ERROR: 0:8: 'constructor'"),
    "A constructor error is contained in the info log.");
  ok(infoLog.includes("ERROR: 0:8: '='"),
    "A dimension error is contained in the info log.");
  ok(infoLog.includes("ERROR: 0:8: 'assign'"),
    "An assignment error is contained in the info log.");

  fsEditor.replaceText("vec4", { line: 2, ch: 14 }, { line: 2, ch: 18 });
  error = await panel.once(EVENTS.SHADER_COMPILED);

  ok(error,
    "The new fragment shader source was compiled with errors.");

  infoLog = (error.compile != "") ? error.compile : error.link;

  isnot(infoLog, "",
    "The one of the compile or link info logs should not be empty.");
  is(infoLog.split("ERROR").length - 1, 1,
    "The info log contains one error.");
  ok(infoLog.includes("ERROR: 0:6: 'constructor'"),
    "A constructor error is contained in the info log.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);

  vsEditor.replaceText("vec4", { line: 7, ch: 22 }, { line: 7, ch: 26 });
  error = await panel.once(EVENTS.SHADER_COMPILED);
  ok(!error, "The new vertex shader source was compiled successfully.");

  fsEditor.replaceText("vec3", { line: 2, ch: 14 }, { line: 2, ch: 18 });
  error = await panel.once(EVENTS.SHADER_COMPILED);
  ok(!error, "The new fragment shader source was compiled successfully.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);

  await teardown(panel);
  finish();
}
