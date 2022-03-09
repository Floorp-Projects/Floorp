/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests setting breakpoints in an original file and
// removing it in the generated file.

"use strict";

requestLongerTimeout(2);

add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger("doc-sourcemaps.html", "entry.js");

  ok(true, "Original sources exist");

  await selectSource(dbg, "entry.js");

  await clickGutter(dbg, 9);
  await waitForBreakpointCount(dbg, 1);
  await assertBreakpoint(dbg, 9);
  assertBreakpointSnippet(dbg, 3, "output(times2(3));");

  await selectSource(dbg, "bundle.js");
  await assertBreakpoint(dbg, 55);
  assertBreakpointSnippet(dbg, 3, "output(times2(3));");

  await clickGutter(dbg, 55);
  await waitForBreakpointCount(dbg, 0);
  await assertNoBreakpoint(dbg, 55);

  await selectSource(dbg, "entry.js");
  await assertNoBreakpoint(dbg, 9);
});
