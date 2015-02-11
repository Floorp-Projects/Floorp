/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if filtering items in the network table works correctly.
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

const REQUESTS_WITH_MEDIA_AND_FLASH = REQUESTS_WITH_MEDIA.concat([
  { url: "sjs_content-type-test-server.sjs?fmt=flash" },
]);

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

      // First test with single filters...
      testFilterButtons(aMonitor, "all");
      testContents([1, 1, 1, 1, 1, 1, 1, 1])
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
          testFilterButtons(aMonitor, "html");
          return testContents([1, 0, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          // Reset filters
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
          testFilterButtons(aMonitor, "css");
          return testContents([0, 1, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-js-button"));
          testFilterButtons(aMonitor, "js");
          return testContents([0, 0, 1, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-xhr-button"));
          testFilterButtons(aMonitor, "xhr");
          return testContents([1, 1, 1, 1, 1, 1, 1, 1]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-fonts-button"));
          testFilterButtons(aMonitor, "fonts");
          return testContents([0, 0, 0, 1, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-images-button"));
          testFilterButtons(aMonitor, "images");
          return testContents([0, 0, 0, 0, 1, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-media-button"));
          testFilterButtons(aMonitor, "media");
          return testContents([0, 0, 0, 0, 0, 1, 1, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-flash-button"));
          testFilterButtons(aMonitor, "flash");
          return testContents([0, 0, 0, 0, 0, 0, 0, 1]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          testFilterButtons(aMonitor, "all");
          return testContents([1, 1, 1, 1, 1, 1, 1, 1]);
        })
        // ...then combine multiple filters together.
        .then(() => {
          // Enable filtering for html and css; should show request of both type.
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
          testFilterButtonsCustom(aMonitor, [0, 1, 1, 0, 0, 0, 0, 0, 0, 0]);
          return testContents([1, 1, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-flash-button"));
          testFilterButtonsCustom(aMonitor, [0, 1, 1, 0, 0, 0, 0, 0, 1, 0]);
          return testContents([1, 1, 0, 0, 0, 0, 0, 1]);
        })
        .then(() => {
          // Disable some filters. Only one left active.
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-flash-button"));
          testFilterButtons(aMonitor, "html");
          return testContents([1, 0, 0, 0, 0, 0, 0, 0]);
        })
        .then(() => {
          // Disable last active filter. Should toggle to all.
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
          testFilterButtons(aMonitor, "all");
          return testContents([1, 1, 1, 1, 1, 1, 1, 1]);
        })
        .then(() => {
          // Enable few filters and click on all. Only "all" should be checked.
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
          testFilterButtonsCustom(aMonitor, [0, 1, 1, 0, 0, 0, 0, 0, 0]);
          EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-all-button"));
          testFilterButtons(aMonitor, "all");
          return testContents([1, 1, 1, 1, 1, 1, 1, 1]);
        })
        .then(() => {
          return teardown(aMonitor);
        })
        .then(finish);
    });

    function testContents(aVisibility) {
      isnot(RequestsMenu.selectedItem, null,
        "There should still be a selected item after filtering.");
      is(RequestsMenu.selectedIndex, 0,
        "The first item should be still selected after filtering.");
      is(NetMonitorView.detailsPaneHidden, false,
        "The details pane should still be visible after filtering.");

      is(RequestsMenu.items.length, aVisibility.length,
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

      return promise.resolve(null);
    }

    loadCommonFrameScript();
    performRequestsInContent(REQUESTS_WITH_MEDIA_AND_FLASH);
  });
}
