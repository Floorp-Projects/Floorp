/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Network throttling integration test.

"use strict";

add_task(function* () {
  yield throttleTest(true);
  yield throttleTest(false);
});

function* throttleTest(actuallyThrottle) {
  requestLongerTimeout(2);

  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  const {ACTIVITY_TYPE, EVENTS, NetMonitorController, NetMonitorView} = monitor.panelWin;

  info("Starting test... (actuallyThrottle = " + actuallyThrottle + ")");

  // When throttling, must be smaller than the length of the content
  // of SIMPLE_URL in bytes.
  const size = actuallyThrottle ? 200 : 0;

  const request = {
    "NetworkMonitor.throttleData": {
      roundTripTimeMean: 0,
      roundTripTimeMax: 0,
      downloadBPSMean: size,
      downloadBPSMax: size,
      uploadBPSMean: 10000,
      uploadBPSMax: 10000,
    },
  };
  let client = monitor._controller.webConsoleClient;

  info("sending throttle request");
  let deferred = promise.defer();
  client.setPreferences(request, response => {
    deferred.resolve(response);
  });
  yield deferred.promise;

  let eventPromise = monitor.panelWin.once(EVENTS.RECEIVED_EVENT_TIMINGS);
  yield NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_DISABLED);
  yield eventPromise;

  let requestItem = NetMonitorView.RequestsMenu.getItemAtIndex(0);
  const reportedOneSecond = requestItem.attachment.eventTimings.timings.receive > 1000;
  if (actuallyThrottle) {
    ok(reportedOneSecond, "download reported as taking more than one second");
  } else {
    ok(!reportedOneSecond, "download reported as taking less than one second");
  }

  yield teardown(monitor);
}
