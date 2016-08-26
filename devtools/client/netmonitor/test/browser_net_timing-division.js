/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if timing intervals are divided againts seconds when appropriate.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  let { $all, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 2);
  // Timeout needed for having enough divisions on the time scale.
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(2, null, 3000);
  });
  yield wait;

  let milDivs = $all(".requests-menu-timings-division[division-scale=millisecond]");
  let secDivs = $all(".requests-menu-timings-division[division-scale=second]");
  let minDivs = $all(".requests-menu-timings-division[division-scale=minute]");

  info("Number of millisecond divisions: " + milDivs.length);
  info("Number of second divisions: " + secDivs.length);
  info("Number of minute divisions: " + minDivs.length);

  for (let div of milDivs) {
    info("Millisecond division: " + div.getAttribute("value"));
  }
  for (let div of secDivs) {
    info("Second division: " + div.getAttribute("value"));
  }
  for (let div of minDivs) {
    info("Minute division: " + div.getAttribute("value"));
  }

  is(RequestsMenu.itemCount, 2,
    "There should be only two requests made.");

  let firstRequest = RequestsMenu.getItemAtIndex(0);
  let lastRequest = RequestsMenu.getItemAtIndex(1);

  info("First request happened at: " +
    firstRequest.attachment.responseHeaders.headers.find(e => e.name == "Date").value);
  info("Last request happened at: " +
    lastRequest.attachment.responseHeaders.headers.find(e => e.name == "Date").value);

  ok(secDivs.length,
    "There should be at least one division on the seconds time scale.");
  ok(secDivs[0].getAttribute("value").match(/\d+\.\d{2}\s\w+/),
    "The division on the seconds time scale looks legit.");

  return teardown(monitor);
});
