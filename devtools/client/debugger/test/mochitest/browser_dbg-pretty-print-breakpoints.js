/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that breakpoints set in the pretty printed files display and paused
// correctly.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");
  await prettyPrint(dbg);

  await addBreakpoint(dbg, "pretty.js:formatted", 5);

  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint is created");

  invokeInTab("stuff");
  await waitForPaused(dbg);

  await assertBreakpointsInNonPrettyAndPrettySources(dbg);

  is(
    dbg.selectors.getBreakpointCount(),
    1,
    "Only one breakpoint still exists after pause "
  );

  await resume(dbg);

  info("Wait for another pause because of a debugger statement on line 8");
  await waitForPaused(dbg);

  await resume(dbg);
  assertNotPaused(dbg);

  await closeTab(dbg, "pretty.js");
  await closeTab(dbg, "pretty.js:formatted");
});

// Tests that breakpoints appear and work when you reload a page
// with pretty-printed files.
add_task(async function () {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");
  await prettyPrint(dbg);

  await addBreakpoint(dbg, "pretty.js:formatted", 5);

  info("Check that equivalent breakpoint to pretty.js (generated source)");
  await selectSource(dbg, "pretty.js");
  await assertBreakpoint(dbg, 4);

  await selectSource(dbg, "pretty.js:formatted");

  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");

  await reload(dbg, "pretty.js", "pretty.js:formatted");

  await waitForSelectedSource(dbg, "pretty.js:formatted");

  invokeInTab("stuff");
  await waitForPaused(dbg);

  await assertBreakpointsInNonPrettyAndPrettySources(dbg);

  is(
    dbg.selectors.getBreakpointCount(),
    1,
    "Only one breakpoint still exists after reload and pause "
  );
});

// Test that breakpoints appear and work when set in the minified source
add_task(async function () {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");

  info("Add breakpoint to pretty.js (generated source)");
  await addBreakpoint(dbg, "pretty.js", 4, 8);

  await prettyPrint(dbg);

  info(
    "Check that equivalent breakpoint is added to pretty.js:formatted (original source)"
  );
  await selectSource(dbg, "pretty.js:formatted");
  await assertBreakpoint(dbg, 5);

  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint created");

  await reload(dbg, "pretty.js", "pretty.js:formatted");

  await waitForSelectedSource(dbg, "pretty.js:formatted");

  invokeInTab("stuff");
  await waitForPaused(dbg);

  await assertBreakpointsInNonPrettyAndPrettySources(dbg);

  is(
    dbg.selectors.getBreakpointCount(),
    1,
    "Only one breakpoint still exists after reload and pause "
  );
});

async function assertBreakpointsInNonPrettyAndPrettySources(dbg) {
  info(
    "Asserts breakpoint pause and display on the correct line in the pretty printed source"
  );
  const prettyPrintedSource = findSource(dbg, "pretty.js:formatted");
  await assertPausedAtSourceAndLine(dbg, prettyPrintedSource.id, 5);
  await assertBreakpoint(dbg, 5);

  await selectSource(dbg, "pretty.js");

  info("Assert pause and display on the correct line in the minified source");
  const minifiedSource = findSource(dbg, "pretty.js");
  await assertPausedAtSourceAndLine(dbg, minifiedSource.id, 4, 8);
  await assertBreakpoint(dbg, 4);
}
