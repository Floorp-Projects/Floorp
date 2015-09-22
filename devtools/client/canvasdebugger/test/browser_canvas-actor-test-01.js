/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the canvas debugger leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

function* ifTestingSupported() {
  let { target, front } = yield initCallWatcherBackend(SIMPLE_CANVAS_URL);

  ok(target, "Should have a target available.");
  ok(front, "Should have a protocol front available.");

  yield removeTab(target.tab);
  finish();
}
