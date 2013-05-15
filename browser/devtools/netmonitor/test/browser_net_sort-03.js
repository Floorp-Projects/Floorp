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

    let { $, L10N, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

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
          aDebuggee.performRequests();
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
          aDebuggee.performRequests();
          return waitForNetworkEvents(aMonitor, 5);
        })
        .then(() => {
          info("Testing status sort again, descending.");
          testHeaders("status", "descending");
          return testContents([14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0], 12);
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish);
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
      let deferred = Promise.defer();

      isnot(RequestsMenu.selectedItem, null,
        "There should still be a selected item after sorting.");
      is(RequestsMenu.selectedIndex, aSelection,
        "The first item should be still selected after sorting.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should still be visible after sorting.");

      is(RequestsMenu.allItems.length, aOrder.length,
        "There should be a specific number of items in the requests menu.");
      is(RequestsMenu.visibleItems.length, aOrder.length,
        "There should be a specific number of visbile items in the requests menu.");

      for (let i = 0; i < aOrder.length; i++) {
        is(RequestsMenu.getItemAtIndex(i), RequestsMenu.allItems[i],
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
            size: L10N.getFormatStr("networkMenu.sizeKB", 0),
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
            size: L10N.getFormatStr("networkMenu.sizeKB", 0.01),
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
            size: L10N.getFormatStr("networkMenu.sizeKB", 0.02),
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
            size: L10N.getFormatStr("networkMenu.sizeKB", 0.03),
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
            size: L10N.getFormatStr("networkMenu.sizeKB", 0.04),
            time: true
          });
      }

      executeSoon(deferred.resolve);
      return deferred.promise;
    }

    aDebuggee.performRequests();
  });
}
