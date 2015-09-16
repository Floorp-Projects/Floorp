/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if beacons are handled correctly.
 */

var test = Task.async(function*() {
  let [, debuggee, monitor] = yield initNetMonitor(SEND_BEACON_URL);
  let { RequestsMenu } = monitor.panelWin.NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  is(RequestsMenu.itemCount, 0, "The requests menu should be empty.");

  debuggee.performRequest();

  yield waitForNetworkEvents(monitor, 1);
  is(RequestsMenu.itemCount, 1, "The beacon should be recorded.");
  let request = RequestsMenu.getItemAtIndex(0);
  is(request.attachment.method, "POST", "The method is correct.");
  ok(request.attachment.url.endsWith("beacon_request"), "The URL is correct.");
  is(request.attachment.status, "404", "The status is correct.");

  yield teardown(monitor);
  finish();
});
