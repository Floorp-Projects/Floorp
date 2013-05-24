/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if filtering items in the network table works correctly with new requests
 * and while sorting is enabled.
 */

function test() {
  initNetMonitor(FILTERING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 7).then(() => {
      EventUtils.sendMouseEvent({ type: "mousedown" }, $("#details-pane-toggle"));

      isnot(RequestsMenu.selectedItem, null,
        "There should be a selected item in the requests menu.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be selected in the requests menu.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should not be hidden after toggle button was pressed.");

      testButtons();
      testContents([0, 1, 2, 3, 4, 5, 6], 7, 0)
        .then(() => {
          info("Sorting by size, ascending.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-size-button"));
          testButtons();
          return testContents([6, 4, 5, 0, 1, 2, 3], 7, 6);
        })
        .then(() => {
          info("Testing html filtering.");
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
          testButtons("html");
          return testContents([6, 4, 5, 0, 1, 2, 3], 1, 6);
        })
        .then(() => {
          info("Performing more requests.");
          aDebuggee.performRequests('{ "getMedia": true }');
          return waitForNetworkEvents(aMonitor, 7);
        })
        .then(() => {
          info("Testing html filtering again.");
          resetSorting();
          testButtons("html");
          return testContents([8, 13, 9, 11, 10, 12, 0, 4, 1, 5, 2, 6, 3, 7], 2, 13);
        })
        .then(() => {
          info("Performing more requests.");
          aDebuggee.performRequests('{ "getMedia": true }');
          return waitForNetworkEvents(aMonitor, 7);
        })
        .then(() => {
          info("Testing html filtering again.");
          resetSorting();
          testButtons("html");
          return testContents([12, 13, 20, 14, 16, 18, 15, 17, 19, 0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11], 3, 20);
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish);
    });

    function resetSorting() {
      for (let i = 0; i < 3; i++) {
        EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-size-button"));
      }
    }

    function testButtons(aFilterType) {
      let doc = aMonitor.panelWin.document;
      let target = doc.querySelector("#requests-menu-filter-" + aFilterType + "-button");
      let buttons = doc.querySelectorAll(".requests-menu-footer-button");

      for (let button of buttons) {
        if (button != target) {
          is(button.hasAttribute("checked"), false,
            "The " + button.id + " button should not have a 'checked' attribute.");
        } else {
          is(button.hasAttribute("checked"), true,
            "The " + button.id + " button should have a 'checked' attribute.");
        }
      }
    }

    function testContents(aOrder, aVisible, aSelection) {
      isnot(RequestsMenu.selectedItem, null,
        "There should still be a selected item after filtering.");
      is(RequestsMenu.selectedIndex, aSelection,
        "The first item should be still selected after filtering.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should still be visible after filtering.");

      is(RequestsMenu.allItems.length, aOrder.length,
        "There should be a specific amount of items in the requests menu.");
      is(RequestsMenu.visibleItems.length, aVisible,
        "There should be a specific amount of visbile items in the requests menu.");

      for (let i = 0; i < aOrder.length; i++) {
        is(RequestsMenu.getItemAtIndex(i), RequestsMenu.allItems[i],
          "The requests menu items aren't ordered correctly. Misplaced item " + i + ".");
      }

      for (let i = 0, len = aOrder.length / 7; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i]),
          "GET", CONTENT_TYPE_SJS + "?fmt=html", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "html",
            fullMimeType: "text/html; charset=utf-8"
        });
      }
      for (let i = 0, len = aOrder.length / 7; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len]),
          "GET", CONTENT_TYPE_SJS + "?fmt=css", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "css",
            fullMimeType: "text/css; charset=utf-8"
        });
      }
      for (let i = 0, len = aOrder.length / 7; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 2]),
          "GET", CONTENT_TYPE_SJS + "?fmt=js", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "js",
            fullMimeType: "application/javascript; charset=utf-8"
        });
      }
      for (let i = 0, len = aOrder.length / 7; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 3]),
          "GET", CONTENT_TYPE_SJS + "?fmt=font", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "woff",
            fullMimeType: "font/woff"
        });
      }
      for (let i = 0, len = aOrder.length / 7; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 4]),
          "GET", CONTENT_TYPE_SJS + "?fmt=image", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "png",
            fullMimeType: "image/png"
        });
      }
      for (let i = 0, len = aOrder.length / 7; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 5]),
          "GET", CONTENT_TYPE_SJS + "?fmt=audio", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "ogg",
            fullMimeType: "audio/ogg"
        });
      }
      for (let i = 0, len = aOrder.length / 7; i < len; i++) {
        verifyRequestItemTarget(RequestsMenu.getItemAtIndex(aOrder[i + len * 6]),
          "GET", CONTENT_TYPE_SJS + "?fmt=video", {
            fuzzyUrl: true,
            status: 200,
            statusText: "OK",
            type: "webm",
            fullMimeType: "video/webm"
        });
      }

      return Promise.resolve(null);
    }

    let str = "'<p>'" + new Array(10).join(Math.random(10)) + "'</p>'";
    aDebuggee.performRequests('{ "htmlContent": "' + str + '", "getMedia": true }');
  });
}
