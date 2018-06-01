/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if error indicators are shown in the editor's gutter and text area
 * when there's a shader compilation error.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(SIMPLE_CANVAS_URL);
  const { gFront, EVENTS, ShadersEditorsView } = panel.panelWin;

  reload(target);
  await promise.all([
    once(gFront, "program-linked"),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]);

  const vsEditor = await ShadersEditorsView._getEditor("vs");
  const fsEditor = await ShadersEditorsView._getEditor("fs");

  vsEditor.replaceText("vec3", { line: 7, ch: 22 }, { line: 7, ch: 26 });
  let vertError = await panel.panelWin.once(EVENTS.SHADER_COMPILED);
  checkHasVertFirstError(true, vertError);
  checkHasVertSecondError(false, vertError);
  info("Error marks added in the vertex shader editor.");

  vsEditor.insertText(" ", { line: 1, ch: 0 });
  await panel.panelWin.once(EVENTS.EDITOR_ERROR_MARKERS_REMOVED);
  is(vsEditor.getText(1), "       precision lowp float;", "Typed space.");
  checkHasVertFirstError(false, vertError);
  checkHasVertSecondError(false, vertError);
  info("Error marks removed while typing in the vertex shader editor.");

  vertError = await panel.panelWin.once(EVENTS.SHADER_COMPILED);
  checkHasVertFirstError(true, vertError);
  checkHasVertSecondError(false, vertError);
  info("Error marks were re-added after recompiling the vertex shader.");

  fsEditor.replaceText("vec4", { line: 2, ch: 14 }, { line: 2, ch: 18 });
  let fragError = await panel.panelWin.once(EVENTS.SHADER_COMPILED);
  checkHasVertFirstError(true, vertError);
  checkHasVertSecondError(false, vertError);
  checkHasFragError(true, fragError);
  info("Error marks added in the fragment shader editor.");

  fsEditor.insertText(" ", { line: 1, ch: 0 });
  await panel.panelWin.once(EVENTS.EDITOR_ERROR_MARKERS_REMOVED);
  is(fsEditor.getText(1), "       precision lowp float;", "Typed space.");
  checkHasVertFirstError(true, vertError);
  checkHasVertSecondError(false, vertError);
  checkHasFragError(false, fragError);
  info("Error marks removed while typing in the fragment shader editor.");

  fragError = await panel.panelWin.once(EVENTS.SHADER_COMPILED);
  checkHasVertFirstError(true, vertError);
  checkHasVertSecondError(false, vertError);
  checkHasFragError(true, fragError);
  info("Error marks were re-added after recompiling the fragment shader.");

  vsEditor.replaceText("2", { line: 3, ch: 19 }, { line: 3, ch: 20 });
  await panel.panelWin.once(EVENTS.EDITOR_ERROR_MARKERS_REMOVED);
  checkHasVertFirstError(false, vertError);
  checkHasVertSecondError(false, vertError);
  checkHasFragError(true, fragError);
  info("Error marks removed while typing in the vertex shader editor again.");

  vertError = await panel.panelWin.once(EVENTS.SHADER_COMPILED);
  checkHasVertFirstError(true, vertError);
  checkHasVertSecondError(true, vertError);
  checkHasFragError(true, fragError);
  info("Error marks were re-added after recompiling the fragment shader again.");

  await teardown(panel);
  finish();

  function checkHasVertFirstError(bool, error) {
    ok(error, "Vertex shader compiled with errors.");
    isnot(error.link, "", "The linkage status should not be empty.");

    const line = 7;
    info("Checking first vertex shader error on line " + line + "...");

    is(vsEditor.hasMarker(line, "errors", "error"), bool,
      "Error is " + (bool ? "" : "not ") + "shown in the editor's gutter.");
    is(vsEditor.hasLineClass(line, "error-line"), bool,
      "Error style is " + (bool ? "" : "not ") + "applied to the faulty line.");

    const parsed = ShadersEditorsView._errors.vs;
    is(parsed.length >= 1, bool,
      "There's " + (bool ? ">= 1" : "< 1") + " parsed vertex shader error(s).");

    if (bool) {
      is(parsed[0].line, line,
        "The correct line was parsed.");
      is(parsed[0].messages.length, 3,
        "There are 3 parsed messages.");
      ok(parsed[0].messages[0].includes("'constructor' : too many arguments"),
        "The correct first message was parsed.");
      ok(parsed[0].messages[1].includes("'=' : dimension mismatch"),
        "The correct second message was parsed.");
      ok(parsed[0].messages[2].includes("'assign' : cannot convert from"),
        "The correct third message was parsed.");
    }
  }

  function checkHasVertSecondError(bool, error) {
    ok(error, "Vertex shader compiled with errors.");
    isnot(error.link, "", "The linkage status should not be empty.");

    const line = 8;
    info("Checking second vertex shader error on line " + line + "...");

    is(vsEditor.hasMarker(line, "errors", "error"), bool,
      "Error is " + (bool ? "" : "not ") + "shown in the editor's gutter.");
    is(vsEditor.hasLineClass(line, "error-line"), bool,
      "Error style is " + (bool ? "" : "not ") + "applied to the faulty line.");

    const parsed = ShadersEditorsView._errors.vs;
    is(parsed.length >= 2, bool,
      "There's " + (bool ? ">= 2" : "< 2") + " parsed vertex shader error(s).");

    if (bool) {
      is(parsed[1].line, line,
        "The correct line was parsed.");
      is(parsed[1].messages.length, 2,
        "There is 2 parsed messages.");
      ok(parsed[1].messages[0].includes("'=' : dimension mismatch"),
        "The correct first message was parsed.");
      ok(parsed[1].messages[1].includes("'assign' : cannot convert from"),
        "The correct second message was parsed.");
    }
  }

  function checkHasFragError(bool, error) {
    ok(error, "Fragment shader compiled with errors.");
    isnot(error.link, "", "The linkage status should not be empty.");

    const line = 5;
    info("Checking first vertex shader error on line " + line + "...");

    is(fsEditor.hasMarker(line, "errors", "error"), bool,
      "Error is " + (bool ? "" : "not ") + "shown in the editor's gutter.");
    is(fsEditor.hasLineClass(line, "error-line"), bool,
      "Error style is " + (bool ? "" : "not ") + "applied to the faulty line.");

    const parsed = ShadersEditorsView._errors.fs;
    is(parsed.length >= 1, bool,
      "There's " + (bool ? ">= 2" : "< 1") + " parsed fragment shader error(s).");

    if (bool) {
      is(parsed[0].line, line,
        "The correct line was parsed.");
      is(parsed[0].messages.length, 1,
        "There is 1 parsed message.");
      ok(parsed[0].messages[0].includes("'constructor' : too many arguments"),
        "The correct message was parsed.");
    }
  }
}
