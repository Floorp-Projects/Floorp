/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test enabling and disabling a debugger statement using editor context menu
add_task(async function() {
  const dbg = await initDebugger("doc-pause-points.html", "pause-points.js");
  await selectSource(dbg, "pause-points.js");
  await waitForSelectedSource(dbg, "pause-points.js");

  await addBreakpoint(dbg, "pause-points.js", 9);

  info("Disable the debugger statement on line 9");
  rightClickElement(dbg, "gutter", 9);
  selectContextMenuItem(
    dbg,
    selectors.breakpointContextMenu.disableDbgStatement
  );
  await waitForCondition(dbg, "false");

  let bp = findBreakpoint(dbg, "pause-points.js", 9);
  is(
    bp.options.condition,
    "false",
    "The breakpoint for debugger statement changes to a conditional breakpoint with 'false' as statement"
  );

  info("Enable the previously disabled debugger statement");
  rightClickElement(dbg, "gutter", 9);
  selectContextMenuItem(
    dbg,
    selectors.breakpointContextMenu.enableDbgStatement
  );
  await waitForBreakpointWithoutCondition(dbg, "pause-points.js", 9);

  bp = findBreakpoint(dbg, "pause-points.js", 9);
  is(bp.options.condition, null, "The conditional statement is removed");
});

function waitForBreakpointWithoutCondition(dbg, url, line) {
  return waitForState(dbg, () => {
    const bp = findBreakpoint(dbg, url, line);
    return bp && !bp.options.condition;
  });
}
