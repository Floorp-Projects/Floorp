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

  let { EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let requestUrl = "http://test1.example.com" + CORS_SJS_PATH;

  info("Waiting for OPTIONS, then POST");
  let wait = waitForNetworkEvents(monitor, 1, 1);
  yield ContentTask.spawn(tab.linkedBrowser, requestUrl, function* (url) {
    content.wrappedJSObject.performRequests(url, "triggering/preflight", "post-data");
  });
  yield wait;

  const METHODS = ["OPTIONS", "POST"];

  // Check the requests that were sent
  for (let [i, method] of METHODS.entries()) {
    let item = RequestsMenu.getItemAtIndex(i);
    is(item.method, method, `The ${method} request has the right method`);
    is(item.url, requestUrl, `The ${method} request has the right URL`);
  }

  // Resend both requests without modification. Wait for resent OPTIONS, then POST.
  // POST is supposed to have no preflight OPTIONS request this time (CORS is disabled)
  let onRequests = waitForNetworkEvents(monitor, 1, 1);
  for (let [i, method] of METHODS.entries()) {
    let item = RequestsMenu.getItemAtIndex(i);

    info(`Selecting the ${method} request (at index ${i})`);
    let onUpdate = monitor.panelWin.once(EVENTS.TAB_UPDATED);
    RequestsMenu.selectedItem = item;
    yield onUpdate;

    info("Cloning the selected request into a custom clone");
    let onPopulate = monitor.panelWin.once(EVENTS.CUSTOMREQUESTVIEW_POPULATED);
    RequestsMenu.cloneSelectedRequest();
    yield onPopulate;

    info("Sending the cloned request (without change)");
    RequestsMenu.sendCustomRequest();
  }

  info("Waiting for both resent requests");
  yield onRequests;

  // Check the resent requests
  for (let [i, method] of METHODS.entries()) {
    let index = i + 2;
    let item = RequestsMenu.getItemAtIndex(index);
    is(item.method, method, `The ${method} request has the right method`);
    is(item.url, requestUrl, `The ${method} request has the right URL`);
    is(item.status, 200, `The ${method} response has the right status`);

    if (method === "POST") {
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
