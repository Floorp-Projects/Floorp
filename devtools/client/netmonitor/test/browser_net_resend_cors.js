/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if resending a CORS request avoids the security checks and doesn't send
 * a preflight OPTIONS request (bug 1270096 and friends)
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CORS_URL);
  info("Starting test... ");

  const { store, windowRequire, connector } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getRequestById,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  const requestUrl = "http://test1.example.com" + CORS_SJS_PATH;

  info("Waiting for OPTIONS, then POST");
  const wait = waitForNetworkEvents(monitor, 2);
  await ContentTask.spawn(tab.linkedBrowser, requestUrl, async function(url) {
    content.wrappedJSObject.performRequests(url, "triggering/preflight", "post-data");
  });
  await wait;

  const METHODS = ["OPTIONS", "POST"];
  const ITEMS = METHODS.map((val, i) => getSortedRequests(store.getState()).get(i));

  // Check the requests that were sent
  ITEMS.forEach((item, i) => {
    is(item.method, METHODS[i], `The ${item.method} request has the right method`);
    is(item.url, requestUrl, `The ${item.method} request has the right URL`);
  });

  // Resend both requests without modification. Wait for resent OPTIONS, then POST.
  // POST is supposed to have no preflight OPTIONS request this time (CORS is disabled)
  const onRequests = waitForNetworkEvents(monitor, 1);
  for (let item of ITEMS) {
    info(`Selecting the ${item.method} request`);
    store.dispatch(Actions.selectRequest(item.id));

    // Wait for requestHeaders and responseHeaders are required when fetching data
    // from back-end.
    await waitUntil(() => {
      item = getRequestById(store.getState(), item.id);
      return item.requestHeaders && item.responseHeaders;
    });

    const { size } = getSortedRequests(store.getState());

    info("Cloning the selected request into a custom clone");
    store.dispatch(Actions.cloneSelectedRequest());

    info("Sending the cloned request (without change)");
    store.dispatch(Actions.sendCustomRequest(connector));

    await waitUntil(() => getSortedRequests(store.getState()).size === size + 1);
  }

  info("Waiting for both resent requests");
  await onRequests;

  // Check the resent requests
  for (let i = 0; i < ITEMS.length; i++) {
    let item = ITEMS[i];
    is(item.method, METHODS[i], `The ${item.method} request has the right method`);
    is(item.url, requestUrl, `The ${item.method} request has the right URL`);
    is(item.status, 200, `The ${item.method} response has the right status`);

    if (item.method === "POST") {
      is(item.method, "POST", `The ${item.method} request has the right method`);

      // Trigger responseContent update requires to wait until
      // responseContentAvailable set true
      await waitUntil(() => {
        item = getRequestById(store.getState(), item.id);
        return item.responseContentAvailable;
      });
      await connector.requestData(item.id, "responseContent");

      // Wait for both requestPostData & responseContent payloads arrived.
      await waitUntil(() => {
        item = getRequestById(store.getState(), item.id);
        return item.responseContent && item.requestPostData;
      });

      is(item.requestPostData.postData.text, "post-data",
        "The POST request has the right POST data");
      is(item.responseContent.content.text, "Access-Control-Allow-Origin: *",
        "The POST response has the right content");
    }
  }

  info("Finishing the test");
  return teardown(monitor);
});
