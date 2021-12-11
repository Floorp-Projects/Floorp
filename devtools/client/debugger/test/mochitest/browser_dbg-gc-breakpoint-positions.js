/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Test that we can set breakpoints in scripts that have been GCed.

add_task(async function() {
  const dbg = await initDebugger("doc-gc-breakpoint-positions.html",
                                 "doc-gc-breakpoint-positions.html");
  await selectSource(dbg, "doc-gc-breakpoint-positions.html");
  await addBreakpoint(dbg, "doc-gc-breakpoint-positions.html", 21);
  ok(true, "Added breakpoint at GC'ed script location");
});
