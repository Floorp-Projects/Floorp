/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that vertex and fragment shader sources can be changed.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  const programActor = await once(front, "program-linked");
  const vertexShader = await programActor.getVertexShader();
  const fragmentShader = await programActor.getFragmentShader();

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 128, y: 128 }, { r: 191, g: 64, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);

  let vertSource = await vertexShader.getText();
  let fragSource = await fragmentShader.getText();
  ok(!vertSource.includes("2.0"),
    "The vertex shader source is correct before changing it.");
  ok(!fragSource.includes("0.5"),
    "The fragment shader source is correct before changing it.");

  const newVertSource = vertSource.replace("1.0", "2.0");
  let status = await vertexShader.compile(newVertSource);
  ok(!status,
    "The new vertex shader source was compiled without errors.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 128, y: 128 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);

  vertSource = await vertexShader.getText();
  fragSource = await fragmentShader.getText();
  ok(vertSource.includes("2.0"),
    "The vertex shader source is correct after changing it.");
  ok(!fragSource.includes("0.5"),
    "The fragment shader source is correct after changing the vertex shader.");

  const newFragSource = fragSource.replace("1.0", "0.5");
  status = await fragmentShader.compile(newFragSource);
  ok(!status,
    "The new fragment shader source was compiled without errors.");

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 128, y: 128 }, { r: 255, g: 0, b: 0, a: 127 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);

  vertSource = await vertexShader.getText();
  fragSource = await fragmentShader.getText();
  ok(vertSource.includes("2.0"),
    "The vertex shader source is correct after changing the fragment shader.");
  ok(fragSource.includes("0.5"),
    "The fragment shader source is correct after changing it.");

  await removeTab(target.tab);
  finish();
}
