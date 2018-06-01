/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that errors are properly handled when trying to compile a
 * defective shader source.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  const programActor = await once(front, "program-linked");
  const vertexShader = await programActor.getVertexShader();
  const fragmentShader = await programActor.getFragmentShader();

  const oldVertSource = await vertexShader.getText();
  const newVertSource = oldVertSource.replace("vec4", "vec3");

  try {
    await vertexShader.compile(newVertSource);
    ok(false, "Vertex shader was compiled with a defective source!");
  } catch (error) {
    ok(error,
      "The new vertex shader source was compiled with errors.");

    // The implementation has the choice to defer all compile-time errors to link time.
    const infoLog = (error.compile != "") ? error.compile : error.link;

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
  }

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  ok(true, "The shader was reverted to the old source.");

  const vertSource = await vertexShader.getText();
  ok(vertSource.includes("vec4(aVertexPosition, 1.0);"),
    "The previous correct vertex shader source was preserved.");

  const oldFragSource = await fragmentShader.getText();
  const newFragSource = oldFragSource.replace("vec3", "vec4");

  try {
    await fragmentShader.compile(newFragSource);
    ok(false, "Fragment shader was compiled with a defective source!");
  } catch (error) {
    ok(error,
      "The new fragment shader source was compiled with errors.");

    // The implementation has the choice to defer all compile-time errors to link time.
    const infoLog = (error.compile != "") ? error.compile : error.link;

    isnot(infoLog, "",
      "The one of the compile or link info logs should not be empty.");
    is(infoLog.split("ERROR").length - 1, 1,
      "The info log contains one error.");
    ok(infoLog.includes("ERROR: 0:6: 'constructor'"),
      "A constructor error is contained in the info log.");
  }

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  ok(true, "The shader was reverted to the old source.");

  const fragSource = await fragmentShader.getText();
  ok(fragSource.includes("vec3 vFragmentColor;"),
    "The previous correct fragment shader source was preserved.");

  await programActor.highlight([0, 1, 0, 1]);
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  ok(true, "Highlighting worked after setting a defective fragment source.");

  await programActor.unhighlight();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  ok(true, "Unhighlighting worked after setting a defective vertex source.");

  await removeTab(target.tab);
  finish();
}
