/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that breakpoints in pretty printed files
// are properly removed when the user deletes them on the generated page.
add_task(async function() {
  const dbg = await initDebugger("doc-pretty.html", "pretty.js");

  await selectSource(dbg, "pretty.js");
  await prettyPrint(dbg);

  await addBreakpoint(dbg, "pretty.js:formatted", 5);

  info(`Close the pretty-printed, so it is not pretty-printed on reload`);
  await waitForBreakpointCount(dbg, 1);
  await closeTab(dbg, "pretty.js:formatted");

  info(`Remove the breakpoint from pretty.js`);
  clickGutter(dbg, 4);
  await waitForBreakpointCount(dbg, 0);

  info("Reload and pretty print pretty.js");
  await reload(dbg, "pretty.js", "pretty.js");
  await selectSource(dbg, "pretty.js");
  await prettyPrint(dbg);

  info(`Confirm that pretty.js:formatted does not have any breakpoints`);
  is(dbg.selectors.getBreakpointCount(), 0, "Breakpoints should be cleared");
});
