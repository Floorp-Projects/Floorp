/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// - Tests adding a watchpoint
// - Tests removing a watchpoint
// - Tests adding a watchpoint, resuming to after the youngest frame has popped,
// then removing and adding a watchpoint during the same pause

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-sources.html");

  // Do not await for navigation as an early breakpoint pauses the document load
  const onNavigated = navigateTo(`${EXAMPLE_URL}doc-watchpoints.html`);
  await waitForSources(dbg, "doc-watchpoints.html");
  await selectSource(dbg, "doc-watchpoints.html");
  await waitForPaused(dbg);
  const sourceId = findSource(dbg, "doc-watchpoints.html").id;

  info("Add a get watchpoint at b");
  await toggleScopeNode(dbg, 3);
  const addedWatchpoint = waitForDispatch(dbg.store, "SET_WATCHPOINT");
  await rightClickScopeNode(dbg, 5);
  let popup = await waitForContextMenu(dbg);
  let submenu = await openContextMenuSubmenu(dbg, selectors.watchpointsSubmenu);
  const getWatchpointItem = document.querySelector(selectors.addGetWatchpoint);
  submenu.activateItem(getWatchpointItem);
  await addedWatchpoint;
  popup.hidePopup();

  await resume(dbg);
  await waitForPaused(dbg);
  await waitForState(dbg, () => dbg.selectors.getSelectedInlinePreviews());
  assertPausedAtSourceAndLine(dbg, sourceId, 17);
  is(await getScopeNodeValue(dbg, 5), "3");
  const whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(whyPaused, `Paused on property get\nobj.b`);

  info("Resume and wait to pause at the access to b in the first `obj.b;`");
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sourceId, 19);

  info("Remove the get watchpoint on b");
  const removedWatchpoint1 = waitForDispatch(dbg.store, "REMOVE_WATCHPOINT");
  const el1 = await waitForElementWithSelector(dbg, ".remove-watchpoint-get");
  el1.scrollIntoView();
  clickElementWithSelector(dbg, ".remove-watchpoint-get");
  await removedWatchpoint1;

  info(
    "Resume and wait to skip the second `obj.b` and pause on the debugger statement"
  );
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sourceId, 21);

  info("Resume and pause on the debugger statement in getB");
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sourceId, 5);

  info("Add a get watchpoint to b");
  await toggleScopeNode(dbg, 4);
  const addedWatchpoint2 = waitForDispatch(dbg.store, "SET_WATCHPOINT");
  await rightClickScopeNode(dbg, 6);
  popup = await waitForContextMenu(dbg);
  submenu = await openContextMenuSubmenu(dbg, selectors.watchpointsSubmenu);
  const getWatchpointItem2 = document.querySelector(selectors.addGetWatchpoint);
  submenu.activateItem(getWatchpointItem2);
  await addedWatchpoint2;
  popup.hidePopup();

  info("Resume and wait to pause at the access to b in getB");
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sourceId, 6);

  info("Resume and pause on the debugger statement");
  await waitForRequestsToSettle(dbg);
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sourceId, 24);

  info("Remove the get watchpoint on b");
  const removedWatchpoint2 = waitForDispatch(dbg.store, "REMOVE_WATCHPOINT");
  await toggleScopeNode(dbg, 3);

  const el2 = await waitForElementWithSelector(dbg, ".remove-watchpoint-get");
  el2.scrollIntoView();
  clickElementWithSelector(dbg, ".remove-watchpoint-get");
  await removedWatchpoint2;

  info("Add back the get watchpoint on b");
  const addedWatchpoint3 = waitForDispatch(dbg.store, "SET_WATCHPOINT");
  await rightClickScopeNode(dbg, 5);
  popup = await waitForContextMenu(dbg);
  submenu = await openContextMenuSubmenu(dbg, selectors.watchpointsSubmenu);
  const getWatchpointItem3 = document.querySelector(selectors.addGetWatchpoint);
  submenu.activateItem(getWatchpointItem3);
  await addedWatchpoint3;
  popup.hidePopup();

  info("Resume and wait to pause on the final `obj.b;`");
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, sourceId, 25);

  info("Do a last resume to finalize the document load");
  await resume(dbg);
  await onNavigated;

  await waitForRequestsToSettle(dbg);
});
