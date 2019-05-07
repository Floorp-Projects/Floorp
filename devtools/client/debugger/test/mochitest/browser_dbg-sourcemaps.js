/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests loading sourcemapped sources, setting breakpoints, and
// stepping in them.
requestLongerTimeout(2);

function assertBreakpointExists(dbg, source, line) {
  const {
    selectors: { getBreakpoint },
    getState
  } = dbg;

  ok(
    getBreakpoint({ sourceId: source.id, line }),
    "Breakpoint has correct line"
  );
}

async function assertEditorBreakpoint(dbg, line, shouldExist) {
  const el = await getLineEl(dbg, line);
  const exists = !!el.querySelector(".new-breakpoint");
  ok(
    exists === shouldExist,
    `Breakpoint ${shouldExist ? "exists" : "does not exist"} on line ${line}`
  );
}

async function getLineEl(dbg, line) {
  let el = await codeMirrorGutterElement(dbg, line);
  while (el && !el.matches(".CodeMirror-code > div")) {
    el = el.parentElement;
  }
  return el;
}

async function clickGutter(dbg, line) {
  const el = await codeMirrorGutterElement(dbg, line);
  clickDOMElement(dbg, el);
}

async function waitForBreakpointCount(dbg, count) {
  const {
    selectors: { getBreakpointCount },
    getState
  } = dbg;
  await waitForState(dbg, state => getBreakpointCount() == count);
}

add_task(async function() {
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
    getState
  } = dbg;

  ok(true, "Original sources exist");
  const bundleSrc = findSource(dbg, "bundle.js");

  // Check that the original sources appear in the source tree
  await clickElement(dbg, "sourceDirectoryLabel", 4);
  await assertSourceCount(dbg, 9);

  await selectSource(dbg, bundleSrc);

  await clickGutter(dbg, 70);
  await waitForBreakpointCount(dbg, 1);
  await assertEditorBreakpoint(dbg, 70, true);

  await clickGutter(dbg, 70);
  await waitForBreakpointCount(dbg, 0);
  is(dbg.selectors.getBreakpointCount(), 0, "No breakpoints exists");

  const entrySrc = findSource(dbg, "entry.js");

  await selectSource(dbg, entrySrc);
  ok(
    getCM(dbg)
      .getValue()
      .includes("window.keepMeAlive"),
    "Original source text loaded correctly"
  );

  // Test breaking on a breakpoint
  await addBreakpoint(dbg, "entry.js", 15);
  is(getBreakpointCount(), 1, "One breakpoint exists");
  assertBreakpointExists(dbg, entrySrc, 15);

  invokeInTab("keepMeAlive");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  await stepIn(dbg);
  assertPausedLocation(dbg);

  await dbg.actions.jumpToMappedSelectedLocation(getContext(dbg));
  await stepOver(dbg);
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 71);

  await dbg.actions.jumpToMappedSelectedLocation(getContext(dbg));
  await stepOut(dbg);
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 16);
});
