/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if filtering items in the network table works correctly.
 */
const BASIC_REQUESTS = [
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=css&text=sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=js&text=sample" },
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

const REQUESTS_WITH_MEDIA_AND_FLASH_AND_WS = REQUESTS_WITH_MEDIA_AND_FLASH.concat([
  /* "Upgrade" is a reserved header and can not be set on XMLHttpRequest */
  { url: "sjs_content-type-test-server.sjs?fmt=ws" },
]);

const EXPECTED_REQUESTS = [
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=html",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "html",
      fullMimeType: "text/html; charset=utf-8"
    }
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=css",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "css",
      fullMimeType: "text/css; charset=utf-8"
    }
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=js",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "js",
      fullMimeType: "application/javascript; charset=utf-8"
    }
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=font",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "woff",
      fullMimeType: "font/woff"
    }
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=image",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "png",
      fullMimeType: "image/png"
    }
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=audio",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "ogg",
      fullMimeType: "audio/ogg"
    }
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=video",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "webm",
      fullMimeType: "video/webm"
    },
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=flash",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      type: "x-shockwave-flash",
      fullMimeType: "application/x-shockwave-flash"
    }
  },
  {
    method: "GET",
    url: CONTENT_TYPE_SJS + "?fmt=ws",
    data: {
      fuzzyUrl: true,
      status: 101,
      statusText: "Switching Protocols",
    }
  }
];

add_task(function* () {
  let { monitor } = yield initNetMonitor(FILTERING_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSelectedRequest,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  function setFreetextFilter(value) {
    store.dispatch(Actions.setRequestFilterText(value));
  }

  info("Starting test... ");

  let wait = waitForNetworkEvents(monitor, 9);
  loadCommonFrameScript();
  yield performRequestsInContent(REQUESTS_WITH_MEDIA_AND_FLASH_AND_WS);
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);

  isnot(getSelectedRequest(store.getState()), null,
    "There should be a selected item in the requests menu.");
  is(getSelectedIndex(store.getState()), 0,
    "The first item should be selected in the requests menu.");
  is(!!document.querySelector(".network-details-panel"), true,
    "The network details panel should render correctly.");

  // First test with single filters...
  testFilterButtons(monitor, "all");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-html-button"));
  testFilterButtons(monitor, "html");
  yield testContents([1, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Reset filters
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-css-button"));
  testFilterButtons(monitor, "css");
  yield testContents([0, 1, 0, 0, 0, 0, 0, 0, 0]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-js-button"));
  testFilterButtons(monitor, "js");
  yield testContents([0, 0, 1, 0, 0, 0, 0, 0, 0]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-xhr-button"));
  testFilterButtons(monitor, "xhr");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 0]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  EventUtils.sendMouseEvent({ type: "click" },
     document.querySelector(".requests-list-filter-fonts-button"));
  testFilterButtons(monitor, "fonts");
  yield testContents([0, 0, 0, 1, 0, 0, 0, 0, 0]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-images-button"));
  testFilterButtons(monitor, "images");
  yield testContents([0, 0, 0, 0, 1, 0, 0, 0, 0]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-media-button"));
  testFilterButtons(monitor, "media");
  yield testContents([0, 0, 0, 0, 0, 1, 1, 0, 0]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-ws-button"));
  testFilterButtons(monitor, "ws");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 1]);

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));

  testFilterButtons(monitor, "all");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1]);

  // Text in filter box that matches nothing should hide all.
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  setFreetextFilter("foobar");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Text in filter box that matches should filter out everything else.
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  setFreetextFilter("sample");
  yield testContents([1, 1, 1, 0, 0, 0, 0, 0, 0]);

  // Text in filter box that matches should filter out everything else.
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  setFreetextFilter("SAMPLE");
  yield testContents([1, 1, 1, 0, 0, 0, 0, 0, 0]);

  // Test negative filtering (only show unmatched items)
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  setFreetextFilter("-sample");
  yield testContents([0, 0, 0, 1, 1, 1, 1, 1, 1]);

  // ...then combine multiple filters together.

  // Enable filtering for html and css; should show request of both type.
  setFreetextFilter("");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-html-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-css-button"));
  testFilterButtonsCustom(monitor, [0, 1, 1, 0, 0, 0, 0, 0, 0, 0]);
  yield testContents([1, 1, 0, 0, 0, 0, 0, 0, 0]);

  // Html and css filter enabled and text filter should show just the html and css match.
  // Should not show both the items matching the button plus the items matching the text.
  setFreetextFilter("sample");
  yield testContents([1, 1, 0, 0, 0, 0, 0, 0, 0]);
  setFreetextFilter("");
  testFilterButtonsCustom(monitor, [0, 1, 1, 0, 0, 0, 0, 0, 0, 0]);
  yield testContents([1, 1, 0, 0, 0, 0, 0, 0, 0]);

  // Disable some filters. Only one left active.
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-css-button"));
  testFilterButtons(monitor, "html");
  yield testContents([1, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Disable last active filter. Should toggle to all.
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-html-button"));
  testFilterButtons(monitor, "all");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1]);

  // Enable few filters and click on all. Only "all" should be checked.
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-html-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-css-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-ws-button"));
  testFilterButtonsCustom(monitor, [0, 1, 1, 0, 0, 0, 0, 0, 1, 0]);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-all-button"));
  testFilterButtons(monitor, "all");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1]);

  yield teardown(monitor);

  function getSelectedIndex(state) {
    if (!state.requests.selectedId) {
      return -1;
    }
    return getSortedRequests(state).findIndex(r => r.id === state.requests.selectedId);
  }

  function* testContents(visibility) {
    let requestItems = document.querySelectorAll(".request-list-item");
    for (let requestItem of requestItems) {
      requestItem.scrollIntoView();
      let requestsListStatus = requestItem.querySelector(".requests-list-status");
      EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
      yield waitUntil(() => requestsListStatus.title);
    }

    isnot(getSelectedRequest(store.getState()), undefined,
      "There should still be a selected item after filtering.");
    is(getSelectedIndex(store.getState()), 0,
      "The first item should be still selected after filtering.");

    let items = getSortedRequests(store.getState());
    let visibleItems;

    // Filter results will be updated asynchronously, so we should wait until
    // displayed requests reach final state.
    yield waitUntil(() => {
      visibleItems = getDisplayedRequests(store.getState());
      return visibleItems.size === visibility.filter(e => e).length;
    });

    is(items.size, visibility.length,
       "There should be a specific amount of items in the requests menu.");
    is(visibleItems.size, visibility.filter(e => e).length,
       "There should be a specific amount of visible items in the requests menu.");

    for (let i = 0; i < visibility.length; i++) {
      let itemId = items.get(i).id;
      let shouldBeVisible = !!visibility[i];
      let isThere = visibleItems.some(r => r.id == itemId);

      is(isThere, shouldBeVisible,
        `The item at index ${i} has visibility=${shouldBeVisible}`);

      if (shouldBeVisible) {
        let { method, url, data } = EXPECTED_REQUESTS[i];
        verifyRequestItemTarget(
          document,
          getDisplayedRequests(store.getState()),
          getSortedRequests(store.getState()).get(i),
          method,
          url,
          data
        );
      }
    }
  }
});
