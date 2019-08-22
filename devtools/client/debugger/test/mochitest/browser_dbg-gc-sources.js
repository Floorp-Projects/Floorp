/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Test that we can set breakpoints in scripts that have been GCed.
// Check that breakpoints can be set in GC'ed inline scripts, and in
// generated and original sources of scripts with source maps specified either
// inline  or in their HTTP response headers.

add_task(async function() {
  const dbg = await initDebugger("doc-gc-sources.html",
                                 "doc-gc-sources.html",
                                 "collected-bundle.js",
                                 "collected.js",
                                 "collected2.js");
  await selectSource(dbg, "doc-gc-sources.html");
  await addBreakpoint(dbg, "doc-gc-sources.html", 21);
  await selectSource(dbg, "collected-bundle.js");
  await addBreakpoint(dbg, "collected-bundle.js", 31);
  await selectSource(dbg, "collected.js");
  await addBreakpoint(dbg, "collected.js", 2);
  await selectSource(dbg, "collected2.js");
  await addBreakpoint(dbg, "collected2.js", 2);
  ok(true, "Added breakpoint in GC'ed sources");
});
