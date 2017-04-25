/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if timing intervals are divided againts seconds when appropriate.
 */

add_task(function* () {
  // Hide file, protocol, remoteip columns to make sure timing division
  // can render properly
  Services.prefs.setCharPref(
    "devtools.netmonitor.hiddenColumns",
    "[\"file\",\"protocol\",\"remoteip\"]"
  );

  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  let { document, gStore, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  gStore.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 2);
  // Timeout needed for having enough divisions on the time scale.
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests(2, null, 3000);
  });
  yield wait;

  let milDivs = document.querySelectorAll(
    ".requests-list-timings-division[data-division-scale=millisecond]");
  let secDivs = document.querySelectorAll(
    ".requests-list-timings-division[data-division-scale=second]");
  let minDivs = document.querySelectorAll(
    ".requests-list-timings-division[data-division-scale=minute]");

  info("Number of millisecond divisions: " + milDivs.length);
  info("Number of second divisions: " + secDivs.length);
  info("Number of minute divisions: " + minDivs.length);

  milDivs.forEach(div => info(`Millisecond division: ${div.textContent}`));
  secDivs.forEach(div => info(`Second division: ${div.textContent}`));
  minDivs.forEach(div => info(`Minute division: ${div.textContent}`));

  is(gStore.getState().requests.requests.size, 2,
     "There should be only two requests made.");

  let firstRequest = getSortedRequests(gStore.getState()).get(0);
  let lastRequest = getSortedRequests(gStore.getState()).get(1);

  info("First request happened at: " +
       firstRequest.responseHeaders.headers.find(e => e.name == "Date").value);
  info("Last request happened at: " +
       lastRequest.responseHeaders.headers.find(e => e.name == "Date").value);

  ok(secDivs.length,
     "There should be at least one division on the seconds time scale.");
  ok(secDivs[0].textContent.match(/\d+\.\d{2}\s\w+/),
     "The division on the seconds time scale looks legit.");

  return teardown(monitor);
});
