/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the editor does not crash when closing a tab (using the contextmenu)

"use strict";

add_task(async function testEditorExceptionClose() {
  const dbg = await initDebugger("doc-exceptions.html", "exceptions.js");
  await selectSource(dbg, "exceptions.js");
  is(countTabs(dbg), 1, "Tab for the exceptions.js source is open");
  await openActiveTabContextMenuAndSelectCloseTabItem(dbg);
  is(countTabs(dbg), 0, "All tabs are closed");
});

async function openActiveTabContextMenuAndSelectCloseTabItem(dbg) {
  const waitForOpen = waitForContextMenu(dbg);
  info(`Open the current active tab context menu`);
  rightClickElement(dbg, "activeTab");
  await waitForOpen;
  const wait = waitForDispatch(dbg.store, "CLOSE_TABS");
  info(`Select the close tab context menu item`);
  selectContextMenuItem(dbg, `#node-menu-close-tab`);
  return wait;
}
