/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Network throttling integration test.

"use strict";

add_task(async function() {
  await throttleTest(true);
  await throttleTest(false);
});

async function throttleTest(actuallyThrottle) {
  requestLongerTimeout(2);

  const { monitor } = await initNetMonitor(SIMPLE_URL);
  const { store, windowRequire, connector } = monitor.panelWin;
  const { ACTIVITY_TYPE } = windowRequire("devtools/client/netmonitor/src/constants");
  const { setPreferences, triggerActivity } = connector;
  const {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  info("Starting test... (actuallyThrottle = " + actuallyThrottle + ")");

  // When throttling, must be smaller than the length of the content
  // of SIMPLE_URL in bytes.
  const size = actuallyThrottle ? 200 : 0;

  const request = {
    "NetworkMonitor.throttleData": {
      latencyMean: 0,
      latencyMax: 0,
      downloadBPSMean: size,
      downloadBPSMax: size,
      uploadBPSMean: 10000,
      uploadBPSMax: 10000,
    },
  };

  info("sending throttle request");
  await new Promise((resolve) => {
    setPreferences(request, response => {
      resolve(response);
    });
  });

  const wait = waitForNetworkEvents(monitor, 1);
  await triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_DISABLED);
  await wait;

  await waitForRequestData(store, ["eventTimings"]);

  const requestItem = getSortedRequests(store.getState()).get(0);
  const reportedOneSecond = requestItem.eventTimings.timings.receive > 1000;
  if (actuallyThrottle) {
    ok(reportedOneSecond, "download reported as taking more than one second");
  } else {
    ok(!reportedOneSecond, "download reported as taking less than one second");
  }

  await teardown(monitor);
}
