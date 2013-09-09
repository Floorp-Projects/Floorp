/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the WebGL context is never instrumented anymore after the
 * finalize method is called.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);

  let linked = once(front, "program-linked");
  yield front.setup();
  yield linked;
  ok(true, "Canvas was correctly instrumented on the first navigation.");

  once(front, "program-linked").then(() => {
    ok(false, "A 'program-linked' notification shouldn't have been sent!");
  });

  yield front.finalize();
  yield reload(target);
  yield removeTab(target.tab);
  finish();
}
