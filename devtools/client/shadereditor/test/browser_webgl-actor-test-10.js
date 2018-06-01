/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the WebGL context is correctly instrumented every time the
 * target navigates.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);

  front.setup({ reload: true });
  await testHighlighting((await once(front, "program-linked")));
  ok(true, "Canvas was correctly instrumented on the first navigation.");

  reload(target);
  await testHighlighting((await once(front, "program-linked")));
  ok(true, "Canvas was correctly instrumented on the second navigation.");

  reload(target);
  await testHighlighting((await once(front, "program-linked")));
  ok(true, "Canvas was correctly instrumented on the third navigation.");

  await removeTab(target.tab);
  finish();

  function testHighlighting(programActor) {
    return (async function() {
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct before highlighting.");

      await programActor.highlight([0, 1, 0, 1]);
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
      await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after highlighting.");

      await programActor.unhighlight();
      await ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      await ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after unhighlighting.");
    })();
  }
}
