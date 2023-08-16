/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests adding and removing tabs

"use strict";

add_task(async function testTabsOnReload() {
  const dbg = await initDebugger(
    "doc-scripts.html",
    "simple1.js",
    "simple2.js"
  );

  await selectSource(dbg, "simple1.js");
  await selectSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);

  info("Test reloading the debugger");
  await reload(dbg, "simple1.js", "simple2.js");
  await waitForSelectedSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);

  info("Test reloading the debuggee a second time");
  await reload(dbg, "simple1.js", "simple2.js");
  await waitForSelectedSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);
});

add_task(async function testOpeningAndClosingTabs() {
  const dbg = await initDebugger(
    "doc-scripts.html",
    "simple1.js",
    "simple2.js",
    "simple3.js"
  );

  // /!\ Tabs are opened by default on the left/beginning
  // so that they are displayed in the other way around.
  // To make the test clearer insert them in a way so that
  // they are in the expected order: simple1 then simple2,...
  await selectSource(dbg, "simple3.js");
  await selectSource(dbg, "simple2.js");
  await selectSource(dbg, "simple1.js");

  info("Reselect simple2 so that we then close the selected tab");
  await selectSource(dbg, "simple2.js");
  await closeTab(dbg, "simple2.js");
  is(countTabs(dbg), 2);
  info("Removing the tab in the middle should select the following one");
  await waitForSelectedSource(dbg, "simple3.js");

  await closeTab(dbg, "simple3.js");
  is(countTabs(dbg), 1);
  info("Removing the last tab should select the first tab before");
  await waitForSelectedSource(dbg, "simple1.js");

  info("Re-open a second tab so that we can cover closing the first tab");
  await selectSource(dbg, "simple2.js");
  is(countTabs(dbg), 2);
  await closeTab(dbg, "simple1.js");
  info("Removing the first tab should select the first tab after");
  is(countTabs(dbg), 1);
  await waitForSelectedSource(dbg, "simple2.js");

  info("Close the last tab");
  await closeTab(dbg, "simple2.js");
  is(countTabs(dbg), 0);
  is(
    dbg.selectors.getSelectedLocation(),
    null,
    "Selected location is cleared when closing the last tab"
  );

  info("Test reloading the debugger");
  await reload(dbg, "simple1.js", "simple2.js", "simple3.js");
  is(countTabs(dbg), 0);

  // /!\ Tabs are opened by default on the left/beginning
  // so that they are displayed in the other way around.
  // To make the test clearer insert them in a way so that
  // they are in the expected order: simple1 then simple2,...
  await selectSource(dbg, "simple3.js");
  await selectSource(dbg, "simple2.js");
  await selectSource(dbg, "simple1.js");
  is(countTabs(dbg), 3);

  info("Reselect simple3 so that we then close the selected tab");
  await selectSource(dbg, "simple3.js");

  info("Removing the last tab, should select the one before");
  await closeTab(dbg, "simple3.js");
  is(countTabs(dbg), 2);
  await waitForSelectedSource(dbg, "simple2.js");

  info("Test the close all tabs context menu");
  const waitForOpen = waitForContextMenu(dbg);
  info(`Open the current active tab context menu`);
  rightClickElement(dbg, "activeTab");
  await waitForOpen;
  const onCloseTabsAction = waitForDispatch(dbg.store, "CLOSE_TABS");
  info(`Select the close all tabs context menu item`);
  selectContextMenuItem(dbg, `#node-menu-close-all-tabs`);
  await onCloseTabsAction;
  is(countTabs(dbg), 0);
});
