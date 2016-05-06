/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if sorting columns in the network table works correctly with new requests.
 */

function test() {
  initNetMonitor(SORTING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    // It seems that this test may be slow on debug builds. This could be because
    // of the heavy dom manipulation associated with sorting.
    requestLongerTimeout(2);

    let { $, $all, L10N, NetMonitorView } = aMonitor.panelWin;
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

    waitForNetworkEvents(aMonitor, 5).then(() => {
      EventUtils.sendMouseEvent({ type: "mousedown" }, $("#details-pane-toggle"));

      isnot(RequestsMenu.selectedItem, null,
        "There should be a selected item in the requests menu.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be selected in the requests menu.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should not be hidden after toggle button was pressed.");

      testHeaders();
      testContents([0, 2, 4, 3, 1], 0)
        .then(() => {
          info("Testing status sort, ascending.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-status-button"));
          testHeaders("status", "ascending");
          return testContents([0, 1, 2, 3, 4], 0);
        })
        .then(() => {
          info("Performing more requests.");
          performRequestsInContent(requests);
          return waitForNetworkEvents(aMonitor, 5);
        })
        .then(() => {
          info("Testing status sort again, ascending.");
          testHeaders("status", "ascending");
          return testContents([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 0);
        })
        .then(() => {
          info("Testing status sort, descending.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-status-button"));
          testHeaders("status", "descending");
          return testContents([9, 8, 7, 6, 5, 4, 3, 2, 1, 0], 9);
        })
        .then(() => {
          info("Performing more requests.");
          performRequestsInContent(requests);
          return waitForNetworkEvents(aMonitor, 5);
        })
        .then(() => {
          info("Testing status sort again, descending.");
          testHeaders("status", "descending");
          return testContents([14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0], 14);
        })
        .then(() => {
          info("Testing status sort yet again, ascending.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-status-button"));
          testHeaders("status", "ascending");
          return testContents([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14], 0);
        })
        .then(() => {
          info("Testing status sort yet again, descending.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-status-button"));
          testHeaders("status", "descending");
          return testContents([14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0], 14);
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish, e => {
          ok(false, e);
        });
    }, e => {
      ok(false, e);
    });

    function testHeaders(aSortType, aDirection) {
      let doc = aMonitor.panelWin.document;
      let target = doc.querySelector("#requests-menu-" + aSortType + "-button");
      let headers = doc.querySelectorAll(".requests-menu-header-button");

      for (let header of headers) {
        if (header != target) {
          is(header.hasAttribute("sorted"), false,
            "The " + header.id + " header should not have a 'sorted' attribute.");
          is(header.hasAttribute("tooltiptext"), false,
            "The " + header.id + " header should not have a 'tooltiptext' attribute.");
        } else {
          is(header.getAttribute("sorted"), aDirection,
            "The " + header.id + " header has an incorrect 'sorted' attribute.");
          is(header.getAttribute("tooltiptext"), aDirection == "ascending"
            ? L10N.getStr("networkMenu.sortedAsc")
            : L10N.getStr("networkMenu.sortedDesc"),
            "The " + header.id + " has an incorrect 'tooltiptext' attribute.");
        }
      }
    }

    function testContents(aOrder, aSelection) {
      isnot(RequestsMenu.selectedItem, null,
        "There should still be a selected item after sorting.");
      is(RequestsMenu.selectedIndex, aSelection,
        "The first item should be still selected after sorting.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should still be visible after sorting.");

      is(RequestsMenu.items.length, aOrder.length,
        "There should be a specific number of items in the requests menu.");
      is(RequestsMenu.visibleItems.length, aOrder.length,
        "There should be a specific number of visbile items in the requests menu.");
      is($all(".side-menu-widget-item").length, aOrder.length,
        "The visible items in the requests menu are, in fact, visible!");

      for (let i = 0; i < aOrder.length; i++) {
        is(RequestsMenu.getItemAtIndex(i), RequestsMenu.items[i],
          "The requests menu items aren't ordered correctly. Misplaced item " + i + ".");
      }

      for (let i = 0, len = aOrder.length / 5; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i]),
          "GET1", SORTING_SJS + "?index=1", {
            fuzzyUrl: true,
            status: 101,
            statusText: "Meh",
            type: "1",
            fullMimeType: "text/1",
            transferred: L10N.getStr("networkMenu.sizeUnavailable"),
            size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0),
            time: true
          });
      }
      for (let i = 0, len = aOrder.length / 5; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len]),
          "GET2", SORTING_SJS + "?index=2", {
            fuzzyUrl: true,
            status: 200,
            statusText: "Meh",
            type: "2",
            fullMimeType: "text/2",
            transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
            size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
            time: true
          });
      }
      for (let i = 0, len = aOrder.length / 5; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 2]),
          "GET3", SORTING_SJS + "?index=3", {
            fuzzyUrl: true,
            status: 300,
            statusText: "Meh",
            type: "3",
            fullMimeType: "text/3",
            transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.03),
            size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.03),
            time: true
          });
      }
      for (let i = 0, len = aOrder.length / 5; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 3]),
          "GET4", SORTING_SJS + "?index=4", {
            fuzzyUrl: true,
            status: 400,
            statusText: "Meh",
            type: "4",
            fullMimeType: "text/4",
            transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.04),
            size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.04),
            time: true
          });
      }
      for (let i = 0, len = aOrder.length / 5; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 4]),
          "GET5", SORTING_SJS + "?index=5", {
            fuzzyUrl: true,
            status: 500,
            statusText: "Meh",
            type: "5",
            fullMimeType: "text/5",
            transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.05),
            size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.05),
            time: true
          });
      }

      return promise.resolve(null);
    }

    performRequestsInContent(requests).then(null, e => console.error(e));
  });
}
