/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the highlight/unhighlight and blackbox/unblackbox operations on
 * program actors work as expected.
 */

async function ifWebGLSupported() {
  let { target, front } = await initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = await once(front, "program-linked");
  let vertexShader = await programActor.getVertexShader();
  let fragmentShader = await programActor.getFragmentShader();

  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  await checkShaderSource("The shader sources are correct before highlighting.");
  ok(true, "The corner pixel colors are correct before highlighting.");

  await programActor.highlight([0, 1, 0, 1]);
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  await checkShaderSource("The shader sources are preserved after highlighting.");
  ok(true, "The corner pixel colors are correct after highlighting.");

  await programActor.unhighlight();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  await checkShaderSource("The shader sources are correct after unhighlighting.");
  ok(true, "The corner pixel colors are correct after unhighlighting.");

  await programActor.blackbox();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 0, b: 0, a: 255 }, true);
  await checkShaderSource("The shader sources are preserved after blackboxing.");
  ok(true, "The corner pixel colors are correct after blackboxing.");

  await programActor.unblackbox();
  await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
  await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
  await checkShaderSource("The shader sources are correct after unblackboxing.");
  ok(true, "The corner pixel colors are correct after unblackboxing.");

  function checkShaderSource(aMessage) {
    return (async function() {
      let newVertexShader = await programActor.getVertexShader();
      let newFragmentShader = await programActor.getFragmentShader();
      is(vertexShader, newVertexShader,
        "The same vertex shader actor was retrieved.");
      is(fragmentShader, newFragmentShader,
        "The same fragment shader actor was retrieved.");

      let vertSource = await newVertexShader.getText();
      let fragSource = await newFragmentShader.getText();
      ok(vertSource.includes("I'm special!") &&
         fragSource.includes("I'm also special!"), aMessage);
    })();
  }

  await removeTab(target.tab);
  finish();
}
