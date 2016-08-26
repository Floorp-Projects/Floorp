/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if requests render correct information in the menu UI.
 */

function test() {
  initNetMonitor(SIMPLE_SJS).then(({ tab, monitor }) => {
    info("Starting test... ");

    let { L10N, NetMonitorView } = monitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(monitor, 1)
      .then(() => teardown(monitor))
      .then(finish);

    monitor.panelWin.once(monitor.panelWin.EVENTS.NETWORK_EVENT, () => {
      is(RequestsMenu.selectedItem, null,
        "There shouldn't be any selected item in the requests menu.");
      is(RequestsMenu.itemCount, 1,
        "The requests menu should not be empty after the first request.");
      is(NetMonitorView.detailsPaneHidden, true,
        "The details pane should still be hidden after the first request.");

      let requestItem = RequestsMenu.getItemAtIndex(0);

      is(typeof requestItem.value, "string",
        "The attached request id is incorrect.");
      isnot(requestItem.value, "",
        "The attached request id should not be empty.");

      is(typeof requestItem.attachment.startedDeltaMillis, "number",
        "The attached startedDeltaMillis is incorrect.");
      is(requestItem.attachment.startedDeltaMillis, 0,
        "The attached startedDeltaMillis should be zero.");

      is(typeof requestItem.attachment.startedMillis, "number",
        "The attached startedMillis is incorrect.");
      isnot(requestItem.attachment.startedMillis, 0,
        "The attached startedMillis should not be zero.");

      is(requestItem.attachment.requestHeaders, undefined,
        "The requestHeaders should not yet be set.");
      is(requestItem.attachment.requestCookies, undefined,
        "The requestCookies should not yet be set.");
      is(requestItem.attachment.requestPostData, undefined,
        "The requestPostData should not yet be set.");

      is(requestItem.attachment.responseHeaders, undefined,
        "The responseHeaders should not yet be set.");
      is(requestItem.attachment.responseCookies, undefined,
        "The responseCookies should not yet be set.");

      is(requestItem.attachment.httpVersion, undefined,
        "The httpVersion should not yet be set.");
      is(requestItem.attachment.status, undefined,
        "The status should not yet be set.");
      is(requestItem.attachment.statusText, undefined,
        "The statusText should not yet be set.");

      is(requestItem.attachment.headersSize, undefined,
        "The headersSize should not yet be set.");
      is(requestItem.attachment.transferredSize, undefined,
        "The transferredSize should not yet be set.");
      is(requestItem.attachment.contentSize, undefined,
        "The contentSize should not yet be set.");

      is(requestItem.attachment.mimeType, undefined,
        "The mimeType should not yet be set.");
      is(requestItem.attachment.responseContent, undefined,
        "The responseContent should not yet be set.");

      is(requestItem.attachment.totalTime, undefined,
        "The totalTime should not yet be set.");
      is(requestItem.attachment.eventTimings, undefined,
        "The eventTimings should not yet be set.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS);
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_REQUEST_HEADERS, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      ok(requestItem.attachment.requestHeaders,
        "There should be a requestHeaders attachment available.");
      is(requestItem.attachment.requestHeaders.headers.length, 9,
        "The requestHeaders attachment has an incorrect |headers| property.");
      isnot(requestItem.attachment.requestHeaders.headersSize, 0,
        "The requestHeaders attachment has an incorrect |headersSize| property.");
      // Can't test for the exact request headers size because the value may
      // vary across platforms ("User-Agent" header differs).

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS);
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_REQUEST_COOKIES, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      ok(requestItem.attachment.requestCookies,
        "There should be a requestCookies attachment available.");
      is(requestItem.attachment.requestCookies.cookies.length, 2,
        "The requestCookies attachment has an incorrect |cookies| property.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS);
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_REQUEST_POST_DATA, () => {
      ok(false, "Trap listener: this request doesn't have any post data.");
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_RESPONSE_HEADERS, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      ok(requestItem.attachment.responseHeaders,
        "There should be a responseHeaders attachment available.");
      is(requestItem.attachment.responseHeaders.headers.length, 10,
        "The responseHeaders attachment has an incorrect |headers| property.");
      is(requestItem.attachment.responseHeaders.headersSize, 330,
        "The responseHeaders attachment has an incorrect |headersSize| property.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS);
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_RESPONSE_COOKIES, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      ok(requestItem.attachment.responseCookies,
        "There should be a responseCookies attachment available.");
      is(requestItem.attachment.responseCookies.cookies.length, 2,
        "The responseCookies attachment has an incorrect |cookies| property.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS);
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.STARTED_RECEIVING_RESPONSE, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      is(requestItem.attachment.httpVersion, "HTTP/1.1",
        "The httpVersion attachment has an incorrect value.");
      is(requestItem.attachment.status, "200",
        "The status attachment has an incorrect value.");
      is(requestItem.attachment.statusText, "Och Aye",
        "The statusText attachment has an incorrect value.");
      is(requestItem.attachment.headersSize, 330,
        "The headersSize attachment has an incorrect value.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS, {
        status: "200",
        statusText: "Och Aye"
      });
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.UPDATING_RESPONSE_CONTENT, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      is(requestItem.attachment.transferredSize, "12",
        "The transferredSize attachment has an incorrect value.");
      is(requestItem.attachment.contentSize, "12",
        "The contentSize attachment has an incorrect value.");
      is(requestItem.attachment.mimeType, "text/plain; charset=utf-8",
        "The mimeType attachment has an incorrect value.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS, {
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.01),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.01),
      });
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_RESPONSE_CONTENT, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      ok(requestItem.attachment.responseContent,
        "There should be a responseContent attachment available.");
      is(requestItem.attachment.responseContent.content.mimeType,
        "text/plain; charset=utf-8",
        "The responseContent attachment has an incorrect |content.mimeType| property.");
      is(requestItem.attachment.responseContent.content.text,
        "Hello world!",
        "The responseContent attachment has an incorrect |content.text| property.");
      is(requestItem.attachment.responseContent.content.size,
        12,
        "The responseContent attachment has an incorrect |content.size| property.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS, {
        type: "plain",
        fullMimeType: "text/plain; charset=utf-8",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.01),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.01),
      });
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.UPDATING_EVENT_TIMINGS, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      is(typeof requestItem.attachment.totalTime, "number",
        "The attached totalTime is incorrect.");
      ok(requestItem.attachment.totalTime >= 0,
        "The attached totalTime should be positive.");

      is(typeof requestItem.attachment.endedMillis, "number",
        "The attached endedMillis is incorrect.");
      ok(requestItem.attachment.endedMillis >= 0,
        "The attached endedMillis should be positive.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS, {
        time: true
      });
    });

    monitor.panelWin.once(monitor.panelWin.EVENTS.RECEIVED_EVENT_TIMINGS, () => {
      let requestItem = RequestsMenu.getItemAtIndex(0);

      ok(requestItem.attachment.eventTimings,
        "There should be a eventTimings attachment available.");
      is(typeof requestItem.attachment.eventTimings.timings.blocked, "number",
        "The eventTimings attachment has an incorrect |timings.blocked| property.");
      is(typeof requestItem.attachment.eventTimings.timings.dns, "number",
        "The eventTimings attachment has an incorrect |timings.dns| property.");
      is(typeof requestItem.attachment.eventTimings.timings.connect, "number",
        "The eventTimings attachment has an incorrect |timings.connect| property.");
      is(typeof requestItem.attachment.eventTimings.timings.send, "number",
        "The eventTimings attachment has an incorrect |timings.send| property.");
      is(typeof requestItem.attachment.eventTimings.timings.wait, "number",
        "The eventTimings attachment has an incorrect |timings.wait| property.");
      is(typeof requestItem.attachment.eventTimings.timings.receive, "number",
        "The eventTimings attachment has an incorrect |timings.receive| property.");
      is(typeof requestItem.attachment.eventTimings.totalTime, "number",
        "The eventTimings attachment has an incorrect |totalTime| property.");

      verifyRequestItemTarget(requestItem, "GET", SIMPLE_SJS, {
        time: true
      });
    });

    tab.linkedBrowser.reload();
  });
}
