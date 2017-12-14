/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test is taking too much time to complete on some hardware since
// release at https://bugzilla.mozilla.org/show_bug.cgi?id=1423158
requestLongerTimeout(3);

/**
 * Test debugging a page with iframes
 *  1. pause in the main thread
 *  2. pause in the iframe
 */
add_task(async function() {
  const dbg = await initDebugger("doc-iframes.html");

  // test pausing in the main thread
  await reload(dbg);
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "doc-iframes.html");
  assertPausedLocation(dbg);

  // test pausing in the iframe
  await resume(dbg);
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "doc-debugger-statements.html");
  assertPausedLocation(dbg);

  // test pausing in the iframe
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedLocation(dbg);
});
