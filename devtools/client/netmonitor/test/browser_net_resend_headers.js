/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if custom request headers are not ignored (bug 1270096 and friends)
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  let { store, windowRequire, connector } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { sendHTTPRequest } = connector;
  let {
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  let requestUrl = SIMPLE_SJS;
  let requestHeaders = [
    { name: "Host", value: "fakehost.example.com" },
    { name: "User-Agent", value: "Testzilla" },
    { name: "Referer", value: "http://example.com/referrer" },
    { name: "Accept", value: "application/jarda"},
    { name: "Accept-Encoding", value: "compress, identity, funcoding" },
    { name: "Accept-Language", value: "cs-CZ" }
  ];

  let wait = waitForNetworkEvents(monitor, 1);
  sendHTTPRequest({
    url: requestUrl,
    method: "POST",
    headers: requestHeaders,
    body: "Hello"
  });
  yield wait;

  let item = getSortedRequests(store.getState()).get(0);
  is(item.method, "POST", "The request has the right method");
  is(item.url, requestUrl, "The request has the right URL");

  for (let { name, value } of item.requestHeaders.headers) {
    info(`Request header: ${name}: ${value}`);
  }

  function hasRequestHeader(name, value) {
    let { headers } = item.requestHeaders;
    return headers.some(h => h.name === name && h.value === value);
  }

  function hasNotRequestHeader(name) {
    let { headers } = item.requestHeaders;
    return headers.every(h => h.name !== name);
  }

  for (let { name, value } of requestHeaders) {
    ok(hasRequestHeader(name, value), `The ${name} header has the right value`);
  }

  // Check that the Cookie header was not added silently (i.e., that the request is
  // anonymous.
  for (let name of ["Cookie"]) {
    ok(hasNotRequestHeader(name), `The ${name} header is not present`);
  }

  return teardown(monitor);
});
