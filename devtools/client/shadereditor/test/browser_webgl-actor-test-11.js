/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the WebGL context is never instrumented anymore after the
 * finalize method is called.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);

  const linked = once(front, "program-linked");
  front.setup({ reload: true });
  await linked;
  ok(true, "Canvas was correctly instrumented on the first navigation.");

  once(front, "program-linked").then(() => {
    ok(false, "A 'program-linked' notification shouldn't have been sent!");
  });

  await front.finalize();
  await reload(target);
  await removeTab(target.tab);
  finish();
}
