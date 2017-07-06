/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if sorting columns in the network table works correctly with new requests.
 */

add_task(function* () {
  let { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  let { monitor } = yield initNetMonitor(SORTING_URL);
  info("Starting test... ");

  // It seems that this test may be slow on debug builds. This could be because
  // of the heavy dom manipulation associated with sorting.
  requestLongerTimeout(2);

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSelectedRequest,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Loading the frame script and preparing the xhr request URLs so we can
  // generate some requests later.
  loadCommonFrameScript();
  let requests = [{
    url: "sjs_sorting-test-server.sjs?index=1&" + Math.random(),
    method: "GET1"
  }, {
    url: "sjs_sorting-test-server.sjs?index=5&" + Math.random(),
    method: "GET5"
  }, {
    url: "sjs_sorting-test-server.sjs?index=2&" + Math.random(),
    method: "GET2"
  }, {
    url: "sjs_sorting-test-server.sjs?index=4&" + Math.random(),
    method: "GET4"
  }, {
    url: "sjs_sorting-test-server.sjs?index=3&" + Math.random(),
    method: "GET3"
  }];

  let wait = waitForNetworkEvents(monitor, 5);
  yield performRequestsInContent(requests);
  yield wait;

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".network-details-panel-toggle"));

  isnot(getSelectedRequest(store.getState()), undefined,
    "There should be a selected item in the requests menu.");
  is(getSelectedIndex(store.getState()), 0,
    "The first item should be selected in the requests menu.");
  is(!!document.querySelector(".network-details-panel"), true,
    "The network details panel should be visible after toggle button was pressed.");

  testHeaders();
  testContents([0, 2, 4, 3, 1], 0);

  info("Testing status sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-status-button"));
  testHeaders("status", "ascending");
  testContents([0, 1, 2, 3, 4], 0);

  info("Performing more requests.");
  wait = waitForNetworkEvents(monitor, 5);
  yield performRequestsInContent(requests);
  yield wait;

  info("Testing status sort again, ascending.");
  testHeaders("status", "ascending");
  testContents([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 0);

  info("Testing status sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-status-button"));
  testHeaders("status", "descending");
  testContents([9, 8, 7, 6, 5, 4, 3, 2, 1, 0], 9);

  info("Performing more requests.");
  wait = waitForNetworkEvents(monitor, 5);
  yield performRequestsInContent(requests);
  yield wait;

  info("Testing status sort again, descending.");
  testHeaders("status", "descending");
  testContents([14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0], 14);

  info("Testing status sort yet again, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-status-button"));
  testHeaders("status", "ascending");
  testContents([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14], 0);

  info("Testing status sort yet again, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-status-button"));
  testHeaders("status", "descending");
  testContents([14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0], 14);

  return teardown(monitor);

  function testHeaders(sortType, direction) {
    let doc = monitor.panelWin.document;
    let target = doc.querySelector("#requests-list-" + sortType + "-button");
    let headers = doc.querySelectorAll(".requests-list-header-button");

    for (let header of headers) {
      if (header != target) {
        ok(!header.hasAttribute("data-sorted"),
          "The " + header.id + " header does not have a 'data-sorted' attribute.");
        ok(!header.getAttribute("title").includes(L10N.getStr("networkMenu.sortedAsc")) &&
          !header.getAttribute("title").includes(L10N.getStr("networkMenu.sortedDesc")),
          "The " + header.id +
          " header does not include any sorting in the 'title' attribute.");
      } else {
        is(header.getAttribute("data-sorted"), direction,
          "The " + header.id + " header has a correct 'data-sorted' attribute.");
        const sorted = direction == "ascending"
          ? L10N.getStr("networkMenu.sortedAsc")
          : L10N.getStr("networkMenu.sortedDesc");
        ok(header.getAttribute("title").includes(sorted),
          "The " + header.id +
          " header includes the used sorting in the 'title' attribute.");
      }
    }
  }

  function getSelectedIndex(state) {
    if (!state.requests.selectedId) {
      return -1;
    }
    return getSortedRequests(state).findIndex(r => r.id === state.requests.selectedId);
  }

  function testContents(order, selection) {
    isnot(getSelectedRequest(store.getState()), undefined,
      "There should still be a selected item after sorting.");
    is(getSelectedIndex(store.getState()), selection,
      "The first item should be still selected after sorting.");
    is(!!document.querySelector(".network-details-panel"), true,
      "The network details panel should still be visible after sorting.");

    is(getSortedRequests(store.getState()).length, order.length,
      "There should be a specific number of items in the requests menu.");
    is(getDisplayedRequests(store.getState()).length, order.length,
      "There should be a specific number of visbile items in the requests menu.");
    is(document.querySelectorAll(".request-list-item").length, order.length,
      "The visible items in the requests menu are, in fact, visible!");

    for (let i = 0, len = order.length / 5; i < len; i++) {
      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        getSortedRequests(store.getState()).get(order[i]),
        "GET1", SORTING_SJS + "?index=1", {
          fuzzyUrl: true,
          status: 101,
          statusText: "Meh",
          type: "1",
          fullMimeType: "text/1",
          transferred: L10N.getStr("networkMenu.sizeUnavailable"),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 0),
          time: true
        });
    }
    for (let i = 0, len = order.length / 5; i < len; i++) {
      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        getSortedRequests(store.getState()).get(order[i + len]),
        "GET2", SORTING_SJS + "?index=2", {
          fuzzyUrl: true,
          status: 200,
          statusText: "Meh",
          type: "2",
          fullMimeType: "text/2",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 19),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 19),
          time: true
        });
    }
    for (let i = 0, len = order.length / 5; i < len; i++) {
      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        getSortedRequests(store.getState()).get(order[i + len * 2]),
        "GET3", SORTING_SJS + "?index=3", {
          fuzzyUrl: true,
          status: 300,
          statusText: "Meh",
          type: "3",
          fullMimeType: "text/3",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 29),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 29),
          time: true
        });
    }
    for (let i = 0, len = order.length / 5; i < len; i++) {
      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        getSortedRequests(store.getState()).get(order[i + len * 3]),
        "GET4", SORTING_SJS + "?index=4", {
          fuzzyUrl: true,
          status: 400,
          statusText: "Meh",
          type: "4",
          fullMimeType: "text/4",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 39),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 39),
          time: true
        });
    }
    for (let i = 0, len = order.length / 5; i < len; i++) {
      verifyRequestItemTarget(
        document,
        getDisplayedRequests(store.getState()),
        getSortedRequests(store.getState()).get(order[i + len * 4]),
        "GET5", SORTING_SJS + "?index=5", {
          fuzzyUrl: true,
          status: 500,
          statusText: "Meh",
          type: "5",
          fullMimeType: "text/5",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 49),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 49),
          time: true
        });
    }
  }
});
