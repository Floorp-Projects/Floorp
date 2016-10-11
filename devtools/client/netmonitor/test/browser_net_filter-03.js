/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if filtering items in the network table works correctly with new requests
 * and while sorting is enabled.
 */
const BASIC_REQUESTS = [
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined" },
  { url: "sjs_content-type-test-server.sjs?fmt=css" },
  { url: "sjs_content-type-test-server.sjs?fmt=js" },
];

const REQUESTS_WITH_MEDIA = BASIC_REQUESTS.concat([
  { url: "sjs_content-type-test-server.sjs?fmt=font" },
  { url: "sjs_content-type-test-server.sjs?fmt=image" },
  { url: "sjs_content-type-test-server.sjs?fmt=audio" },
  { url: "sjs_content-type-test-server.sjs?fmt=video" },
]);

add_task(function* () {
  let { monitor } = yield initNetMonitor(FILTERING_URL);
  info("Starting test... ");

  // It seems that this test may be slow on Ubuntu builds running on ec2.
  requestLongerTimeout(2);

  let { $, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  // The test assumes that the first HTML request here has a longer response
  // body than the other HTML requests performed later during the test.
  let requests = Cu.cloneInto(REQUESTS_WITH_MEDIA, {});
  let newres = "res=<p>" + new Array(10).join(Math.random(10)) + "</p>";
  requests[0].url = requests[0].url.replace("res=undefined", newres);

  loadCommonFrameScript();

  let wait = waitForNetworkEvents(monitor, 7);
  yield performRequestsInContent(requests);
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" }, $("#details-pane-toggle"));

  isnot(RequestsMenu.selectedItem, null,
    "There should be a selected item in the requests menu.");
  is(RequestsMenu.selectedIndex, 0,
    "The first item should be selected in the requests menu.");
  is(NetMonitorView.detailsPaneHidden, false,
    "The details pane should not be hidden after toggle button was pressed.");

  testFilterButtons(monitor, "all");
  testContents([0, 1, 2, 3, 4, 5, 6], 7, 0);

  info("Sorting by size, ascending.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-size-button"));
  testFilterButtons(monitor, "all");
  testContents([6, 4, 5, 0, 1, 2, 3], 7, 6);

  info("Testing html filtering.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
  testFilterButtons(monitor, "html");
  testContents([6, 4, 5, 0, 1, 2, 3], 1, 6);

  info("Performing more requests.");
  wait = waitForNetworkEvents(monitor, 7);
  performRequestsInContent(REQUESTS_WITH_MEDIA);
  yield wait;

  info("Testing html filtering again.");
  resetSorting();
  testFilterButtons(monitor, "html");
  testContents([8, 13, 9, 11, 10, 12, 0, 4, 1, 5, 2, 6, 3, 7], 2, 13);

  info("Performing more requests.");
  performRequestsInContent(REQUESTS_WITH_MEDIA);
  yield waitForNetworkEvents(monitor, 7);

  info("Testing html filtering again.");
  resetSorting();
  testFilterButtons(monitor, "html");
  testContents([12, 13, 20, 14, 16, 18, 15, 17, 19, 0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11],
    3, 20);

  yield teardown(monitor);

  function resetSorting() {
    EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-waterfall-button"));
    EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-size-button"));
  }

  function testContents(order, visible, selection) {
    isnot(RequestsMenu.selectedItem, null,
      "There should still be a selected item after filtering.");
    is(RequestsMenu.selectedIndex, selection,
      "The first item should be still selected after filtering.");
    is(NetMonitorView.detailsPaneHidden, false,
      "The details pane should still be visible after filtering.");

    is(RequestsMenu.items.length, order.length,
      "There should be a specific amount of items in the requests menu.");
    is(RequestsMenu.visibleItems.length, visible,
      "There should be a specific amount of visible items in the requests menu.");
  }
});
