/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the rendering is updated when a varying variable is
 * changed in one shader.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  const programActor = await once(front, "program-linked");
  const vertexShader = await programActor.getVertexShader();
  const fragmentShader = await programActor.getFragmentShader();

  const oldVertSource = await vertexShader.getText();
  const newVertSource = oldVertSource.replace("= aVertexColor", "= vec3(0, 0, 1)");
  const status = await vertexShader.compile(newVertSource);
  ok(!status,
    "The new vertex shader source was compiled without errors.");

  await front.waitForFrame();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 128, y: 128 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 255, a: 255 }, true);

  const vertSource = await vertexShader.getText();
  const fragSource = await fragmentShader.getText();
  ok(vertSource.includes("vFragmentColor = vec3(0, 0, 1);"),
    "The vertex shader source is correct after changing it.");
  ok(fragSource.includes("gl_FragColor = vec4(vFragmentColor, 1.0);"),
    "The fragment shader source is correct after changing the vertex shader.");

  await removeTab(target.tab);
  finish();
}
