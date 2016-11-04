/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test timing of upload when throttling.

"use strict";

add_task(function* () {
  yield throttleUploadTest(true);
  yield throttleUploadTest(false);
});

function* throttleUploadTest(actuallyThrottle) {
  let { tab, monitor } = yield initNetMonitor(
    HAR_EXAMPLE_URL + "html_har_post-data-test-page.html");

  info("Starting test... (actuallyThrottle = " + actuallyThrottle + ")");

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  const size = 4096;
  const uploadSize = actuallyThrottle ? size / 3 : 0;

  const request = {
    "NetworkMonitor.throttleData": {
      roundTripTimeMean: 0,
      roundTripTimeMax: 0,
      downloadBPSMean: 200000,
      downloadBPSMax: 200000,
      uploadBPSMean: uploadSize,
      uploadBPSMax: uploadSize,
    },
  };
  let client = monitor._controller.webConsoleClient;

  info("sending throttle request");
  let deferred = promise.defer();
  client.setPreferences(request, response => {
    deferred.resolve(response);
  });
  yield deferred.promise;

  RequestsMenu.lazyUpdate = false;

  // Execute one POST request on the page and wait till its done.
  let wait = waitForNetworkEvents(monitor, 0, 1);
  yield ContentTask.spawn(tab.linkedBrowser, { size }, function* (args) {
    content.wrappedJSObject.executeTest2(args.size);
  });
  yield wait;

  // Copy HAR into the clipboard (asynchronous).
  let jsonString = yield RequestsMenu.contextMenu.copyAllAsHar();
  let har = JSON.parse(jsonString);

  // Check out the HAR log.
  isnot(har.log, null, "The HAR log must exist");
  is(har.log.pages.length, 1, "There must be one page");
  is(har.log.entries.length, 1, "There must be one request");

  let entry = har.log.entries[0];
  is(entry.request.postData.text, "x".repeat(size),
     "Check post data payload");

  const wasTwoSeconds = entry.timings.send >= 2000;
  if (actuallyThrottle) {
    ok(wasTwoSeconds, "upload should have taken more than 2 seconds");
  } else {
    ok(!wasTwoSeconds, "upload should not have taken more than 2 seconds");
  }

  // Clean up
  yield teardown(monitor);
}
