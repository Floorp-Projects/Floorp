/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Testing displaying breakpoints in the breakpoints list and the tooltip
// shows the source url.

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
    source1.url,
    "The breakpoint heading tooltip shows the source info for the first breakpoint"
  );
  is(
    breakpointHeadings[0].textContent,
    "simple1.js",
    "The info displayed for the breakpoint heading of the 1st breakpoint is correct"
  );
  is(
    breakpointItems[0].textContent,
    "func();5:18",
    "The info displayed for the 1st breakpoint is correct"
  );

  is(
    breakpointHeadings[1].title,
    source2.url,
    "The breakpoint heading tooltip shows the source info for the second breakpoint"
  );
  is(
    breakpointHeadings[1].textContent,
    "simple2.js",
    "The info displayed for the breakpoint heading of the 2nd breakpoint is correct"
  );
  is(
    breakpointItems[1].textContent,
    "return x + y;3:5",
    "The info displayed for the 2nd breakpoint is correct"
  );

  await removeBreakpoint(dbg, source1.id, 5);
  await removeBreakpoint(dbg, source2.id, 3);
});

add_task(async function testBreakpointsListForOriginalFiles() {
  const dbg = await initDebugger("doc-sourcemaps.html", "entry.js");

  info("Add breakpoint to the entry.js (original source)");
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
    source.url,
    "The breakpoint heading tooltip shows the source info for the first breakpoint"
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

add_task(async function testBreakpointsListForIgnoredLines() {
  const dbg = await initDebugger("doc-sourcemaps.html", "entry.js");

  info("Add breakpoint to the entry.js (original source)");
  await selectSource(dbg, "entry.js");
  await addBreakpoint(dbg, "entry.js", 5);

  info("Ignoring line 5 to 6 which has a breakpoint already set");
  await selectEditorLinesAndOpenContextMenu(dbg, {
    startLine: 5,
    endLine: 6,
  });
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  info("Assert the breakpoint on the ignored line");
  let breakpointItems = findAllElements(dbg, "breakpointItems");
  is(
    breakpointItems[0].textContent,
    "output(times2(1));5",
    "The info displayed for the 1st breakpoint is correct"
  );
  const firstBreakpointCheck = breakpointItems[0].querySelector("input");
  ok(
    firstBreakpointCheck.disabled,
    "The first breakpoint checkbox on an ignored line is disabled"
  );
  ok(
    !firstBreakpointCheck.checked,
    "The first breakpoint on an ignored line is not checked"
  );

  info("Ignoring line 8 to 9 which currently has not breakpoint");
  await selectEditorLinesAndOpenContextMenu(dbg, {
    startLine: 8,
    endLine: 9,
  });
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  await addBreakpointViaGutter(dbg, 9);

  breakpointItems = findAllElements(dbg, "breakpointItems");
  is(
    breakpointItems[1].textContent,
    "output(times2(3));9",
    "The info displayed for the 2nd breakpoint is correct"
  );
  const secondBreakpointCheck = breakpointItems[1].querySelector("input");
  ok(
    secondBreakpointCheck.disabled,
    "The second breakpoint checkbox on an ignored line is disabled"
  );
  ok(
    !secondBreakpointCheck.checked,
    "The second breakpoint on an ignored line is not checked"
  );

  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "UNBLACKBOX_WHOLE_SOURCES");

  info("Assert that both breakpoints are now enabled");
  breakpointItems = findAllElements(dbg, "breakpointItems");
  [...breakpointItems].forEach(breakpointItem => {
    const check = breakpointItem.querySelector("input");
    ok(
      !check.disabled,
      "The breakpoint checkbox on the unignored line is enabled"
    );
    ok(check.checked, "The breakpoint on the unignored line is checked");
  });

  await dbg.toolbox.closeToolbox();
});
