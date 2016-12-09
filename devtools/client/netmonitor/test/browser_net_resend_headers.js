/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if custom request headers are not ignored (bug 1270096 and friends)
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_SJS);
  info("Starting test... ");

  let { NetMonitorView, NetMonitorController } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let requestUrl = SIMPLE_SJS;
  let requestHeaders = [
    { name: "Host", value: "fakehost.example.com" },
    { name: "User-Agent", value: "Testzilla" },
    { name: "Referer", value: "http://example.com/referrer" },
    { name: "Accept", value: "application/jarda"},
    { name: "Accept-Encoding", value: "compress, identity, funcoding" },
    { name: "Accept-Language", value: "cs-CZ" }
  ];

  let wait = waitForNetworkEvents(monitor, 0, 1);
  NetMonitorController.webConsoleClient.sendHTTPRequest({
    url: requestUrl,
    method: "POST",
    headers: requestHeaders,
    body: "Hello"
  });
  yield wait;

  let { attachment } = RequestsMenu.getItemAtIndex(0);
  is(attachment.method, "POST", "The request has the right method");
  is(attachment.url, requestUrl, "The request has the right URL");

  for (let { name, value } of attachment.requestHeaders.headers) {
    info(`Request header: ${name}: ${value}`);
  }

  function hasRequestHeader(name, value) {
    let { headers } = attachment.requestHeaders;
    return headers.some(h => h.name === name && h.value === value);
  }

  function hasNotRequestHeader(name) {
    let { headers } = attachment.requestHeaders;
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
