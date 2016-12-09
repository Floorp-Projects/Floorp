/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if beacons from other tabs are properly ignored.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_URL);
  let { RequestsMenu } = monitor.panelWin.NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  let beaconTab = yield addTab(SEND_BEACON_URL);
  info("Beacon tab added successfully.");

  is(RequestsMenu.itemCount, 0, "The requests menu should be empty.");

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(beaconTab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequest();
  });
  tab.linkedBrowser.reload();
  yield wait;

  is(RequestsMenu.itemCount, 1, "Only the reload should be recorded.");
  let request = RequestsMenu.getItemAtIndex(0);
  is(request.attachment.method, "GET", "The method is correct.");
  is(request.attachment.status, "200", "The status is correct.");

  yield removeTab(beaconTab);
  return teardown(monitor);
});
