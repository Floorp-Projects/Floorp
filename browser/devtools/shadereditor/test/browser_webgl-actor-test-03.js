/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if notifications about WebGL programs being linked are sent
 * after a target navigation.
 */

function ifWebGLSupported() {
  let [target, debuggee, front] = yield initBackend(SIMPLE_CANVAS_URL);

  let navigated = once(target, "navigate");
  let linked = once(front, "program-linked");

  yield front.setup();
  ok(true, "The front was setup up successfully.");

  yield navigated;
  ok(true, "Target automatically navigated when the front was set up.");

  yield linked;
  ok(true, "A 'program-linked' notification was sent after reloading.");

  yield removeTab(target.tab);
  finish();
}
