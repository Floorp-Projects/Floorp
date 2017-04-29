/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test different text filtering flags
 */
const REQUESTS = [
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=css&text=sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=js&text=sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=font" },
  { url: "sjs_content-type-test-server.sjs?fmt=image" },
  { url: "sjs_content-type-test-server.sjs?fmt=audio" },
  { url: "sjs_content-type-test-server.sjs?fmt=video" },
  { url: "sjs_status-codes-test-server.sjs?sts=304" },
];

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
    url: STATUS_CODES_SJS + "?sts=304",
    data: {
      status: 304,
      statusText: "Not Modified",
      displayedStatus: "304",
      type: "plain",
      fullMimeType: "text/plain; charset=utf-8"
    }
  },
];

add_task(function* () {
  let { monitor } = yield initNetMonitor(FILTERING_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let {
    getDisplayedRequests,
    getSortedRequests,
  } = windowRequire("devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  function setFreetextFilter(value) {
    store.dispatch(Actions.setRequestFilterText(value));
  }

  info("Starting test... ");

  let waitNetwork = waitForNetworkEvents(monitor, 8);
  loadCommonFrameScript();
  yield performRequestsInContent(REQUESTS);
  yield waitNetwork;

  // Test running flag once requests finish running
  setFreetextFilter("is:running");
  testContents([0, 0, 0, 0, 0, 0, 0, 0]);

  // Test cached flag
  setFreetextFilter("is:from-cache");
  testContents([0, 0, 0, 0, 0, 0, 0, 1]);

  setFreetextFilter("is:cached");
  testContents([0, 0, 0, 0, 0, 0, 0, 1]);

  // Test negative cached flag
  setFreetextFilter("-is:from-cache");
  testContents([1, 1, 1, 1, 1, 1, 1, 0]);

  setFreetextFilter("-is:cached");
  testContents([1, 1, 1, 1, 1, 1, 1, 0]);

  // Test status-code flag
  setFreetextFilter("status-code:200");
  testContents([1, 1, 1, 1, 1, 1, 1, 0]);

  // Test status-code negative flag
  setFreetextFilter("-status-code:200");
  testContents([0, 0, 0, 0, 0, 0, 0, 1]);

  // Test mime-type flag
  setFreetextFilter("mime-type:HtmL");
  testContents([1, 0, 0, 0, 0, 0, 0, 0]);

  // Test mime-type negative flag
  setFreetextFilter("-mime-type:HtmL");
  testContents([0, 1, 1, 1, 1, 1, 1, 1]);

  // Test method flag
  setFreetextFilter("method:get");
  testContents([1, 1, 1, 1, 1, 1, 1, 1]);

  // Test unmatched method flag
  setFreetextFilter("method:post");
  testContents([0, 0, 0, 0, 0, 0, 0, 0]);

  // Test scheme flag (all requests are http)
  setFreetextFilter("scheme:http");
  testContents([1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("scheme:https");
  testContents([0, 0, 0, 0, 0, 0, 0, 0]);

  // Test mixing flags
  setFreetextFilter("-mime-type:HtmL status-code:200");
  testContents([0, 1, 1, 1, 1, 1, 1, 0]);

  // Test regex filter
  setFreetextFilter("regexp:content.*?Sam");
  testContents([1, 0, 0, 0, 0, 0, 0, 0]);

  yield teardown(monitor);

  function testContents(visibility) {
    const items = getSortedRequests(store.getState());
    const visibleItems = getDisplayedRequests(store.getState());

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
