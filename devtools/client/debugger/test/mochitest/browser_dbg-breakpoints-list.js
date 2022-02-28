/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing displaying breakpoints in the breakpoints list and the tooltip
// shows the thread information.

"use strict";

add_task(async function testBreakpointsListForMultipleTargets() {
  const dbg = await initDebugger(
    "doc_dbg-fission-frame-sources.html",
    "simple1.js",
    "simple2.js"
  );

  info("Add breakpoint to the source (simple1.js) in the main thread");
  await selectSource(dbg, "simple1.js");
  const source1 = findSource(dbg, "simple1.js");
  await addBreakpoint(dbg, "simple1.js", 5);

  info("Add breakpoint to the source (simple2.js) in the frame");
  await selectSource(dbg, "simple2.js");
  const source2 = findSource(dbg, "simple2.js");
  await addBreakpoint(dbg, "simple2.js", 3);

  const breakpointHeadings = findAllElements(dbg, "breakpointHeadings");
  const breakpointItems = findAllElements(dbg, "breakpointItems");

  is(
    breakpointHeadings.length,
    2,
    "The breakpoint list shows two breakpoints sources"
  );
  is(
    breakpointItems.length,
    2,
    "The breakpoint list shows only two breakpoints"
  );

  is(
    breakpointHeadings[0].title,
    `Main Thread - ${source1.url}`,
    "The breakpoint heading tooltip shows the thread and source info for the first breakpoint"
  );
  is(
    breakpointHeadings[0].textContent,
    "simple1.js",
    "The info displayed for the breakpoint heading of the 1st breakpoint is correct"
  );
  is(
    breakpointItems[0].textContent,
    "func();5:17",
    "The info displayed for the 1st breakpoint is correct"
  );

  // With fission and EFT disabled all the sources are going to be under the Main Thread
  const expectedThreadName =
    !Services.prefs.getBoolPref("fission.autostart") &&
    !Services.prefs.getBoolPref("devtools.every-frame-target.enabled")
      ? "Main Thread"
      : "Test remote frame sources";

  is(
    breakpointHeadings[1].title,
    `${expectedThreadName} - ${source2.url}`,
    "The breakpoint heading tooltip shows the thread and source info for the second breakpoint"
  );
  is(
    breakpointHeadings[1].textContent,
    "simple2.js",
    "The info displayed for the breakpoint heading of the 2nd breakpoint is correct"
  );
  is(
    breakpointItems[1].textContent,
    "return x + y;3:4",
    "The info displayed for the 2nd breakpoint is correct"
  );

  await removeBreakpoint(dbg, source1.id, 5);
  await removeBreakpoint(dbg, source2.id, 3);
});

add_task(async function testBreakpointsListForOriginalFiles() {
  const dbg = await initDebugger("doc-sourcemaps.html", "entry.js");

  info("Add breakpoint to the entry.js (original source) in the main thread");
  await selectSource(dbg, "entry.js");
  const source = findSource(dbg, "entry.js");
  await addBreakpoint(dbg, "entry.js", 5);

  const breakpointHeadings = findAllElements(dbg, "breakpointHeadings");
  const breakpointItems = findAllElements(dbg, "breakpointItems");

  is(
    breakpointHeadings.length,
    1,
    "The breakpoint list shows one breakpoints sources"
  );
  is(
    breakpointItems.length,
    1,
    "The breakpoint list shows only one breakpoints"
  );

  is(
    breakpointHeadings[0].title,
    `Main Thread - ${source.url}`,
    "The breakpoint heading tooltip shows the thread and source info for the first breakpoint"
  );
  is(
    breakpointHeadings[0].textContent,
    "entry.js",
    "The info displayed for the breakpoint heading of the 1st breakpoint is correct"
  );
  is(
    breakpointItems[0].textContent,
    "output(times2(1));5",
    "The info displayed for the 1st breakpoint is correct"
  );

  await removeBreakpoint(dbg, source.id, 5);
});
