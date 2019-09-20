/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests adding a watchpoint

add_task(async function() {
  pushPref("devtools.debugger.features.watchpoints", true);
  const dbg = await initDebugger("doc-sources.html");

  await navigate(dbg, "doc-watchpoints.html", "doc-watchpoints.html");
  await selectSource(dbg, "doc-watchpoints.html");
  await waitForPaused(dbg);

  info(`Add a get watchpoint at b`);
  await toggleScopeNode(dbg, 3);
  const addedWatchpoint = waitForDispatch(dbg, "SET_WATCHPOINT");
  await rightClickScopeNode(dbg, 5);
  selectContextMenuItem(dbg, selectors.watchpointsSubmenu);
  document.querySelector(selectors.addGetWatchpoint).click();
  await addedWatchpoint;

  info(`Resume and wait to pause at the access to b on line 12`);
  resume(dbg);
  await waitForPaused(dbg);
  await waitForState(dbg, () => dbg.selectors.getSelectedInlinePreviews());
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-watchpoints.html").id,
    12
  );

  const removedWatchpoint = waitForDispatch(dbg, "REMOVE_WATCHPOINT");
  await rightClickScopeNode(dbg, 5);
  selectContextMenuItem(dbg, selectors.removeWatchpoint);
  await removedWatchpoint;
  resume(dbg);
  await waitForRequestsToSettle(dbg);
});
