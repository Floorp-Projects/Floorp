/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if notifications about WebGL programs being linked are sent
 * after a target navigation.
 */

async function ifWebGLSupported() {
  const { target, front } = await initBackend(SIMPLE_CANVAS_URL);

  const navigated = once(target, "navigate");
  const linked = once(front, "program-linked");

  await front.setup({ reload: true });
  ok(true, "The front was setup up successfully.");

  await navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  await linked;
  ok(true, "A 'program-linked' notification was sent after reloading.");

  await removeTab(target.tab);
  finish();
}
