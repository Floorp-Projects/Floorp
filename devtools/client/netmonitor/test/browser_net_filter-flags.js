/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test different text filtering flags
 */
const REQUESTS = [
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=html&res=undefined&text=Sample" +
         "&cookies=1" },
  { url: "sjs_content-type-test-server.sjs?fmt=css&text=sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=js&text=sample" },
  { url: "sjs_content-type-test-server.sjs?fmt=font" },
  { url: "sjs_content-type-test-server.sjs?fmt=image" },
  { url: "sjs_content-type-test-server.sjs?fmt=audio" },
  { url: "sjs_content-type-test-server.sjs?fmt=video" },
  { url: "sjs_content-type-test-server.sjs?fmt=gzip" },
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
    url: CONTENT_TYPE_SJS + "?fmt=gzip",
    data: {
      fuzzyUrl: true,
      status: 200,
      statusText: "OK",
      displayedStatus: "200",
      type: "plain",
      fullMimeType: "text/plain"
    }
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

  function type(string) {
    for (let ch of string) {
      EventUtils.synthesizeKey(ch, {}, monitor.panelWin);
    }
  }

  // Filtering network request will start fetching data lazily
  // (fetching requestHeaders & responseHeaders for filtering WS & XHR)
  // Lazy fetching will be executed when user focuses on filter box.
  function setFreetextFilter(value) {
    let filterBox = document.querySelector(".devtools-filterinput");
    filterBox.focus();
    filterBox.value = "";
    type(value);
  }

  info("Starting test... ");

  let waitNetwork = waitForNetworkEvents(monitor, REQUESTS.length);
  loadCommonFrameScript();
  yield performRequestsInContent(REQUESTS);
  yield waitNetwork;

  // Test running flag once requests finish running
  setFreetextFilter("is:running");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test cached flag
  setFreetextFilter("is:from-cache");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 1]);

  setFreetextFilter("is:cached");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 1]);

  // Test negative cached flag
  setFreetextFilter("-is:from-cache");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 0]);

  setFreetextFilter("-is:cached");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 0]);

  // Test status-code flag
  setFreetextFilter("status-code:200");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 0]);

  // Test status-code negative flag
  setFreetextFilter("-status-code:200");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 1]);

  // Test mime-type flag
  setFreetextFilter("mime-type:HtmL");
  yield testContents([1, 1, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test mime-type negative flag
  setFreetextFilter("-mime-type:HtmL");
  yield testContents([0, 0, 1, 1, 1, 1, 1, 1, 1, 1]);

  // Test method flag
  setFreetextFilter("method:get");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  // Test unmatched method flag
  setFreetextFilter("method:post");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test scheme flag (all requests are http)
  setFreetextFilter("scheme:http");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("scheme:https");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test regex filter
  setFreetextFilter("regexp:content.*?Sam");
  yield testContents([1, 1, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test set-cookie-name flag
  setFreetextFilter("set-cookie-name:name2");
  yield testContents([0, 1, 0, 0, 0, 0, 0, 0, 0, 0]);

  setFreetextFilter("set-cookie-name:not-existing");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test set-cookie-value flag
  setFreetextFilter("set-cookie-value:value2");
  yield testContents([0, 1, 0, 0, 0, 0, 0, 0, 0, 0]);

  setFreetextFilter("set-cookie-value:not-existing");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test set-cookie-domain flag
  setFreetextFilter("set-cookie-domain:.example.com");
  yield testContents([0, 1, 0, 0, 0, 0, 0, 0, 0, 0]);

  setFreetextFilter("set-cookie-domain:.foo.example.com");
  yield testContents([0, 1, 0, 0, 0, 0, 0, 0, 0, 0]);

  setFreetextFilter("set-cookie-domain:.not-existing.example.com");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test size
  setFreetextFilter("size:-1");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  setFreetextFilter("size:0");
  yield testContents([0, 0, 0, 0, 1, 1, 1, 1, 0, 1]);

  setFreetextFilter("size:34");
  yield testContents([0, 0, 1, 1, 0, 0, 0, 0, 0, 0]);

  // Testing the lower bound
  setFreetextFilter("size:9.659k");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 1, 0]);

  // Testing the actual value
  setFreetextFilter("size:10989");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 1, 0]);

  // Testing the upper bound
  setFreetextFilter("size:11.804k");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 1, 0]);

  // Test transferred
  setFreetextFilter("transferred:200");
  yield testContents([0, 0, 0, 0, 1, 1, 1, 1, 0, 0]);

  setFreetextFilter("transferred:234");
  yield testContents([1, 0, 1, 0, 0, 0, 0, 0, 0, 1]);

  setFreetextFilter("transferred:248");
  yield testContents([0, 0, 1, 1, 0, 0, 0, 0, 0, 1]);

  // Test larger-than
  setFreetextFilter("larger-than:-1");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("larger-than:0");
  yield testContents([1, 1, 1, 1, 0, 0, 0, 0, 1, 0]);

  setFreetextFilter("larger-than:33");
  yield testContents([0, 0, 1, 1, 0, 0, 0, 0, 1, 0]);

  setFreetextFilter("larger-than:34");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 1, 0]);

  setFreetextFilter("larger-than:10.73k");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 1, 0]);

  setFreetextFilter("larger-than:10.732k");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test transferred-larger-than
  setFreetextFilter("transferred-larger-than:-1");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("transferred-larger-than:214");
  yield testContents([1, 1, 1, 1, 0, 0, 0, 0, 1, 1]);

  setFreetextFilter("transferred-larger-than:247");
  yield testContents([0, 1, 1, 1, 0, 0, 0, 0, 1, 0]);

  setFreetextFilter("transferred-larger-than:248");
  yield testContents([0, 1, 0, 1, 0, 0, 0, 0, 1, 0]);

  setFreetextFilter("transferred-larger-than:10.73k");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test cause
  setFreetextFilter("cause:xhr");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("cause:script");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test has-response-header
  setFreetextFilter("has-response-header:Content-Type");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("has-response-header:Last-Modified");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test remote-ip
  setFreetextFilter("remote-ip:127.0.0.1");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("remote-ip:192.168.1.2");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test domain
  setFreetextFilter("domain:example.com");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("domain:wrongexample.com");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test protocol
  setFreetextFilter("protocol:http/1");
  yield testContents([1, 1, 1, 1, 1, 1, 1, 1, 1, 1]);

  setFreetextFilter("protocol:http/2");
  yield testContents([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

  // Test mixing flags
  setFreetextFilter("-mime-type:HtmL status-code:200");
  yield testContents([0, 0, 1, 1, 1, 1, 1, 1, 1, 0]);

  yield teardown(monitor);

  function* testContents(visibility) {
    let requestItems = document.querySelectorAll(".request-list-item");
    for (let requestItem of requestItems) {
      requestItem.scrollIntoView();
      let requestsListStatus = requestItem.querySelector(".requests-list-status");
      EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
      yield waitUntil(() => requestsListStatus.title);
    }

    let items = getSortedRequests(store.getState());
    let visibleItems = getDisplayedRequests(store.getState());

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

      // Filter results will be updated asynchronously, so we should wait until
      // displayed requests reach final state.
      yield waitUntil(() => {
        visibleItems = getDisplayedRequests(store.getState());
        isThere = visibleItems.some(r => r.id == itemId);
        return isThere === shouldBeVisible;
      });

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
