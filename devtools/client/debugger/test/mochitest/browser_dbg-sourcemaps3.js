/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests loading sourcemapped sources, setting breakpoints, and
// inspecting restored scopes.
requestLongerTimeout(2);

// This source map does not have source contents, so it's fetched separately
add_task(async function () {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger(
    "doc-sourcemaps3.html",
    "bundle.js",
    "sorted.js",
    "test.js"
  );

  ok(true, "Original sources exist");
  const sortedSrc = findSource(dbg, "sorted.js");

  await selectSource(dbg, sortedSrc);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, sortedSrc, 9, 5);
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  ok(
    dbg.selectors.getBreakpoint(
      createLocation({ source: sortedSrc, line: 9, column: 4 })
    ),
    "Breakpoint has correct line"
  );

  invokeInTab("test");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sortedSrc.id, 9, 5);

  is(getScopeNodeLabel(dbg, 1), "Block");
  is(getScopeNodeLabel(dbg, 2), "na");
  is(getScopeNodeLabel(dbg, 3), "nb");

  is(getScopeNodeLabel(dbg, 4), "Function Body");

  await toggleScopeNode(dbg, 4);

  is(getScopeNodeLabel(dbg, 5), "ma");
  is(getScopeNodeLabel(dbg, 6), "mb");

  await toggleScopeNode(dbg, 7);

  is(getScopeNodeLabel(dbg, 8), "a");
  is(getScopeNodeLabel(dbg, 9), "b");

  is(getScopeNodeLabel(dbg, 10), "Module");

  await toggleScopeNode(dbg, 10);

  is(getScopeNodeLabel(dbg, 11), "binaryLookup:o(n, e, r)");
  is(getScopeNodeLabel(dbg, 12), "comparer:t(n, e)");
  is(getScopeNodeLabel(dbg, 13), "fancySort");

  const frameLabels = [
    ...findAllElementsWithSelector(dbg, ".pane.frames .frame .title"),
  ].map(el => el.textContent);
  // The frame display named are mapped to the original source.
  // For example "fancySort" method is named "u" in the generated source.
  Assert.deepEqual(frameLabels, [
    "comparer",
    "binaryLookup",
    "fancySort",
    "fancySort",
    "originalTestName",
  ]);

  info(
    "Verify that original function names are displayed in frames on source selection"
  );
  await selectSource(dbg, "test.js");

  const frameLabelsAfterUpdate = [
    ...findAllElementsWithSelector(dbg, ".pane.frames .frame .title"),
  ].map(el => el.textContent);
  Assert.deepEqual(frameLabelsAfterUpdate, [
    "comparer",
    "binaryLookup",
    "fancySort",
    "fancySort",
    "originalTestName", // <== this frame was updated
  ]);
});
