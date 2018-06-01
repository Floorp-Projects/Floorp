/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests render correct information in the menu UI.
 */

function test() {
  // Disable tcp fast open, because it is setting a response header indicator
  // (bug 1352274). TCP Fast Open is not present on all platforms therefore the
  // number of response headers will vary depending on the platform.
  Services.prefs.setBoolPref("network.tcp.tcp_fastopen_enable", false);

  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  initNetMonitor(SIMPLE_SJS).then(async ({ tab, monitor }) => {
    info("Starting test... ");

    const { document, store, windowRequire, connector } = monitor.panelWin;
    const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
    const { EVENTS } = windowRequire("devtools/client/netmonitor/src/constants");
    const {
      getDisplayedRequests,
      getSelectedRequest,
      getSortedRequests,
    } = windowRequire("devtools/client/netmonitor/src/selectors/index");

    store.dispatch(Actions.batchEnable(false));

    const promiseList = [];
    promiseList.push(waitForNetworkEvents(monitor, 1));

    function expectEvent(evt, cb) {
      promiseList.push(new Promise((resolve, reject) => {
        monitor.panelWin.api.once(evt, _ => {
          cb().then(resolve, reject);
        });
      }));
    }

    expectEvent(EVENTS.NETWORK_EVENT, async () => {
      is(getSelectedRequest(store.getState()), null,
        "There shouldn't be any selected item in the requests menu.");
      is(store.getState().requests.requests.size, 1,
        "The requests menu should not be empty after the first request.");
      is(!!document.querySelector(".network-details-panel"), false,
        "The network details panel should still be hidden after first request.");

      const requestItem = getSortedRequests(store.getState()).get(0);

      is(typeof requestItem.id, "string",
        "The attached request id is incorrect.");
      isnot(requestItem.id, "",
        "The attached request id should not be empty.");

      is(typeof requestItem.startedMillis, "number",
        "The attached startedMillis is incorrect.");
      isnot(requestItem.startedMillis, 0,
        "The attached startedMillis should not be zero.");

      is(requestItem.requestHeaders, undefined,
        "The requestHeaders should not yet be set.");
      is(requestItem.requestCookies, undefined,
        "The requestCookies should not yet be set.");
      is(requestItem.requestPostData, undefined,
        "The requestPostData should not yet be set.");

      is(requestItem.responseHeaders, undefined,
        "The responseHeaders should not yet be set.");
      is(requestItem.responseCookies, undefined,
        "The responseCookies should not yet be set.");

      is(requestItem.httpVersion, undefined,
        "The httpVersion should not yet be set.");
      is(requestItem.status, undefined,
        "The status should not yet be set.");
      is(requestItem.statusText, undefined,
        "The statusText should not yet be set.");

      is(requestItem.headersSize, undefined,
        "The headersSize should not yet be set.");
      is(requestItem.transferredSize, undefined,
        "The transferredSize should not yet be set.");
      is(requestItem.contentSize, undefined,
        "The contentSize should not yet be set.");

      is(requestItem.responseContent, undefined,
        "The responseContent should not yet be set.");

      is(requestItem.totalTime, undefined,
        "The totalTime should not yet be set.");
      is(requestItem.eventTimings, undefined,
        "The eventTimings should not yet be set.");

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS
      );
    });

    expectEvent(EVENTS.RECEIVED_REQUEST_HEADERS, async () => {
      await waitForRequestData(store, ["requestHeaders"]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      ok(requestItem.requestHeaders,
        "There should be a requestHeaders data available.");
      is(requestItem.requestHeaders.headers.length, 10,
        "The requestHeaders data has an incorrect |headers| property.");
      isnot(requestItem.requestHeaders.headersSize, 0,
        "The requestHeaders data has an incorrect |headersSize| property.");
      // Can't test for the exact request headers size because the value may
      // vary across platforms ("User-Agent" header differs).

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS
      );
    });

    expectEvent(EVENTS.RECEIVED_REQUEST_COOKIES, async () => {
      await waitForRequestData(store, ["requestCookies"]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      ok(requestItem.requestCookies,
        "There should be a requestCookies data available.");
      is(requestItem.requestCookies.length, 2,
        "The requestCookies data has an incorrect |cookies| property.");

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS
      );
    });

    monitor.panelWin.api.once(EVENTS.RECEIVED_REQUEST_POST_DATA, () => {
      ok(false, "Trap listener: this request doesn't have any post data.");
    });

    expectEvent(EVENTS.RECEIVED_RESPONSE_HEADERS, async () => {
      await waitForRequestData(store, ["responseHeaders"]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      ok(requestItem.responseHeaders,
        "There should be a responseHeaders data available.");
      is(requestItem.responseHeaders.headers.length, 13,
        "The responseHeaders data has an incorrect |headers| property.");
      is(requestItem.responseHeaders.headersSize, 335,
        "The responseHeaders data has an incorrect |headersSize| property.");

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS
      );
    });

    expectEvent(EVENTS.RECEIVED_RESPONSE_COOKIES, async () => {
      await waitForRequestData(store, ["responseCookies"]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      ok(requestItem.responseCookies,
        "There should be a responseCookies data available.");
      is(requestItem.responseCookies.length, 2,
        "The responseCookies data has an incorrect |cookies| property.");

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS
      );
    });

    expectEvent(EVENTS.STARTED_RECEIVING_RESPONSE, async () => {
      await waitForRequestData(store, [
        "httpVersion",
        "status",
        "statusText",
        "headersSize"
      ]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      is(requestItem.httpVersion, "HTTP/1.1",
        "The httpVersion data has an incorrect value.");
      is(requestItem.status, "200",
        "The status data has an incorrect value.");
      is(requestItem.statusText, "Och Aye",
        "The statusText data has an incorrect value.");
      is(requestItem.headersSize, 335,
        "The headersSize data has an incorrect value.");

      const requestListItem = document.querySelector(".request-list-item");
      requestListItem.scrollIntoView();
      const requestsListStatus = requestListItem.querySelector(".status-code");
      EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
      await waitUntil(() => requestsListStatus.title);

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS,
        {
          status: "200",
          statusText: "Och Aye"
        }
      );
    });

    expectEvent(EVENTS.PAYLOAD_READY, async () => {
      await waitForRequestData(store, [
        "transferredSize",
        "contentSize",
        "mimeType"
      ]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      is(requestItem.transferredSize, "347",
        "The transferredSize data has an incorrect value.");
      is(requestItem.contentSize, "12",
        "The contentSize data has an incorrect value.");
      is(requestItem.mimeType, "text/plain; charset=utf-8",
        "The mimeType data has an incorrect value.");

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS,
        {
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 347),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 12),
        }
      );
    });

    expectEvent(EVENTS.UPDATING_EVENT_TIMINGS, async () => {
      await waitForRequestData(store, ["eventTimings"]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      is(typeof requestItem.totalTime, "number",
        "The attached totalTime is incorrect.");
      ok(requestItem.totalTime >= 0,
        "The attached totalTime should be positive.");

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS,
        {
          time: true
        }
      );
    });

    expectEvent(EVENTS.RECEIVED_EVENT_TIMINGS, async () => {
      await waitForRequestData(store, ["eventTimings"]);

      const requestItem = getSortedRequests(store.getState()).get(0);

      ok(requestItem.eventTimings,
        "There should be a eventTimings data available.");
      is(typeof requestItem.eventTimings.timings.blocked, "number",
        "The eventTimings data has an incorrect |timings.blocked| property.");
      is(typeof requestItem.eventTimings.timings.dns, "number",
        "The eventTimings data has an incorrect |timings.dns| property.");
      is(typeof requestItem.eventTimings.timings.ssl, "number",
        "The eventTimings data has an incorrect |timings.ssl| property.");
      is(typeof requestItem.eventTimings.timings.connect, "number",
        "The eventTimings data has an incorrect |timings.connect| property.");
      is(typeof requestItem.eventTimings.timings.send, "number",
        "The eventTimings data has an incorrect |timings.send| property.");
      is(typeof requestItem.eventTimings.timings.wait, "number",
        "The eventTimings data has an incorrect |timings.wait| property.");
      is(typeof requestItem.eventTimings.timings.receive, "number",
        "The eventTimings data has an incorrect |timings.receive| property.");
      is(typeof requestItem.eventTimings.totalTime, "number",
        "The eventTimings data has an incorrect |totalTime| property.");

      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        requestItem,
        "GET",
        SIMPLE_SJS,
        {
          time: true
        }
      );
    });

    const wait = waitForNetworkEvents(monitor, 1);
    tab.linkedBrowser.reload();
    await wait;

    const requestItem = getSortedRequests(store.getState()).get(0);

    if (!requestItem.requestHeaders) {
      connector.requestData(requestItem.id, "requestHeaders");
    }
    if (!requestItem.responseHeaders) {
      connector.requestData(requestItem.id, "responseHeaders");
    }

    await Promise.all(promiseList);
    await teardown(monitor);
    finish();
  });
}
