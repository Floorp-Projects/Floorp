"use strict";

add_task(async function closing_tab_with_dependents_should_close_window() {
  info("Opening window");
  let win = await BrowserTestUtils.openNewBrowserWindow();

  info("Opening tab with data URI");
  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, `data:text/html,<html%20onclick="W=window.open()"><body%20onbeforeunload="W.close()">`);
  info("Closing original tab in this window.");
  await BrowserTestUtils.removeTab(win.gBrowser.tabs[0]);
  info("Clicking into the window");
  let depTabOpened = BrowserTestUtils.waitForEvent(win.gBrowser.tabContainer, "TabOpen");
  await BrowserTestUtils.synthesizeMouse("html", 0, 0, {}, tab.linkedBrowser);

  let openedTab = (await depTabOpened).target;
  info("Got opened tab");

  let otherTabClosePromise = BrowserTestUtils.tabRemoved(openedTab);
  let windowClosedPromise = BrowserTestUtils.windowClosed(win);
  await BrowserTestUtils.removeTab(tab);
  info("Wait for other tab to close, this shouldn't time out");
  await otherTabClosePromise;
  is(Cu.isDeadWrapper(openedTab) || openedTab.linkedBrowser == null, true, "Opened tab should also have closed");
  info("If we timeout now, the window failed to close - that shouldn't happen!");
  await windowClosedPromise;
});

