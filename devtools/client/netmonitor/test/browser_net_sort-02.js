/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if sorting columns in the network table works correctly.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SORTING_URL);
  info("Starting test... ");

  // It seems that this test may be slow on debug builds. This could be because
  // of the heavy dom manipulation associated with sorting.
  requestLongerTimeout(2);

  let { $, $all, L10N, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

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

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 5);
  yield performRequestsInContent(requests);
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" }, $("#details-pane-toggle"));

  isnot(RequestsMenu.selectedItem, null,
    "There should be a selected item in the requests menu.");
  is(RequestsMenu.selectedIndex, 0,
    "The first item should be selected in the requests menu.");
  is(NetMonitorView.detailsPaneHidden, false,
    "The details pane should not be hidden after toggle button was pressed.");

  testHeaders();
  testContents([0, 2, 4, 3, 1]);

  info("Testing status sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-status-button"));
  testHeaders("status", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing status sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-status-button"));
  testHeaders("status", "descending");
  testContents([4, 3, 2, 1, 0]);

  info("Testing status sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-status-button"));
  testHeaders("status", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing method sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-method-button"));
  testHeaders("method", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing method sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-method-button"));
  testHeaders("method", "descending");
  testContents([4, 3, 2, 1, 0]);

  info("Testing method sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-method-button"));
  testHeaders("method", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing file sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-file-button"));
  testHeaders("file", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing file sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-file-button"));
  testHeaders("file", "descending");
  testContents([4, 3, 2, 1, 0]);

  info("Testing file sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-file-button"));
  testHeaders("file", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing type sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-type-button"));
  testHeaders("type", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing type sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-type-button"));
  testHeaders("type", "descending");
  testContents([4, 3, 2, 1, 0]);

  info("Testing type sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-type-button"));
  testHeaders("type", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing transferred sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-transferred-button"));
  testHeaders("transferred", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing transferred sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-transferred-button"));
  testHeaders("transferred", "descending");
  testContents([4, 3, 2, 1, 0]);

  info("Testing transferred sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-transferred-button"));
  testHeaders("transferred", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing size sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-size-button"));
  testHeaders("size", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing size sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-size-button"));
  testHeaders("size", "descending");
  testContents([4, 3, 2, 1, 0]);

  info("Testing size sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-size-button"));
  testHeaders("size", "ascending");
  testContents([0, 1, 2, 3, 4]);

  info("Testing waterfall sort, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-waterfall-button"));
  testHeaders("waterfall", "ascending");
  testContents([0, 2, 4, 3, 1]);

  info("Testing waterfall sort, descending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-waterfall-button"));
  testHeaders("waterfall", "descending");
  testContents([4, 2, 0, 1, 3]);

  info("Testing waterfall sort, ascending. Checking sort loops correctly.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-waterfall-button"));
  testHeaders("waterfall", "ascending");
  testContents([0, 2, 4, 3, 1]);

  return teardown(monitor);

  function testHeaders(sortType, direction) {
    let doc = monitor.panelWin.document;
    let target = doc.querySelector("#requests-menu-" + sortType + "-button");
    let headers = doc.querySelectorAll(".requests-menu-header-button");

    for (let header of headers) {
      if (header != target) {
        is(header.hasAttribute("sorted"), false,
          "The " + header.id + " header should not have a 'sorted' attribute.");
        is(header.hasAttribute("tooltiptext"), false,
          "The " + header.id + " header should not have a 'tooltiptext' attribute.");
      } else {
        is(header.getAttribute("sorted"), direction,
          "The " + header.id + " header has an incorrect 'sorted' attribute.");
        is(header.getAttribute("tooltiptext"), direction == "ascending"
          ? L10N.getStr("networkMenu.sortedAsc")
          : L10N.getStr("networkMenu.sortedDesc"),
          "The " + header.id + " has an incorrect 'tooltiptext' attribute.");
      }
    }
  }

  function testContents([a, b, c, d, e]) {
    isnot(RequestsMenu.selectedItem, null,
      "There should still be a selected item after sorting.");
    is(RequestsMenu.selectedIndex, a,
      "The first item should be still selected after sorting.");
    is(NetMonitorView.detailsPaneHidden, false,
      "The details pane should still be visible after sorting.");

    is(RequestsMenu.items.length, 5,
      "There should be a total of 5 items in the requests menu.");
    is(RequestsMenu.visibleItems.length, 5,
      "There should be a total of 5 visbile items in the requests menu.");
    is($all(".side-menu-widget-item").length, 5,
      "The visible items in the requests menu are, in fact, visible!");

    is(RequestsMenu.getItemAtIndex(0), RequestsMenu.items[0],
      "The requests menu items aren't ordered correctly. First item is misplaced.");
    is(RequestsMenu.getItemAtIndex(1), RequestsMenu.items[1],
      "The requests menu items aren't ordered correctly. Second item is misplaced.");
    is(RequestsMenu.getItemAtIndex(2), RequestsMenu.items[2],
      "The requests menu items aren't ordered correctly. Third item is misplaced.");
    is(RequestsMenu.getItemAtIndex(3), RequestsMenu.items[3],
      "The requests menu items aren't ordered correctly. Fourth item is misplaced.");
    is(RequestsMenu.getItemAtIndex(4), RequestsMenu.items[4],
      "The requests menu items aren't ordered correctly. Fifth item is misplaced.");

    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(a),
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
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(b),
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
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(c),
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
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(d),
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
    verifyRequestItemTarget(RequestsMenu.getItemAtIndex(e),
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
});
