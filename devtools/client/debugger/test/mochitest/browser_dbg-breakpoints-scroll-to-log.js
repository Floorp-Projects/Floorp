/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Confirms the editor scrolls to the correct line when a log point is set or edited from
// the breakpoints context menu.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html");
  await selectSource(dbg, "long.js");
  await waitForSelectedSource(dbg, "long.js");

  await addBreakpoint(dbg, "long.js", 66);
  await waitForBreakpoint(dbg, "long.js", 66);

  info("scroll editor to the top");
  const cm = getCM(dbg);
  cm.scrollTo(0, 0);

  is(cm.getScrollInfo().top, 0, "editor is scrolled to the top.");

  info("open breakpointContextMenu and select add log");
  await openBreakpointContextMenu(dbg);

  selectContextMenuItem(
    dbg,
    `${selectors.addLogItem},${selectors.editLogItem}`
  );

  info("set the log value");
  await typeInPanel(dbg, "this.todos");
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");

  is(cm.getScrollInfo().top, 870, "editor is scrolled to the log input line.");

  info("scroll editor to the top");
  cm.scrollTo(0, 0);

  is(cm.getScrollInfo().top, 0, "editor is scrolled to the top.");

  info("open breakpointContextMenu and select edit log");
  await openBreakpointContextMenu(dbg);

  selectContextMenuItem(
    dbg,
    `${selectors.addLogItem},${selectors.editLogItem}`
  );

  info("set the log value");
  await typeInPanel(dbg, ".id");
  await waitForDispatch(dbg.store, "SET_BREAKPOINT");

  is(cm.getScrollInfo().top, 870, "editor is scrolled to the log input line.");
});

async function openBreakpointContextMenu(dbg) {
  rightClickElement(dbg, "breakpointItem", 2);
  await waitForContextMenu(dbg);
}
