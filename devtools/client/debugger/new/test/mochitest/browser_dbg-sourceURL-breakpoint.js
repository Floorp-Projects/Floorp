/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that breakpoints are hit in eval'ed sources with a sourceURL property.
add_task(async function() {
  const dbg = await initDebugger("doc-sourceURL-breakpoint.html", "my-foo.js");
  await selectSource(dbg, "my-foo.js");
  await addBreakpoint(dbg, "my-foo.js", 2);

  invokeInTab("foo");
  await waitForPaused(dbg);

  ok(true, "paused at breakpoint");
});
