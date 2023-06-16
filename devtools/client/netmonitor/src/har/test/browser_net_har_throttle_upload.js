/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test timing of upload when throttling.

"use strict";

add_task(async function () {
  await throttleUploadTest(true);
  await throttleUploadTest(false);
});

async function throttleUploadTest(actuallyThrottle) {
  const { tab, monitor } = await initNetMonitor(
    HAR_EXAMPLE_URL + "html_har_post-data-test-page.html",
    { requestCount: 1 }
  );

  info("Starting test... (actuallyThrottle = " + actuallyThrottle + ")");

  const { connector, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  const size = 4096;
  const uploadSize = actuallyThrottle ? size / 3 : 0;

  const throttleProfile = {
    latency: 0,
    download: 200000,
    upload: uploadSize,
  };

  info("sending throttle request");
  await connector.updateNetworkThrottling(true, throttleProfile);

  // Execute one POST request on the page and wait till its done.
  const wait = waitForNetworkEvents(monitor, 1);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ size }],
    async function (args) {
      content.wrappedJSObject.executeTest2(args.size);
    }
  );
  await wait;

  // Copy HAR into the clipboard (asynchronous).
  const har = await copyAllAsHARWithContextMenu(monitor);

  // Check out the HAR log.
  isnot(har.log, null, "The HAR log must exist");
  is(har.log.pages.length, 1, "There must be one page");
  is(har.log.entries.length, 1, "There must be one request");

  const entry = har.log.entries[0];
  is(entry.request.postData.text, "x".repeat(size), "Check post data payload");

  const wasTwoSeconds = entry.timings.send >= 2000;
  if (actuallyThrottle) {
    ok(wasTwoSeconds, "upload should have taken more than 2 seconds");
  } else {
    ok(!wasTwoSeconds, "upload should not have taken more than 2 seconds");
  }

  // Clean up
  await teardown(monitor);
}
