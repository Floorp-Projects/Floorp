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
    "doc-sourcemaps2.html",
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
  assertPausedAtSourceAndLine(dbg, mainSrc.id, 4);

  // Tests the existence of the sourcemap link in the original source.
  let sourceMapLink = findElement(dbg, "mappedSourceLink");
  is(
    sourceMapLink.textContent,
    "To main.min.js",
    "Sourcemap link in original source refers to the bundle"
  );

  await selectSource(dbg, "main.min.js");

  // The mapped source link is computed asynchronously when we are on the bundle
  sourceMapLink = await waitFor(() => findElement(dbg, "mappedSourceLink"));
  is(
    sourceMapLink.textContent,
    "From main.js",
    "Sourcemap link in bundle refers to the original source"
  );
});
