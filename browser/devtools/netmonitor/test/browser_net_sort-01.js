/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the sorting mechanism works correctly.
 */

function test() {
  initNetMonitor(STATUS_CODES_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $all, L10N, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 5).then(() => {
      testContents([0, 1, 2, 3, 4])
        .then(() => {
          info("Testing swap(0, 0)");
          RequestsMenu.swapItemsAtIndices(0, 0);
          RequestsMenu.refreshZebra();
          return testContents([0, 1, 2, 3, 4]);
        })
        .then(() => {
          info("Testing swap(0, 1)");
          RequestsMenu.swapItemsAtIndices(0, 1);
          RequestsMenu.refreshZebra();
          return testContents([1, 0, 2, 3, 4]);
        })
        .then(() => {
          info("Testing swap(0, 2)");
          RequestsMenu.swapItemsAtIndices(0, 2);
          RequestsMenu.refreshZebra();
          return testContents([1, 2, 0, 3, 4]);
        })
        .then(() => {
          info("Testing swap(0, 3)");
          RequestsMenu.swapItemsAtIndices(0, 3);
          RequestsMenu.refreshZebra();
          return testContents([1, 2, 3, 0, 4]);
        })
        .then(() => {
          info("Testing swap(0, 4)");
          RequestsMenu.swapItemsAtIndices(0, 4);
          RequestsMenu.refreshZebra();
          return testContents([1, 2, 3, 4, 0]);
        })
        .then(() => {
          info("Testing swap(1, 0)");
          RequestsMenu.swapItemsAtIndices(1, 0);
          RequestsMenu.refreshZebra();
          return testContents([0, 2, 3, 4, 1]);
        })
        .then(() => {
          info("Testing swap(1, 1)");
          RequestsMenu.swapItemsAtIndices(1, 1);
          RequestsMenu.refreshZebra();
          return testContents([0, 2, 3, 4, 1]);
        })
        .then(() => {
          info("Testing swap(1, 2)");
          RequestsMenu.swapItemsAtIndices(1, 2);
          RequestsMenu.refreshZebra();
          return testContents([0, 1, 3, 4, 2]);
        })
        .then(() => {
          info("Testing swap(1, 3)");
          RequestsMenu.swapItemsAtIndices(1, 3);
          RequestsMenu.refreshZebra();
          return testContents([0, 3, 1, 4, 2]);
        })
        .then(() => {
          info("Testing swap(1, 4)");
          RequestsMenu.swapItemsAtIndices(1, 4);
          RequestsMenu.refreshZebra();
          return testContents([0, 3, 4, 1, 2]);
        })
        .then(() => {
          info("Testing swap(2, 0)");
          RequestsMenu.swapItemsAtIndices(2, 0);
          RequestsMenu.refreshZebra();
          return testContents([2, 3, 4, 1, 0]);
        })
        .then(() => {
          info("Testing swap(2, 1)");
          RequestsMenu.swapItemsAtIndices(2, 1);
          RequestsMenu.refreshZebra();
          return testContents([1, 3, 4, 2, 0]);
        })
        .then(() => {
          info("Testing swap(2, 2)");
          RequestsMenu.swapItemsAtIndices(2, 2);
          RequestsMenu.refreshZebra();
          return testContents([1, 3, 4, 2, 0]);
        })
        .then(() => {
          info("Testing swap(2, 3)");
          RequestsMenu.swapItemsAtIndices(2, 3);
          RequestsMenu.refreshZebra();
          return testContents([1, 2, 4, 3, 0]);
        })
        .then(() => {
          info("Testing swap(2, 4)");
          RequestsMenu.swapItemsAtIndices(2, 4);
          RequestsMenu.refreshZebra();
          return testContents([1, 4, 2, 3, 0]);
        })
        .then(() => {
          info("Testing swap(3, 0)");
          RequestsMenu.swapItemsAtIndices(3, 0);
          RequestsMenu.refreshZebra();
          return testContents([1, 4, 2, 0, 3]);
        })
        .then(() => {
          info("Testing swap(3, 1)");
          RequestsMenu.swapItemsAtIndices(3, 1);
          RequestsMenu.refreshZebra();
          return testContents([3, 4, 2, 0, 1]);
        })
        .then(() => {
          info("Testing swap(3, 2)");
          RequestsMenu.swapItemsAtIndices(3, 2);
          RequestsMenu.refreshZebra();
          return testContents([2, 4, 3, 0, 1]);
        })
        .then(() => {
          info("Testing swap(3, 3)");
          RequestsMenu.swapItemsAtIndices(3, 3);
          RequestsMenu.refreshZebra();
          return testContents([2, 4, 3, 0, 1]);
        })
        .then(() => {
          info("Testing swap(3, 4)");
          RequestsMenu.swapItemsAtIndices(3, 4);
          RequestsMenu.refreshZebra();
          return testContents([2, 3, 4, 0, 1]);
        })
        .then(() => {
          info("Testing swap(4, 0)");
          RequestsMenu.swapItemsAtIndices(4, 0);
          RequestsMenu.refreshZebra();
          return testContents([2, 3, 0, 4, 1]);
        })
        .then(() => {
          info("Testing swap(4, 1)");
          RequestsMenu.swapItemsAtIndices(4, 1);
          RequestsMenu.refreshZebra();
          return testContents([2, 3, 0, 1, 4]);
        })
        .then(() => {
          info("Testing swap(4, 2)");
          RequestsMenu.swapItemsAtIndices(4, 2);
          RequestsMenu.refreshZebra();
          return testContents([4, 3, 0, 1, 2]);
        })
        .then(() => {
          info("Testing swap(4, 3)");
          RequestsMenu.swapItemsAtIndices(4, 3);
          RequestsMenu.refreshZebra();
          return testContents([3, 4, 0, 1, 2]);
        })
        .then(() => {
          info("Testing swap(4, 4)");
          RequestsMenu.swapItemsAtIndices(4, 4);
          RequestsMenu.refreshZebra();
          return testContents([3, 4, 0, 1, 2]);
        })
        .then(() => {
          info("Clearing sort.");
          RequestsMenu.sortBy();
          return testContents([0, 1, 2, 3, 4]);
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish);
    });

    function testContents([a, b, c, d, e]) {
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
        "GET", STATUS_CODES_SJS + "?sts=100", {
          status: 101,
          statusText: "Switching Protocols",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          transferred: L10N.getStr("networkMenu.sizeUnavailable"),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(b),
        "GET", STATUS_CODES_SJS + "?sts=200", {
          status: 202,
          statusText: "Created",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(c),
        "GET", STATUS_CODES_SJS + "?sts=300", {
          status: 303,
          statusText: "See Other",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(d),
        "GET", STATUS_CODES_SJS + "?sts=400", {
          status: 404,
          statusText: "Not Found",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(e),
        "GET", STATUS_CODES_SJS + "?sts=500", {
          status: 501,
          statusText: "Not Implemented",
          type: "plain",
          fullMimeType: "text/plain; charset=utf-8",
          transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          size: L10N.getFormatStrWithNumbers("networkMenu.sizeKB", 0.02),
          time: true
        });

      return promise.resolve(null);
    }

    aDebuggee.performRequests();
  });
}
