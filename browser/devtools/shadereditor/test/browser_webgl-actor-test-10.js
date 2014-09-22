/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the WebGL context is correctly instrumented every time the
 * target navigates.
 */

function ifWebGLSupported() {
  let { target, front } = yield initBackend(SIMPLE_CANVAS_URL);

  front.setup({ reload: true });
  yield testHighlighting((yield once(front, "program-linked")));
  ok(true, "Canvas was correctly instrumented on the first navigation.");

  reload(target);
  yield testHighlighting((yield once(front, "program-linked")));
  ok(true, "Canvas was correctly instrumented on the second navigation.");

  reload(target);
  yield testHighlighting((yield once(front, "program-linked")));
  ok(true, "Canvas was correctly instrumented on the third navigation.");

  yield removeTab(target.tab);
  finish();

  function testHighlighting(programActor) {
    return Task.spawn(function() {
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct before highlighting.");

      yield programActor.highlight([0, 1, 0, 1]);
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 0, g: 0, b: 0, a: 255 }, true);
      yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after highlighting.");

      yield programActor.unhighlight();
      yield ensurePixelIs(front, { x: 0, y: 0 }, { r: 255, g: 0, b: 0, a: 255 }, true);
      yield ensurePixelIs(front, { x: 511, y: 511 }, { r: 0, g: 255, b: 0, a: 255 }, true);
      ok(true, "The corner pixel colors are correct after unhighlighting.");
    });
  }
}
