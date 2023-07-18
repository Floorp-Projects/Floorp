/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests loading sourcemapped sources, setting breakpoints, and
// inspecting restored scopes.
requestLongerTimeout(2);

// This source map does not have source contents, so it's fetched separately
add_task(async function () {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger(
    "doc-sourcemaps3.html",
    "bundle.js",
    "sorted.js",
    "test.js"
  );
  dbg.actions.toggleMapScopes();

  ok(true, "Original sources exist");
  const sortedSrc = findSource(dbg, "sorted.js");

  await selectSource(dbg, sortedSrc);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, sortedSrc, 9, 4);
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");
  ok(
    dbg.selectors.getBreakpoint(
      createLocation({ source: sortedSrc, line: 9, column: 4 })
    ),
    "Breakpoint has correct line"
  );

  invokeInTab("test");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sortedSrc.id, 9, 4);

  is(getScopeLabel(dbg, 1), "Block");
  is(getScopeLabel(dbg, 2), "na");
  is(getScopeLabel(dbg, 3), "nb");

  is(getScopeLabel(dbg, 4), "Function Body");

  await toggleScopeNode(dbg, 4);

  is(getScopeLabel(dbg, 5), "ma");
  is(getScopeLabel(dbg, 6), "mb");

  await toggleScopeNode(dbg, 7);

  is(getScopeLabel(dbg, 8), "a");
  is(getScopeLabel(dbg, 9), "b");

  is(getScopeLabel(dbg, 10), "Module");

  await toggleScopeNode(dbg, 10);

  is(getScopeLabel(dbg, 11), "binaryLookup:o(n, e, r)");
  is(getScopeLabel(dbg, 12), "comparer:t(n, e)");
  is(getScopeLabel(dbg, 13), "fancySort");

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
    "test",
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
