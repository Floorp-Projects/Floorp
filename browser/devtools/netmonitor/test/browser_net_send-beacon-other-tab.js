/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if beacons from other tabs are properly ignored.
 */

var test = Task.async(function*() {
  let [, debuggee, monitor] = yield initNetMonitor(SIMPLE_URL);
  let { RequestsMenu } = monitor.panelWin.NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  let tab = yield addTab(SEND_BEACON_URL);
  let beaconDebuggee = tab.linkedBrowser.contentWindow.wrappedJSObject;
  info("Beacon tab added successfully.");

  is(RequestsMenu.itemCount, 0, "The requests menu should be empty.");

  beaconDebuggee.performRequest();
  debuggee.location.reload();

  yield waitForNetworkEvents(monitor, 1);
  is(RequestsMenu.itemCount, 1, "Only the reload should be recorded.");
  let request = RequestsMenu.getItemAtIndex(0);
  is(request.attachment.method, "GET", "The method is correct.");
  is(request.attachment.status, "200", "The status is correct.");

  yield teardown(monitor);
  removeTab(tab);
  finish();
});
