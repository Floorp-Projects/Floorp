/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the rendering is updated when a varying variable is
 * changed in one shader.
 */

async function ifWebGLSupported() {
  let { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = await once(front, "program-linked");
  let vertexShader = await programActor.getVertexShader();
  let fragmentShader = await programActor.getFragmentShader();

  let oldVertSource = await vertexShader.getText();
  let newVertSource = oldVertSource.replace("= aVertexColor", "= vec3(0, 0, 1)");
  let status = await vertexShader.compile(newVertSource);
  ok(!status,
    "The new vertex shader source was compiled without errors.");

  await front.waitForFrame();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 128, y: 128 }, { r: 0, g: 0, b: 255, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 255, a: 255 }, true);

  let vertSource = await vertexShader.getText();
  let fragSource = await fragmentShader.getText();
  ok(vertSource.includes("vFragmentColor = vec3(0, 0, 1);"),
    "The vertex shader source is correct after changing it.");
  ok(fragSource.includes("gl_FragColor = vec4(vFragmentColor, 1.0);"),
    "The fragment shader source is correct after changing the vertex shader.");

  await removeTab(target.tab);
  finish();
}
