/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if selecting a tab in a tab bar makes it visible
 */
add_task(async function () {
  Services.prefs.clearUserPref(
    "devtools.netmonitor.panes-network-details-width"
  );

  const { tab, monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const topMostDocument = DevToolsUtils.getTopWindow(
    document.defaultView
  ).document;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  const networkEvent = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await networkEvent;

  store.dispatch(Actions.toggleNetworkDetails());

  const splitter = document.querySelector(".splitter");

  await EventUtils.synthesizeMouse(
    splitter,
    0,
    1,
    { type: "mousedown" },
    monitor.panelWin
  );
  await EventUtils.synthesizeMouse(
    splitter,
    300,
    1,
    { type: "mousemove" },
    monitor.panelWin
  );
  await EventUtils.synthesizeMouse(
    splitter,
    300,
    1,
    { type: "mouseup" },
    monitor.panelWin
  );

  await waitUntil(() => document.querySelector(".all-tabs-menu"));
  const allTabsMenu = document.querySelector(".all-tabs-menu");
  const panelsWidth = document.querySelector(".tabs-menu").offsetWidth;

  const selectTabFromTabsMenuButton = async id => {
    EventUtils.sendMouseEvent({ type: "click" }, allTabsMenu);
    const tabMenuElement = topMostDocument.querySelector(
      `#devtools-sidebar-${id}`
    );
    if (tabMenuElement != null) {
      tabMenuElement.click();
      // The tab should be visible within the panel
      const tabLi = document.querySelector(`#${id}-tab`).parentElement;
      const ulScrollPos =
        tabLi.parentElement.scrollLeft + tabLi.parentElement.offsetLeft;
      ok(
        tabLi.offsetLeft >= ulScrollPos &&
          tabLi.offsetLeft + tabLi.offsetWidth <= panelsWidth + ulScrollPos,
        `The ${id} tab is visible`
      );
    }
  };

  for (const elem of [
    "headers",
    "cookies",
    "request",
    "response",
    "timings",
    "security",
  ]) {
    await selectTabFromTabsMenuButton(elem);
  }

  await teardown(monitor);
});
