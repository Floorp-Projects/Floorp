/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests loading sourcemapped sources, setting breakpoints, and
// stepping in them.

"use strict";

requestLongerTimeout(2);

// This source map does not have source contents, so it's fetched separately
add_task(async function () {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger(
    "doc-sourcemaps-indexed.html",
    "main.js",
    "main.min.js"
  );
  const {
    selectors: { getBreakpoint, getBreakpointCount },
  } = dbg;

  ok(true, "Original sources exist");
  const mainSrc = findSource(dbg, "main.js");

  await selectSource(dbg, mainSrc);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, mainSrc, 4, 3);
  is(getBreakpointCount(), 1, "One breakpoint exists");
  ok(
    getBreakpoint(createLocation({ source: mainSrc, line: 4, column: 2 })),
    "Breakpoint has correct line"
  );

  await assertBreakpoint(dbg, 4);
  invokeInTab("logMessage");

  await waitForPausedInOriginalFileAndToggleMapScopes(dbg);
  assertPausedAtSourceAndLine(dbg, mainSrc.id, 4, 3);

  // Tests the existence of the sourcemap link in the original source.
  ok(findElement(dbg, "mappedSourceLink"), "Sourcemap link in original source");
  await selectSource(dbg, "main.min.js");

  ok(
    !findElement(dbg, "mappedSourceLink"),
    "No Sourcemap link exists in generated source"
  );
});
