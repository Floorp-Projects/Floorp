/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if sorting columns in the network table works correctly.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { monitor } = await initNetMonitor(SORTING_URL);
  info("Starting test... ");

  // It seems that this test may be slow on debug builds. This could be because
  // of the heavy dom manipulation associated with sorting.
  requestLongerTimeout(2);

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const {
    getDisplayedRequests,
    getSelectedRequest,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Loading the frame script and preparing the xhr request URLs so we can
  // generate some requests later.
  loadFrameScriptUtils();
  const requests = [{
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

  const wait = waitForNetworkEvents(monitor, 5);
  await performRequestsInContent(requests);
  await wait;

  store.dispatch(Actions.toggleNetworkDetails());

  isnot(getSelectedRequest(store.getState()), undefined,
    "There should be a selected item in the requests menu.");
  is(getSelectedIndex(store.getState()), 0,
    "The first item should be selected in the requests menu.");
  is(!!document.querySelector(".network-details-panel"), true,
    "The network details panel should be visible after toggle button was pressed.");

  testHeaders();
  await testContents([0, 2, 4, 3, 1]);

  info("Testing status sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-status-button"));
  testHeaders("status", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing status sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-status-button"));
  testHeaders("status", "descending");
  await testContents([4, 3, 2, 1, 0]);

  info("Testing status sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-status-button"));
  testHeaders("status", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing method sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-method-button"));
  testHeaders("method", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing method sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-method-button"));
  testHeaders("method", "descending");
  await testContents([4, 3, 2, 1, 0]);

  info("Testing method sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-method-button"));
  testHeaders("method", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing file sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-file-button"));
  testHeaders("file", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing file sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-file-button"));
  testHeaders("file", "descending");
  await testContents([4, 3, 2, 1, 0]);

  info("Testing file sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-file-button"));
  testHeaders("file", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing type sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-type-button"));
  testHeaders("type", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing type sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-type-button"));
  testHeaders("type", "descending");
  await testContents([4, 3, 2, 1, 0]);

  info("Testing type sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-type-button"));
  testHeaders("type", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing transferred sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-transferred-button"));
  testHeaders("transferred", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing transferred sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-transferred-button"));
  testHeaders("transferred", "descending");
  await testContents([4, 3, 2, 1, 0]);

  info("Testing transferred sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-transferred-button"));
  testHeaders("transferred", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing size sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-contentSize-button"));
  testHeaders("contentSize", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing size sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-contentSize-button"));
  testHeaders("contentSize", "descending");
  await testContents([4, 3, 2, 1, 0]);

  info("Testing size sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-contentSize-button"));
  testHeaders("contentSize", "ascending");
  await testContents([0, 1, 2, 3, 4]);

  info("Testing waterfall sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-waterfall-button"));
  testHeaders("waterfall", "ascending");
  await testContents([0, 2, 4, 3, 1]);

  info("Testing waterfall sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-waterfall-button"));
  testHeaders("waterfall", "descending");
  await testContents([4, 2, 0, 1, 3]);

  info("Testing waterfall sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-waterfall-button"));
  testHeaders("waterfall", "ascending");
  await testContents([0, 2, 4, 3, 1]);

  return teardown(monitor);

  function getSelectedIndex(state) {
    if (!state.requests.selectedId) {
      return -1;
    }
    return getSortedRequests(state).findIndex(r => r.id === state.requests.selectedId);
  }

  function testHeaders(sortType, direction) {
    const doc = monitor.panelWin.document;
    const target = doc.querySelector("#requests-list-" + sortType + "-button");
    const headers = doc.querySelectorAll(".requests-list-header-button");

    for (const header of headers) {
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

  async function testContents([a, b, c, d, e]) {
    isnot(getSelectedRequest(store.getState()), undefined,
      "There should still be a selected item after sorting.");
    is(getSelectedIndex(store.getState()), a,
      "The first item should be still selected after sorting.");
    is(!!document.querySelector(".network-details-panel"), true,
      "The network details panel should still be visible after sorting.");

    is(getSortedRequests(store.getState()).length, 5,
      "There should be a total of 5 items in the requests menu.");
    is(getDisplayedRequests(store.getState()).length, 5,
      "There should be a total of 5 visible items in the requests menu.");
    is(document.querySelectorAll(".request-list-item").length, 5,
      "The visible items in the requests menu are, in fact, visible!");

    const requestItems = document.querySelectorAll(".request-list-item");
    for (const requestItem of requestItems) {
      requestItem.scrollIntoView();
      const requestsListStatus = requestItem.querySelector(".status-code");
      EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
      await waitUntil(() => requestsListStatus.title);
    }

    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(a),
      "GET1", SORTING_SJS + "?index=1", {
        fuzzyUrl: true,
        status: 101,
        statusText: "Meh",
        type: "1",
        fullMimeType: "text/1",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 198),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 0),
        time: true
      });
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(b),
      "GET2", SORTING_SJS + "?index=2", {
        fuzzyUrl: true,
        status: 200,
        statusText: "Meh",
        type: "2",
        fullMimeType: "text/2",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 217),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 19),
        time: true
      });
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(c),
      "GET3", SORTING_SJS + "?index=3", {
        fuzzyUrl: true,
        status: 300,
        statusText: "Meh",
        type: "3",
        fullMimeType: "text/3",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 227),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 29),
        time: true
      });
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(d),
      "GET4", SORTING_SJS + "?index=4", {
        fuzzyUrl: true,
        status: 400,
        statusText: "Meh",
        type: "4",
        fullMimeType: "text/4",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 237),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 39),
        time: true
      });
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState()).get(e),
      "GET5", SORTING_SJS + "?index=5", {
        fuzzyUrl: true,
        status: 500,
        statusText: "Meh",
        type: "5",
        fullMimeType: "text/5",
        transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 247),
        size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 49),
        time: true
      });
  }
});
