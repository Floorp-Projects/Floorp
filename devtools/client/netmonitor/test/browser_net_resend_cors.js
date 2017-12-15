/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if resending a CORS request avoids the security checks and doesn't send
 * a preflight OPTIONS request (bug 1270096 and friends)
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CORS_URL);
  info("Starting test... ");

  let { store, windowRequire, connector } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getRequestById,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let requestUrl = "http://test1.example.com" + CORS_SJS_PATH;

  info("Waiting for OPTIONS, then POST");
  let wait = waitForNetworkEvents(monitor, 2);
  yield ContentTask.spawn(tab.linkedBrowser, requestUrl, function* (url) {
    content.wrappedJSObject.performRequests(url, "triggering/preflight", "post-data");
  });
  yield wait;

  const METHODS = ["OPTIONS", "POST"];
  const ITEMS = METHODS.map((val, i) => getSortedRequests(store.getState()).get(i));

  // Check the requests that were sent
  ITEMS.forEach((item, i) => {
    is(item.method, METHODS[i], `The ${item.method} request has the right method`);
    is(item.url, requestUrl, `The ${item.method} request has the right URL`);
  });

  // Resend both requests without modification. Wait for resent OPTIONS, then POST.
  // POST is supposed to have no preflight OPTIONS request this time (CORS is disabled)
  let onRequests = waitForNetworkEvents(monitor, 1);
  for (let item of ITEMS) {
    info(`Selecting the ${item.method} request`);
    store.dispatch(Actions.selectRequest(item.id));

    // Wait for requestHeaders and responseHeaders are required when fetching data
    // from back-end.
    yield waitUntil(() => {
      item = getRequestById(store.getState(), item.id);
      return item.requestHeaders && item.responseHeaders;
    });

    let { size } = getSortedRequests(store.getState());

    info("Cloning the selected request into a custom clone");
    store.dispatch(Actions.cloneSelectedRequest());

    info("Sending the cloned request (without change)");
    store.dispatch(Actions.sendCustomRequest(connector));

    yield waitUntil(() => getSortedRequests(store.getState()).size === size + 1);
  }

  info("Waiting for both resent requests");
  yield onRequests;

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
      yield waitUntil(() => {
        item = getRequestById(store.getState(), item.id);
        return item.responseContentAvailable;
      });
      yield connector.requestData(item.id, "responseContent");

      // Wait for both requestPostData & responseContent payloads arrived.
      yield waitUntil(() => {
        item = getRequestById(store.getState(), item.id);
        return item.responseContent && item.requestPostData;
      });

      is(item.requestPostData.postData.text, "post-data",
        "The POST request has the right POST data");
      // eslint-disable-next-line mozilla/no-cpows-in-tests
      is(item.responseContent.content.text, "Access-Control-Allow-Origin: *",
        "The POST response has the right content");
    }
  }

  info("Finishing the test");
  return teardown(monitor);
});
