/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that CORS preflight requests are displayed by network monitor
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(HTTPS_CORS_URL, {
    requestCount: 1,
  });

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 2);

  info("Performing a CORS request");
  const requestUrl = "https://test1.example.com" + CORS_SJS_PATH;
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [requestUrl],
    async function (url) {
      content.wrappedJSObject.performRequests(
        url,
        "triggering/preflight",
        "post-data"
      );
    }
  );

  info("Waiting until the requests appear in netmonitor");
  await wait;

  info("Checking the preflight and flight methods");
  ["OPTIONS", "POST"].forEach((method, index) => {
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState())[index],
      method,
      requestUrl
    );
  });

  await teardown(monitor);
});
