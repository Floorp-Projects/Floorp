/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the canvas debugger leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

async function ifTestingSupported() {
  const { target, front } = await initCallWatcherBackend(SIMPLE_CANVAS_URL);

  ok(target, "Should have a target available.");
  ok(front, "Should have a protocol front available.");

  await removeTab(target.tab);
  finish();
}
