/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if cached requests have the correct status code
 */

let test = Task.async(function*() {
  let [tab, debuggee, monitor] = yield initNetMonitor(STATUS_CODES_URL, null, true);
  info("Starting test... ");

  let { document, L10N, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;
  NetworkDetails._params.lazyEmpty = false;

  const REQUEST_DATA = [
    {
      method: 'GET',
      uri: STATUS_CODES_SJS + "?sts=ok&cached",
      details: {
        status: 200,
        statusText: 'OK',
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8"
      }
    },
    {
      method: 'GET',
      uri: STATUS_CODES_SJS + "?sts=redirect&cached",
      details: {
        status: 301,
        statusText: 'Moved Permanently',
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    },
    {
      method: 'GET',
      uri: 'http://example.com/redirected',
      details: {
        status: 404,
        statusText: 'Not Found',
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    },
    {
      method: 'GET',
      uri: STATUS_CODES_SJS + "?sts=ok&cached",
      details: {
        status: 200,
        statusText: "OK (cached)",
        fromCache: true,
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8"
      }
    },
    {
      method: 'GET',
      uri: STATUS_CODES_SJS + "?sts=redirect&cached",
      details: {
        status: 301,
        statusText: "Moved Permanently (cached)",
        fromCache: true,
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    },
    {
      method: 'GET',
      uri: 'http://example.com/redirected',
      details: {
        status: 404,
        statusText: 'Not Found',
        type: "html",
        fullMimeType: "text/html; charset=utf-8"
      }
    }
  ];

  info("Performing requests #1...");
  debuggee.performCachedRequests();
  yield waitForNetworkEvents(monitor, 3);

  info("Performing requests #2...");
  debuggee.performCachedRequests();
  yield waitForNetworkEvents(monitor, 3);

  let index = 0;
  for (let request of REQUEST_DATA) {
    let item = RequestsMenu.getItemAtIndex(index);

    info("Verifying request #" + index);
    yield verifyRequestItemTarget(item, request.method, request.uri, request.details);

    index++;
  }

  yield teardown(monitor);
  finish();
});
