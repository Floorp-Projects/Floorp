"use strict";

add_task(function* closing_tab_with_dependents_should_close_window() {
  info("Opening window");
  let win = yield BrowserTestUtils.openNewBrowserWindow();

  info("Opening tab with data URI");
  let tab = yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, `data:text/html,<html%20onclick="W=window.open()"><body%20onbeforeunload="W.close()">`);
  info("Closing original tab in this window.");
  yield BrowserTestUtils.removeTab(win.gBrowser.tabs[0]);
  info("Clicking into the window");
  let depTabOpened = BrowserTestUtils.waitForEvent(win.gBrowser.tabContainer, "TabOpen");
  yield BrowserTestUtils.synthesizeMouse("html", 0, 0, {}, tab.linkedBrowser);

  let openedTab = (yield depTabOpened).target;
  info("Got opened tab");

  let windowClosedPromise = BrowserTestUtils.windowClosed(win);
  yield BrowserTestUtils.removeTab(tab);
  is(Cu.isDeadWrapper(openedTab) || openedTab.linkedBrowser == null, true, "Opened tab should also have closed");
  info("If we timeout now, the window failed to close - that shouldn't happen!");
  yield windowClosedPromise;
});

