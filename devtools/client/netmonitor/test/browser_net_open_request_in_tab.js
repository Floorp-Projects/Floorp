/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Open in new tab works.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test...");

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(1);
  });
  yield wait;

  let requestItem = RequestsMenu.getItemAtIndex(0);
  RequestsMenu.selectedItem = requestItem;

  let onTabOpen = once(gBrowser.tabContainer, "TabOpen", false);
  RequestsMenu.openRequestInTab();
  yield onTabOpen;

  ok(true, "A new tab has been opened");

  yield teardown(monitor);

  gBrowser.removeCurrentTab();
});
