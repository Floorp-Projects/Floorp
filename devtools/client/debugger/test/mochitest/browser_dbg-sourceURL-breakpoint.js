/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that breakpoints are hit in eval'ed sources with a sourceURL property.
add_task(async function() {
  const dbg = await initDebugger("doc-sourceURL-breakpoint.html", "my-foo.js");
  await selectSource(dbg, "my-foo.js");
  await addBreakpoint(dbg, "my-foo.js", 2);

  invokeInTab("foo");
  await waitForPaused(dbg);

  ok(true, "paused at breakpoint");
});
