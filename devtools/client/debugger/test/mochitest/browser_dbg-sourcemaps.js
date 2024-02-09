/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests loading sourcemapped sources, setting breakpoints, and
// stepping in them.

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger(
    "doc-sourcemaps.html",
    "entry.js",
    "output.js",
    "times2.js",
    "opts.js"
  );
  const {
    selectors: { getBreakpointCount },
  } = dbg;

  // Check that the original sources appear in the source tree
  info("Before opening the page directory, no source are displayed");
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });
  await clickElement(dbg, "sourceDirectoryLabel", 4);
  info(
    "After opening the page directory, only all original sources (entry, output, time2, opts). (bundle is still hidden)"
  );
  await waitForSourcesInSourceTree(
    dbg,
    ["entry.js", "output.js", "times2.js", "opts.js"],
    { noExpand: true }
  );
  info("Expand the page folder and assert that the bundle appears");
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  await clickElement(dbg, "sourceDirectoryLabel", 4);
  await waitForSourcesInSourceTree(
    dbg,
    ["entry.js", "output.js", "times2.js", "opts.js", "bundle.js"],
    { noExpand: true }
  );

  const bundleSrc = findSource(dbg, "bundle.js");
  await selectSource(dbg, bundleSrc);

  await clickGutter(dbg, 70);
  await waitForBreakpointCount(dbg, 1);
  await assertBreakpoint(dbg, 70);

  await clickGutter(dbg, 70);
  await waitForBreakpointCount(dbg, 0);

  const entrySrc = findSource(dbg, "entry.js");

  await selectSource(dbg, entrySrc);
  ok(
    getCM(dbg).getValue().includes("window.keepMeAlive"),
    "Original source text loaded correctly"
  );

  // Bug 1824375 - pending location shouldn't be location and include only url, line and column attributes
  let pendingSelectedLocation = Services.prefs.getStringPref(
    "devtools.debugger.pending-selected-location"
  );
  is(
    pendingSelectedLocation,
    JSON.stringify({ url: entrySrc.url, line: 0, column: undefined }),
    "Pending selected location is the expected one"
  );

  // Test breaking on a breakpoint
  await addBreakpoint(dbg, "entry.js", 15);
  is(getBreakpointCount(), 1, "One breakpoint exists");
  assertBreakpointExists(dbg, entrySrc, 15);

  invokeInTab("keepMeAlive");
  await waitForPausedInOriginalFileAndToggleMapScopes(dbg);
  assertPausedAtSourceAndLine(dbg, entrySrc.id, 15);

  await stepIn(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "times2.js").id, 2);

  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "times2.js").id, 3);

  await stepOut(dbg);
  assertPausedAtSourceAndLine(dbg, entrySrc.id, 16);

  pendingSelectedLocation = Services.prefs.getStringPref(
    "devtools.debugger.pending-selected-location"
  );
  is(
    pendingSelectedLocation,
    JSON.stringify({ url: entrySrc.url, line: 16, column: 0 }),
    "Pending selected location is the expected one"
  );

  info("Click on jump to generated source link from editor's footer");
  let mappedSourceLink = findElement(dbg, "mappedSourceLink");
  is(
    mappedSourceLink.textContent,
    "To bundle.js",
    "The link to mapped source mentions the bundle"
  );
  mappedSourceLink.click();

  await waitForSelectedSource(dbg, bundleSrc);
  assertPausedAtSourceAndLine(dbg, bundleSrc.id, 62);
  // The mapped source link is computed asynchronously when we are on the bundle
  mappedSourceLink = await waitFor(() => findElement(dbg, "mappedSourceLink"));
  mappedSourceLink = findElement(dbg, "mappedSourceLink");
  is(
    mappedSourceLink.textContent,
    "From entry.js",
    "The link to mapped source mentions the original source"
  );

  info("Move the cursor within the bundle to another original source");
  getCM(dbg).setCursor({ line: 70, ch: 0 });
  mappedSourceLink = await waitFor(() => findElement(dbg, "mappedSourceLink"));
  is(
    mappedSourceLink.textContent,
    "From times2.js",
    "The link to mapped source updates to the newly selected original source within the bundle"
  );
});

function assertBreakpointExists(dbg, source, line) {
  const {
    selectors: { getBreakpoint },
  } = dbg;

  ok(
    getBreakpoint(createLocation({ source, line })),
    "Breakpoint has correct line"
  );
}

async function waitForBreakpointCount(dbg, count) {
  const {
    selectors: { getBreakpointCount },
  } = dbg;
  await waitForState(dbg, state => getBreakpointCount() == count);
}
