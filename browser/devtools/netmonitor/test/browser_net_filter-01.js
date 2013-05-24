/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if filtering items in the network table works correctly.
 */

function test() {
  initNetMonitor(FILTERING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 8).then(() => {
      EventUtils.sendMouseEvent({ type: "mousedown" }, $("#details-pane-toggle"));

      isnot(RequestsMenu.selectedItem, null,
        "There should be a selected item in the requests menu.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be selected in the requests menu.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should not be hidden after toggle button was pressed.");

      testButtons();
      testContents([1, 1, 1, 1, 1, 1, 1, 1])
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
          testButtons("html");
          return testContents([1, 0, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
          testButtons("css");
          return testContents([0, 1, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-js-button"));
          testButtons("js");
          return testContents([0, 0, 1, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-xhr-button"));
          testButtons("xhr");
          return testContents([1, 1, 1, 1, 1, 1, 1, 1]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-fonts-button"));
          testButtons("fonts");
          return testContents([0, 0, 0, 1, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-images-button"));
          testButtons("images");
          return testContents([0, 0, 0, 0, 1, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-media-button"));
          testButtons("media");
          return testContents([0, 0, 0, 0, 0, 1, 1, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-flash-button"));
          testButtons("flash");
          return testContents([0, 0, 0, 0, 0, 0, 0, 1]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          testButtons();
          return testContents([1, 1, 1, 1, 1, 1, 1, 1]);
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish);
    });

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

    function testContents(aVisibility) {
      isnot(RequestsMenu.selectedItem, null,
        "There should still be a selected item after filtering.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be still selected after filtering.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should still be visible after filtering.");

      is(RequestsMenu.allItems.length, aVisibility.length,
        "There should be a specific amount of items in the requests menu.");
      is(RequestsMenu.visibleItems.length, aVisibility.filter(e => e).length,
        "There should be a specific amount of visbile items in the requests menu.");

      for (let i = 0; i < aVisibility.length; i++) {
        is(RequestsMenu.getItemAtIndex(i).target.hidden, !aVisibility[i],
          "The item at index " + i + " doesn't have the correct hidden state.");
      }

      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(0),
        "GET", CONTENT_TYPE_SJS + "?fmt=html", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "html",
          fullMimeType: "text/html; charset=utf-8"
      });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(1),
        "GET", CONTENT_TYPE_SJS + "?fmt=css", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "css",
          fullMimeType: "text/css; charset=utf-8"
      });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(2),
        "GET", CONTENT_TYPE_SJS + "?fmt=js", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "js",
          fullMimeType: "application/javascript; charset=utf-8"
      });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(3),
        "GET", CONTENT_TYPE_SJS + "?fmt=font", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "woff",
          fullMimeType: "font/woff"
      });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(4),
        "GET", CONTENT_TYPE_SJS + "?fmt=image", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "png",
          fullMimeType: "image/png"
      });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(5),
        "GET", CONTENT_TYPE_SJS + "?fmt=audio", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "ogg",
          fullMimeType: "audio/ogg"
      });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(6),
        "GET", CONTENT_TYPE_SJS + "?fmt=video", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "webm",
          fullMimeType: "video/webm"
      });
      verifyRequestItemTarget(RequestsMenu.getItemAtIndex(7),
        "GET", CONTENT_TYPE_SJS + "?fmt=flash", {
          fuzzyUrl: true,
          status: 200,
          statusText: "OK",
          type: "x-shockwave-flash",
          fullMimeType: "application/x-shockwave-flash"
      });

      return Promise.resolve(null);
    }

    aDebuggee.performRequests('{ "getMedia": true, "getFlash": true }');
  });
}
