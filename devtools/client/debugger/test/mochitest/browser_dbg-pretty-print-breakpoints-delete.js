/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that breakpoints in pretty printed files
// are properly removed when the user deletes them on either the generated or original files.

"use strict";

add_task(async function () {
  info(
    "Test removing the breakpoint from the minified file (generated source) works"
  );

  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");

  await prettyPrint(dbg);

  await addBreakpointToPrettyPrintedFile(dbg);

  info(
    `Close the pretty-printed source, so it is not automatically reopened on reload`
  );
  await closeTab(dbg, "pretty.js:formatted");

  await waitForSelectedSource(dbg, "pretty.js");

  info(
    "Assert that a equivalent breakpoint was set in pretty.js (generated source)"
  );
  await assertBreakpoint(dbg, 4);

  info(`Remove the breakpoint from pretty.js (generated source)`);
  await clickGutter(dbg, 4);

  await waitForBreakpointCount(dbg, 0);

  await reloadAndCheckNoBreakpointExists(dbg);
});

add_task(async function () {
  info(
    "Test removing the breakpoint from the pretty printed (original source) works"
  );

  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");

  await prettyPrint(dbg);

  await addBreakpointToPrettyPrintedFile(dbg);

  info("Check that breakpoint gets added to pretty.js (generated source)");
  await selectSource(dbg, "pretty.js");
  await assertBreakpoint(dbg, 4);

  info("Close the  pretty.js (generated source)");
  await closeTab(dbg, "pretty.js");

  await waitForSelectedSource(dbg, "pretty.js:formatted");

  info(`Remove the breakpoint from pretty.js:formatted (original source)`);
  await clickGutter(dbg, 5);

  await waitForBreakpointCount(dbg, 0);

  info(
    `Close the pretty-printed source, so it is not automatically reopened on reload`
  );
  await closeTab(dbg, "pretty.js:formatted");

  await reloadAndCheckNoBreakpointExists(dbg);
});

async function addBreakpointToPrettyPrintedFile(dbg) {
  // This breakpoint would be set before the debugger statement
  // and should be hit first.
  info("Add breakpoint to pretty.js:formatted (original source)");
  await addBreakpoint(dbg, "pretty.js:formatted", 5);
  await waitForBreakpointCount(dbg, 1);
}

async function reloadAndCheckNoBreakpointExists(dbg) {
  info("Reload and pretty print pretty.js");
  await reload(dbg, "pretty.js");
  await selectSource(dbg, "pretty.js");
  await prettyPrint(dbg);
  info("Check that we do not pause on the removed breakpoint");
  invokeInTab("stuff");
  await waitForPaused(dbg);

  const sourcePretty = findSource(dbg, "pretty.js:formatted");
  info(
    "Assert pause at the debugger statement in pretty.js:formatted (original source) and not the removed breakpoint"
  );
  assertPausedAtSourceAndLine(dbg, sourcePretty.id, 8);

  await selectSource(dbg, "pretty.js");
  const source = findSource(dbg, "pretty.js");

  info(
    "Assert pause at the debugger statement in pretty.js (generated source) and not the removed breakpoint"
  );
  assertPausedAtSourceAndLine(dbg, source.id, 6);

  info(`Confirm that pretty.js:formatted does not have any breakpoints`);
  is(dbg.selectors.getBreakpointCount(), 0, "Breakpoints should be cleared");

  await resume(dbg);
}
