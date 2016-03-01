/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Network throttling integration test.

"use strict";

add_task(function* () {
  requestLongerTimeout(2);

  let [, , monitor] = yield initNetMonitor(SIMPLE_URL);
  const {ACTIVITY_TYPE, NetMonitorController, NetMonitorView} =
        monitor.panelWin;

  info("Starting test... ");

  const request = {
    "NetworkMonitor.throttleData": {
      roundTripTimeMean: 0,
      roundTripTimeMax: 0,
      // Must be smaller than the length of the content of SIMPLE_URL
      // in bytes.
      downloadBPSMean: 200,
      downloadBPSMax: 200,
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

  const startTime = Date.now();
  let eventPromise =
      monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_EVENT_TIMINGS);
  yield NetMonitorController
    .triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_DISABLED);
  const endTime = Date.now();
  ok(endTime - startTime > 1000, "download took more than one second");

  yield eventPromise;
  let requestItem = NetMonitorView.RequestsMenu.getItemAtIndex(0);
  ok(requestItem.attachment.eventTimings.timings.receive > 1000,
     "download reported as taking more than one second");

  yield teardown(monitor);
  finish();
});
