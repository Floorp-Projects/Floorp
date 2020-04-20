/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../shared/test/shared-head.js */
/* exported Toolbox, restartNetMonitor, teardown, waitForExplicitFinish,
   verifyRequestItemTarget, waitFor, waitForDispatch, testFilterButtons,
   performRequestsInContent, waitForNetworkEvents, selectIndexAndWaitForSourceEditor,
   testColumnsAlignment, hideColumn, showColumn, performRequests, waitForRequestData,
   toggleBlockedUrl */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

const {
  getFormattedIPAndPort,
  getFormattedTime,
} = require("devtools/client/netmonitor/src/utils/format-utils");

const {
  getSortedRequests,
  getRequestById,
} = require("devtools/client/netmonitor/src/selectors/index");

const {
  getUnicodeUrl,
  getUnicodeHostname,
} = require("devtools/client/shared/unicode-url");
const {
  getFormattedProtocol,
  getUrlHost,
  getUrlScheme,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  EVENTS,
  TEST_EVENTS,
} = require("devtools/client/netmonitor/src/constants");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

/* eslint-disable no-unused-vars, max-len */
const EXAMPLE_URL =
  "http://example.com/browser/devtools/client/netmonitor/test/";
const HTTPS_EXAMPLE_URL =
  "https://example.com/browser/devtools/client/netmonitor/test/";
/* Since the test server will proxy `ws://example.com` to websocket server on 9988,
so we must sepecify the port explicitly */
const WS_URL = "ws://127.0.0.1:8888/browser/devtools/client/netmonitor/test/";
const WS_HTTP_URL =
  "http://127.0.0.1:8888/browser/devtools/client/netmonitor/test/";

const WS_BASE_URL =
  "http://mochi.test:8888/browser/devtools/client/netmonitor/test/";
const WS_PAGE_URL = WS_BASE_URL + "html_ws-test-page.html";
const WS_PAGE_EARLY_CONNECTION_URL =
  WS_BASE_URL + "html_ws-early-connection-page.html";
const API_CALLS_URL = EXAMPLE_URL + "html_api-calls-test-page.html";
const SIMPLE_URL = EXAMPLE_URL + "html_simple-test-page.html";
const NAVIGATE_URL = EXAMPLE_URL + "html_navigate-test-page.html";
const CONTENT_TYPE_WITHOUT_CACHE_URL =
  EXAMPLE_URL + "html_content-type-without-cache-test-page.html";
const CONTENT_TYPE_WITHOUT_CACHE_REQUESTS = 8;
const CYRILLIC_URL = EXAMPLE_URL + "html_cyrillic-test-page.html";
const STATUS_CODES_URL = EXAMPLE_URL + "html_status-codes-test-page.html";
const POST_DATA_URL = EXAMPLE_URL + "html_post-data-test-page.html";
const POST_ARRAY_DATA_URL = EXAMPLE_URL + "html_post-array-data-test-page.html";
const POST_JSON_URL = EXAMPLE_URL + "html_post-json-test-page.html";
const POST_RAW_URL = EXAMPLE_URL + "html_post-raw-test-page.html";
const POST_RAW_URL_WITH_HASH = EXAMPLE_URL + "html_header-test-page.html";
const POST_RAW_WITH_HEADERS_URL =
  EXAMPLE_URL + "html_post-raw-with-headers-test-page.html";
const PARAMS_URL = EXAMPLE_URL + "html_params-test-page.html";
const JSONP_URL = EXAMPLE_URL + "html_jsonp-test-page.html";
const JSON_LONG_URL = EXAMPLE_URL + "html_json-long-test-page.html";
const JSON_MALFORMED_URL = EXAMPLE_URL + "html_json-malformed-test-page.html";
const JSON_CUSTOM_MIME_URL =
  EXAMPLE_URL + "html_json-custom-mime-test-page.html";
const JSON_TEXT_MIME_URL = EXAMPLE_URL + "html_json-text-mime-test-page.html";
const JSON_B64_URL = EXAMPLE_URL + "html_json-b64.html";
const JSON_BASIC_URL = EXAMPLE_URL + "html_json-basic.html";
const JSON_EMPTY_URL = EXAMPLE_URL + "html_json-empty.html";
const SORTING_URL = EXAMPLE_URL + "html_sorting-test-page.html";
const FILTERING_URL = EXAMPLE_URL + "html_filter-test-page.html";
const INFINITE_GET_URL = EXAMPLE_URL + "html_infinite-get-page.html";
const CUSTOM_GET_URL = EXAMPLE_URL + "html_custom-get-page.html";
const HTTPS_CUSTOM_GET_URL = HTTPS_EXAMPLE_URL + "html_custom-get-page.html";
const SINGLE_GET_URL = EXAMPLE_URL + "html_single-get-page.html";
const STATISTICS_URL = EXAMPLE_URL + "html_statistics-test-page.html";
const CURL_URL = EXAMPLE_URL + "html_copy-as-curl.html";
const CURL_UTILS_URL = EXAMPLE_URL + "html_curl-utils.html";
const SEND_BEACON_URL = EXAMPLE_URL + "html_send-beacon.html";
const CORS_URL = EXAMPLE_URL + "html_cors-test-page.html";
const PAUSE_URL = EXAMPLE_URL + "html_pause-test-page.html";
const OPEN_REQUEST_IN_TAB_URL = EXAMPLE_URL + "html_open-request-in-tab.html";
const CSP_URL = EXAMPLE_URL + "html_csp-test-page.html";
const CSP_RESEND_URL = EXAMPLE_URL + "html_csp-resend-test-page.html";

const SIMPLE_SJS = EXAMPLE_URL + "sjs_simple-test-server.sjs";
const SIMPLE_UNSORTED_COOKIES_SJS =
  EXAMPLE_URL + "sjs_simple-unsorted-cookies-test-server.sjs";
const CONTENT_TYPE_SJS = EXAMPLE_URL + "sjs_content-type-test-server.sjs";
const WS_CONTENT_TYPE_SJS = WS_HTTP_URL + "sjs_content-type-test-server.sjs";
const WS_WS_CONTENT_TYPE_SJS = WS_URL + "sjs_content-type-test-server.sjs";
const HTTPS_CONTENT_TYPE_SJS =
  HTTPS_EXAMPLE_URL + "sjs_content-type-test-server.sjs";
const SERVER_TIMINGS_TYPE_SJS =
  HTTPS_EXAMPLE_URL + "sjs_timings-test-server.sjs";
const STATUS_CODES_SJS = EXAMPLE_URL + "sjs_status-codes-test-server.sjs";
const SORTING_SJS = EXAMPLE_URL + "sjs_sorting-test-server.sjs";
const HTTPS_REDIRECT_SJS = EXAMPLE_URL + "sjs_https-redirect-test-server.sjs";
const CORS_SJS_PATH =
  "/browser/devtools/client/netmonitor/test/sjs_cors-test-server.sjs";
const HSTS_SJS = EXAMPLE_URL + "sjs_hsts-test-server.sjs";
const METHOD_SJS = EXAMPLE_URL + "sjs_method-test-server.sjs";
const SLOW_SJS = EXAMPLE_URL + "sjs_slow-test-server.sjs";
const SET_COOKIE_SAME_SITE_SJS = EXAMPLE_URL + "sjs_set-cookie-same-site.sjs";
const SEARCH_SJS = EXAMPLE_URL + "sjs_search-test-server.sjs";

const HSTS_BASE_URL = EXAMPLE_URL;
const HSTS_PAGE_URL = CUSTOM_GET_URL;

const TEST_IMAGE = EXAMPLE_URL + "test-image.png";
const TEST_IMAGE_DATA_URI =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg==";

const SETTINGS_MENU_ITEMS = {
  "persist-logs": ".netmonitor-settings-persist-item",
  "import-har": ".netmonitor-settings-import-har-item",
  "save-har": ".netmonitor-settings-import-save-item",
  "copy-har": ".netmonitor-settings-import-copy-item",
};

/* eslint-enable no-unused-vars, max-len */

// All tests are asynchronous.
waitForExplicitFinish();

const gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
// To enable logging for try runs, just set the pref to true.
Services.prefs.setBoolPref("devtools.debugger.log", false);

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

// Always reset some prefs to their original values after the test finishes.
const gDefaultFilters = Services.prefs.getCharPref(
  "devtools.netmonitor.filters"
);

// Reveal many columns for test
Services.prefs.setCharPref(
  "devtools.netmonitor.visibleColumns",
  '["initiator","contentSize","cookies","domain","duration",' +
    '"endTime","file","url","latency","method","protocol",' +
    '"remoteip","responseTime","scheme","setCookies",' +
    '"startTime","status","transferred","type","waterfall"]'
);

Services.prefs.setCharPref(
  "devtools.netmonitor.columnsData",
  '[{"name":"status","minWidth":30,"width":5},' +
    '{"name":"method","minWidth":30,"width":5},' +
    '{"name":"domain","minWidth":30,"width":10},' +
    '{"name":"file","minWidth":30,"width":25},' +
    '{"name":"url","minWidth":30,"width":25},' +
    '{"name":"initiator","minWidth":30,"width":20},' +
    '{"name":"type","minWidth":30,"width":5},' +
    '{"name":"transferred","minWidth":30,"width":10},' +
    '{"name":"contentSize","minWidth":30,"width":5},' +
    '{"name":"waterfall","minWidth":150,"width":15}]'
);

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");

  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setCharPref("devtools.netmonitor.filters", gDefaultFilters);
  Services.prefs.clearUserPref("devtools.cache.disabled");
  Services.prefs.clearUserPref("devtools.netmonitor.columnsData");
  Services.prefs.clearUserPref("devtools.netmonitor.visibleColumns");
  Services.cookies.removeAll();
});

function waitForNavigation(target) {
  return new Promise(resolve => {
    target.once("will-navigate", () => {
      target.once("navigate", () => {
        resolve();
      });
    });
  });
}

function toggleCache(target, disabled) {
  const options = { cacheDisabled: disabled, performReload: true };
  const navigationFinished = waitForNavigation(target);

  // Disable the cache for any toolbox that it is opened from this point on.
  Services.prefs.setBoolPref("devtools.cache.disabled", disabled);

  return target.reconfigure({ options }).then(() => navigationFinished);
}

/**
 * Wait for 2 markers during document load.
 */
function waitForTimelineMarkers(monitor) {
  return new Promise(resolve => {
    const markers = [];

    function handleTimelineEvent(marker) {
      info(`Got marker: ${marker.name}`);
      markers.push(marker);
      if (markers.length == 2) {
        monitor.panelWin.api.off(
          TEST_EVENTS.TIMELINE_EVENT,
          handleTimelineEvent
        );
        info("Got two timeline markers, done waiting");
        resolve(markers);
      }
    }

    monitor.panelWin.api.on(TEST_EVENTS.TIMELINE_EVENT, handleTimelineEvent);
  });
}

let finishedQueue = {};
const updatingTypes = [
  "NetMonitor:NetworkEventUpdating:RequestCookies",
  "NetMonitor:NetworkEventUpdating:ResponseCookies",
  "NetMonitor:NetworkEventUpdating:RequestHeaders",
  "NetMonitor:NetworkEventUpdating:ResponseHeaders",
  "NetMonitor:NetworkEventUpdating:RequestPostData",
  "NetMonitor:NetworkEventUpdating:ResponseContent",
  "NetMonitor:NetworkEventUpdating:SecurityInfo",
  "NetMonitor:NetworkEventUpdating:EventTimings",
];
const updatedTypes = [
  "NetMonitor:NetworkEventUpdated:RequestCookies",
  "NetMonitor:NetworkEventUpdated:ResponseCookies",
  "NetMonitor:NetworkEventUpdated:RequestHeaders",
  "NetMonitor:NetworkEventUpdated:ResponseHeaders",
  "NetMonitor:NetworkEventUpdated:RequestPostData",
  "NetMonitor:NetworkEventUpdated:ResponseContent",
  "NetMonitor:NetworkEventUpdated:SecurityInfo",
  "NetMonitor:NetworkEventUpdated:EventTimings",
];

// Start collecting all networkEventUpdate event when panel is opened.
// removeTab() should be called once all corresponded RECEIVED_* events finished.
function startNetworkEventUpdateObserver(panelWin) {
  updatingTypes.forEach(type =>
    panelWin.api.on(type, actor => {
      const key = actor + "-" + updatedTypes[updatingTypes.indexOf(type)];
      finishedQueue[key] = finishedQueue[key] ? finishedQueue[key] + 1 : 1;
    })
  );

  updatedTypes.forEach(type =>
    panelWin.api.on(type, actor => {
      const key = actor + "-" + type;
      finishedQueue[key] = finishedQueue[key] ? finishedQueue[key] - 1 : -1;
    })
  );
}

async function waitForAllNetworkUpdateEvents() {
  function checkNetworkEventUpdateState() {
    for (const key in finishedQueue) {
      if (finishedQueue[key] > 0) {
        return false;
      }
    }

    return true;
  }
  info("Wait for completion of all NetworkUpdateEvents packets...");
  await waitUntil(() => checkNetworkEventUpdateState());
  finishedQueue = {};
}

function initNetMonitor(url, { requestCount, enableCache = false }) {
  info("Initializing a network monitor pane.");

  if (!requestCount) {
    ok(
      false,
      "initNetMonitor should be given a number of requests the page will perform"
    );
  }

  return (async function() {
    const tab = await addTab(url);
    info("Net tab added successfully: " + url);

    const target = await TargetFactory.forTab(tab);

    const toolbox = await gDevTools.showToolbox(target, "netmonitor");
    info("Network monitor pane shown successfully.");

    const monitor = toolbox.getCurrentPanel();

    startNetworkEventUpdateObserver(monitor.panelWin);

    if (!enableCache) {
      const panel = monitor.panelWin;
      const { store, windowRequire } = panel;
      const Actions = windowRequire(
        "devtools/client/netmonitor/src/actions/index"
      );

      info("Disabling cache and reloading page.");

      const requestsDone = waitForNetworkEvents(monitor, requestCount);
      const markersDone = waitForTimelineMarkers(monitor);
      await toggleCache(target, true);
      await Promise.all([requestsDone, markersDone]);
      info(
        "Cache disabled when the current and all future toolboxes are open."
      );

      const consoleFront = await target.getFront("console");

      // Remove any requests generated by the reload while toggling the cache to
      // avoid interfering with the test.
      isnot(
        [...consoleFront.getNetworkEvents()].length,
        0,
        "Request to reconfigure the tab was recorded."
      );
      info("Clearing requests in the console client.");
      await consoleFront.clearNetworkRequests();
      info("Clearing requests in the UI.");

      store.dispatch(Actions.clearRequests());
    }

    return { tab, monitor, toolbox };
  })();
}

function restartNetMonitor(monitor, { requestCount }) {
  info("Restarting the specified network monitor.");

  return (async function() {
    const tab = monitor.toolbox.target.localTab;
    const url = tab.linkedBrowser.currentURI.spec;

    await waitForAllNetworkUpdateEvents();
    info("All pending requests finished.");

    const onDestroyed = monitor.once("destroyed");
    await removeTab(tab);
    await onDestroyed;

    return initNetMonitor(url, { requestCount });
  })();
}

function teardown(monitor) {
  info("Destroying the specified network monitor.");

  return (async function() {
    const tab = monitor.toolbox.target.localTab;

    await waitForAllNetworkUpdateEvents();
    info("All pending requests finished.");

    await monitor.toolbox.destroy();
    await removeTab(tab);
  })();
}

function isFiltering(monitor) {
  const doc = monitor.panelWin.document;
  return !!doc.querySelector(
    ".requests-list-filter-buttons button[aria-pressed]"
  );
}

function waitForNetworkEvents(monitor, getRequests) {
  return new Promise(resolve => {
    const panel = monitor.panelWin;
    const { getNetworkRequest } = panel.connector;
    let networkEvent = 0;
    let nonBlockedNetworkEvent = 0;
    let payloadReady = 0;
    let eventTimings = 0;
    const filtering = isFiltering(monitor);

    function onNetworkEvent(actor) {
      const networkInfo = getNetworkRequest(actor);
      if (!networkInfo) {
        // Must have been related to reloading document to disable cache.
        // Ignore the event.
        return;
      }
      networkEvent++;
      if (!networkInfo.blockedReason) {
        nonBlockedNetworkEvent++;
      }
      maybeResolve(TEST_EVENTS.NETWORK_EVENT, actor, networkInfo);
    }

    function onPayloadReady(actor) {
      const networkInfo = getNetworkRequest(actor);
      if (!networkInfo) {
        // Must have been related to reloading document to disable cache.
        // Ignore the event.
        return;
      }
      payloadReady++;
      maybeResolve(EVENTS.PAYLOAD_READY, actor, networkInfo);
    }

    function onEventTimings(actor) {
      const networkInfo = getNetworkRequest(actor);
      if (!networkInfo) {
        // Must have been related to reloading document to disable cache.
        // Ignore the event.
        return;
      }
      eventTimings++;
      maybeResolve(EVENTS.RECEIVED_EVENT_TIMINGS, actor, networkInfo);
    }

    function maybeResolve(event, actor, networkInfo) {
      info(
        "> Network event progress: " +
          "NetworkEvent: " +
          networkEvent +
          "/" +
          getRequests +
          ", " +
          "PayloadReady: " +
          payloadReady +
          "/" +
          getRequests +
          "EventTimings: " +
          eventTimings +
          "/" +
          getRequests +
          ", " +
          "got " +
          event +
          " for " +
          actor
      );

      const { document } = monitor.panelWin;
      // Wait until networkEvent, payloadReady and event timings finish for each request.
      // The UI won't fetch timings when:
      // * hidden in background,
      // * for any blocked request,
      // * when filtering.
      if (
        networkEvent >= getRequests &&
        payloadReady >= getRequests &&
        (eventTimings >= nonBlockedNetworkEvent ||
          document.visibilityState == "hidden" ||
          filtering)
      ) {
        panel.api.off(TEST_EVENTS.NETWORK_EVENT, onNetworkEvent);
        panel.api.off(EVENTS.PAYLOAD_READY, onPayloadReady);
        panel.api.off(EVENTS.RECEIVED_EVENT_TIMINGS, onEventTimings);
        executeSoon(resolve);
      }
    }

    panel.api.on(TEST_EVENTS.NETWORK_EVENT, onNetworkEvent);
    panel.api.on(EVENTS.PAYLOAD_READY, onPayloadReady);
    panel.api.on(EVENTS.RECEIVED_EVENT_TIMINGS, onEventTimings);
  });
}

function verifyRequestItemTarget(
  document,
  requestList,
  requestItem,
  method,
  url,
  data = {}
) {
  info("> Verifying: " + method + " " + url + " " + data.toSource());

  const visibleIndex = requestList.findIndex(
    needle => needle.id === requestItem.id
  );

  isnot(visibleIndex, -1, "The requestItem exists");
  info("Visible index of item: " + visibleIndex);

  const {
    fuzzyUrl,
    status,
    statusText,
    cause,
    type,
    fullMimeType,
    transferred,
    size,
    time,
    displayedStatus,
  } = data;

  const target = document.querySelectorAll(".request-list-item")[visibleIndex];

  // Bug 1414981 - Request URL should not show #hash
  const unicodeUrl = getUnicodeUrl(url.split("#")[0]);
  const ORIGINAL_FILE_URL = L10N.getFormatStr(
    "netRequest.originalFileURL.tooltip",
    url
  );
  const DECODED_FILE_URL = L10N.getFormatStr(
    "netRequest.decodedFileURL.tooltip",
    unicodeUrl
  );
  const fileToolTip =
    url === unicodeUrl ? url : ORIGINAL_FILE_URL + "\n\n" + DECODED_FILE_URL;
  const requestedFile = requestItem.urlDetails.baseNameWithQuery;
  const host = getUnicodeHostname(getUrlHost(url));
  const scheme = getUrlScheme(url);
  const {
    remoteAddress,
    remotePort,
    totalTime,
    eventTimings = { timings: {} },
  } = requestItem;
  const formattedIPPort = getFormattedIPAndPort(remoteAddress, remotePort);
  const remoteIP = remoteAddress ? `${formattedIPPort}` : "unknown";
  const duration = getFormattedTime(totalTime);
  const latency = getFormattedTime(eventTimings.timings.wait);
  const protocol = getFormattedProtocol(requestItem);

  if (fuzzyUrl) {
    ok(
      requestItem.method.startsWith(method),
      "The attached method is correct."
    );
    ok(requestItem.url.startsWith(url), "The attached url is correct.");
  } else {
    is(requestItem.method, method, "The attached method is correct.");
    is(requestItem.url, url.split("#")[0], "The attached url is correct.");
  }

  is(
    target.querySelector(".requests-list-method").textContent,
    method,
    "The displayed method is correct."
  );

  if (fuzzyUrl) {
    ok(
      target
        .querySelector(".requests-list-file")
        .textContent.startsWith(requestedFile),
      "The displayed file is correct."
    );
    ok(
      target
        .querySelector(".requests-list-file")
        .getAttribute("title")
        .startsWith(fileToolTip),
      "The tooltip file is correct."
    );
  } else {
    is(
      target.querySelector(".requests-list-file").textContent,
      requestedFile,
      "The displayed file is correct."
    );
    is(
      target.querySelector(".requests-list-file").getAttribute("title"),
      fileToolTip,
      "The tooltip file is correct."
    );
  }

  is(
    target.querySelector(".requests-list-protocol").textContent,
    protocol,
    "The displayed protocol is correct."
  );

  is(
    target.querySelector(".requests-list-protocol").getAttribute("title"),
    protocol,
    "The tooltip protocol is correct."
  );

  is(
    target.querySelector(".requests-list-domain").textContent,
    host,
    "The displayed domain is correct."
  );

  const domainTooltip =
    host + (remoteAddress ? " (" + formattedIPPort + ")" : "");
  is(
    target.querySelector(".requests-list-domain").getAttribute("title"),
    domainTooltip,
    "The tooltip domain is correct."
  );

  is(
    target.querySelector(".requests-list-remoteip").textContent,
    remoteIP,
    "The displayed remote IP is correct."
  );

  is(
    target.querySelector(".requests-list-remoteip").getAttribute("title"),
    remoteIP,
    "The tooltip remote IP is correct."
  );

  is(
    target.querySelector(".requests-list-scheme").textContent,
    scheme,
    "The displayed scheme is correct."
  );

  is(
    target.querySelector(".requests-list-scheme").getAttribute("title"),
    scheme,
    "The tooltip scheme is correct."
  );

  is(
    target.querySelector(".requests-list-duration-time").textContent,
    duration,
    "The displayed duration is correct."
  );

  is(
    target.querySelector(".requests-list-duration-time").getAttribute("title"),
    duration,
    "The tooltip duration is correct."
  );

  is(
    target.querySelector(".requests-list-latency-time").textContent,
    latency,
    "The displayed latency is correct."
  );

  is(
    target.querySelector(".requests-list-latency-time").getAttribute("title"),
    latency,
    "The tooltip latency is correct."
  );

  if (status !== undefined) {
    const value = target
      .querySelector(".requests-list-status-code")
      .getAttribute("data-status-code");
    const codeValue = target.querySelector(".requests-list-status-code")
      .textContent;
    const tooltip = target
      .querySelector(".requests-list-status-code")
      .getAttribute("title");
    info("Displayed status: " + value);
    info("Displayed code: " + codeValue);
    info("Tooltip status: " + tooltip);
    is(
      value,
      displayedStatus ? displayedStatus : status,
      "The displayed status is correct."
    );
    is(codeValue, status, "The displayed status code is correct.");
    is(tooltip, status + " " + statusText, "The tooltip status is correct.");
  }
  if (cause !== undefined) {
    const value = Array.from(
      target.querySelector(".requests-list-initiator").childNodes
    )
      .filter(node => node.nodeType === Node.ELEMENT_NODE)
      .map(({ textContent }) => textContent)
      .join("");
    const tooltip = target
      .querySelector(".requests-list-initiator")
      .getAttribute("title");
    info("Displayed cause: " + value);
    info("Tooltip cause: " + tooltip);
    ok(value.includes(cause.type), "The displayed cause is correct.");
    ok(tooltip.includes(cause.type), "The tooltip cause is correct.");
  }
  if (type !== undefined) {
    const value = target.querySelector(".requests-list-type").textContent;
    const tooltip = target
      .querySelector(".requests-list-type")
      .getAttribute("title");
    info("Displayed type: " + value);
    info("Tooltip type: " + tooltip);
    is(value, type, "The displayed type is correct.");
    is(tooltip, fullMimeType, "The tooltip type is correct.");
  }
  if (transferred !== undefined) {
    const value = target.querySelector(".requests-list-transferred")
      .textContent;
    const tooltip = target
      .querySelector(".requests-list-transferred")
      .getAttribute("title");
    info("Displayed transferred size: " + value);
    info("Tooltip transferred size: " + tooltip);
    is(value, transferred, "The displayed transferred size is correct.");
    is(tooltip, transferred, "The tooltip transferred size is correct.");
  }
  if (size !== undefined) {
    const value = target.querySelector(".requests-list-size").textContent;
    const tooltip = target
      .querySelector(".requests-list-size")
      .getAttribute("title");
    info("Displayed size: " + value);
    info("Tooltip size: " + tooltip);
    is(value, size, "The displayed size is correct.");
    is(tooltip, size, "The tooltip size is correct.");
  }
  if (time !== undefined) {
    const value = target.querySelector(".requests-list-timings-total")
      .textContent;
    const tooltip = target
      .querySelector(".requests-list-timings-total")
      .getAttribute("title");
    info("Displayed time: " + value);
    info("Tooltip time: " + tooltip);
    ok(~~value.match(/[0-9]+/) >= 0, "The displayed time is correct.");
    ok(~~tooltip.match(/[0-9]+/) >= 0, "The tooltip time is correct.");
  }

  if (visibleIndex !== -1) {
    if (visibleIndex % 2 === 0) {
      ok(target.classList.contains("even"), "Item should have 'even' class.");
      ok(!target.classList.contains("odd"), "Item shouldn't have 'odd' class.");
    } else {
      ok(
        !target.classList.contains("even"),
        "Item shouldn't have 'even' class."
      );
      ok(target.classList.contains("odd"), "Item should have 'odd' class.");
    }
  }
}

/**
 * Helper function for waiting for an event to fire before resolving a promise.
 * Example: waitFor(aMonitor.panelWin.api, EVENT_NAME);
 *
 * @param object subject
 *        The event emitter object that is being listened to.
 * @param string eventName
 *        The name of the event to listen to.
 * @return object
 *        Returns a promise that resolves upon firing of the event.
 */
function waitFor(subject, eventName) {
  return new Promise(resolve => {
    subject.once(eventName, resolve);
  });
}

/**
 * Wait for an action of the provided type to be dispatched on the provided
 * store.
 *
 * @param {Object} store
 *        The redux store (wait-service middleware required).
 * @param {String} type
 *        Type of the action to wait for.
 */
function waitForDispatch(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => action.type === type,
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}

/**
 * Tests if a button for a filter of given type is the only one checked.
 *
 * @param string filterType
 *        The type of the filter that should be the only one checked.
 */
function testFilterButtons(monitor, filterType) {
  const doc = monitor.panelWin.document;
  const target = doc.querySelector(
    ".requests-list-filter-" + filterType + "-button"
  );
  ok(target, `Filter button '${filterType}' was found`);
  const buttons = [
    ...doc.querySelectorAll(".requests-list-filter-buttons button"),
  ];
  ok(buttons.length > 0, "More than zero filter buttons were found");

  // Only target should be checked.
  const checkStatus = buttons.map(button => (button == target ? 1 : 0));
  testFilterButtonsCustom(monitor, checkStatus);
}

/**
 * Tests if filter buttons have 'checked' attributes set correctly.
 *
 * @param array aIsChecked
 *        An array specifying if a button at given index should have a
 *        'checked' attribute. For example, if the third item of the array
 *        evaluates to true, the third button should be checked.
 */
function testFilterButtonsCustom(monitor, isChecked) {
  const doc = monitor.panelWin.document;
  const buttons = doc.querySelectorAll(".requests-list-filter-buttons button");
  for (let i = 0; i < isChecked.length; i++) {
    const button = buttons[i];
    if (isChecked[i]) {
      is(
        button.getAttribute("aria-pressed"),
        "true",
        "The " + button.id + " button should set 'aria-pressed' = true."
      );
    } else {
      is(
        button.getAttribute("aria-pressed"),
        "false",
        "The " + button.id + " button should set 'aria-pressed' = false."
      );
    }
  }
}

/**
 * Perform the specified requests in the context of the page content.
 *
 * @param Array requests
 *        An array of objects specifying the requests to perform. See
 *        shared/test/frame-script-utils.js for more information.
 *
 * @return A promise that resolves once the requests complete.
 */
function performRequestsInContent(requests) {
  info("Performing requests in the context of the content.");
  return executeInContent("devtools:test:xhr", requests);
}

/**
 * Send an async message to the frame script (chrome -> content) and wait for a
 * response message with the same name (content -> chrome).
 *
 * @param String name
 *        The message name. Should be one of the messages defined
 *        shared/test/frame-script-utils.js
 * @param Object data
 *        Optional data to send along
 * @param Object objects
 *        Optional CPOW objects to send along
 * @param Boolean expectResponse
 *        If set to false, don't wait for a response with the same name from the
 *        content script. Defaults to true.
 *
 * @return Promise
 *         Resolves to the response data if a response is expected, immediately
 *         resolves otherwise
 */
function executeInContent(
  name,
  data = {},
  objects = {},
  expectResponse = true
) {
  const mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  }
  return promise.resolve();
}

/**
 * Wait for a content -> chrome message on the message manager (the window
 * messagemanager is used).
 * @param {String} name The message name
 * @return {Promise} A promise that resolves to the response data when the
 * message has been received
 */
function waitForContentMessage(name) {
  const mm = gBrowser.selectedBrowser.messageManager;

  return new Promise(resolve => {
    mm.addMessageListener(name, function onMessage(msg) {
      mm.removeMessageListener(name, onMessage);
      resolve(msg);
    });
  });
}

function testColumnsAlignment(headers, requestList) {
  const firstRequestLine = requestList.childNodes[0];

  // Find number of columns
  const numberOfColumns = headers.childElementCount;
  for (let i = 0; i < numberOfColumns; i++) {
    const headerColumn = headers.childNodes[i];
    const requestColumn = firstRequestLine.childNodes[i];
    is(
      headerColumn.getBoundingClientRect().left,
      requestColumn.getBoundingClientRect().left,
      "Headers for columns number " + i + " are aligned."
    );
  }
}

async function hideColumn(monitor, column) {
  const { document } = monitor.panelWin;

  info(`Clicking context-menu item for ${column}`);
  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelector(".requests-list-headers")
  );

  const onHeaderRemoved = waitForDOM(
    document,
    `#requests-list-${column}-button`,
    0
  );
  getContextMenuItem(monitor, `request-list-header-${column}-toggle`).click();
  await onHeaderRemoved;

  ok(
    !document.querySelector(`#requests-list-${column}-button`),
    `Column ${column} should be hidden`
  );
}

async function showColumn(monitor, column) {
  const { document } = monitor.panelWin;

  info(`Clicking context-menu item for ${column}`);
  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    document.querySelector(".requests-list-headers")
  );

  const onHeaderAdded = waitForDOM(
    document,
    `#requests-list-${column}-button`,
    1
  );
  getContextMenuItem(monitor, `request-list-header-${column}-toggle`).click();
  await onHeaderAdded;

  ok(
    document.querySelector(`#requests-list-${column}-button`),
    `Column ${column} should be visible`
  );
}

/**
 * Select a request and switch to its response panel.
 *
 * @param {Number} index The request index to be selected
 */
async function selectIndexAndWaitForSourceEditor(monitor, index) {
  const { document } = monitor.panelWin;
  const onResponseContent = monitor.panelWin.api.once(
    TEST_EVENTS.RECEIVED_RESPONSE_CONTENT
  );
  // Select the request first, as it may try to fetch whatever is the current request's
  // responseContent if we select the ResponseTab first.
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[index]
  );
  // We may already be on the ResponseTab, so only select it if needed.
  const editor = document.querySelector("#response-panel .CodeMirror-code");
  if (!editor) {
    const waitDOM = waitForDOM(document, "#response-panel .CodeMirror-code");
    document.querySelector("#response-tab").click();
    await waitDOM;
  }
  await onResponseContent;
}

/**
 * Helper function for executing XHRs on a test page.
 *
 * @param {Number} count Number of requests to be executed.
 */
async function performRequests(monitor, tab, count) {
  const wait = waitForNetworkEvents(monitor, count);
  await ContentTask.spawn(tab.linkedBrowser, count, requestCount => {
    content.wrappedJSObject.performRequests(requestCount);
  });
  await wait;
}

/**
 * Helper function for retrieving `.CodeMirror` content
 */
function getCodeMirrorValue(monitor) {
  const { document } = monitor.panelWin;
  return document.querySelector(".CodeMirror").CodeMirror.getValue();
}

/**
 * Helper function opening the options menu
 */
function openSettingsMenu(monitor) {
  const { document } = monitor.panelWin;
  document.querySelector(".netmonitor-settings-menu-button").click();
}

function clickSettingsMenuItem(monitor, itemKey) {
  openSettingsMenu(monitor);
  const node = getSettingsMenuItem(monitor, itemKey);
  node?.click();
}

function getSettingsMenuItem(monitor, itemKey) {
  const { document } = monitor.panelWin;
  return document.querySelector(SETTINGS_MENU_ITEMS[itemKey]);
}

/**
 * Wait for lazy fields to be loaded in a request.
 *
 * @param Object Store redux store containing request list.
 * @param array fields array of strings which contain field names to be checked
 * on the request.
 */
function waitForRequestData(store, fields, id) {
  return waitUntil(() => {
    let item;
    if (id) {
      item = getRequestById(store.getState(), id);
    } else {
      item = getSortedRequests(store.getState())[0];
    }
    if (!item) {
      return false;
    }
    for (const field of fields) {
      if (!item[field]) {
        return false;
      }
    }
    return true;
  });
}

// Telemetry

/**
 * Helper for verifying telemetry event.
 *
 * @param Object expectedEvent object representing expected event data.
 * @param Object query fields specifying category, method and object
 *                     of the target telemetry event.
 */
function checkTelemetryEvent(expectedEvent, query) {
  const events = queryTelemetryEvents(query);
  is(events.length, 1, "There was only 1 event logged");

  const [event] = events;
  ok(event.session_id > 0, "There is a valid session_id in the logged event");

  const f = e => JSON.stringify(e, null, 2);
  is(
    f(event),
    f({
      ...expectedEvent,
      session_id: event.session_id,
    }),
    "The event has the expected data"
  );
}

function queryTelemetryEvents(query) {
  const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;
  const snapshot = Services.telemetry.snapshotEvents(ALL_CHANNELS, true);
  const category = query.category || "devtools.main";
  const object = query.object || "netmonitor";

  const filtersChangedEvents = snapshot.parent.filter(
    event =>
      event[1] === category && event[2] === query.method && event[3] === object
  );

  // Return the `extra` field (which is event[5]e).
  return filtersChangedEvents.map(event => event[5]);
}

function validateRequests(requests, monitor) {
  const { document, store, windowRequire } = monitor.panelWin;

  const { getDisplayedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  requests.forEach((spec, i) => {
    const { method, url, causeType, causeUri, stack } = spec;

    const requestItem = getSortedRequests(store.getState())[i];
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      requestItem,
      method,
      url,
      { cause: { type: causeType, loadingDocumentUri: causeUri } }
    );

    const { stacktrace } = requestItem;
    const stackLen = stacktrace ? stacktrace.length : 0;

    if (stack) {
      ok(stacktrace, `Request #${i} has a stacktrace`);
      ok(
        stackLen > 0,
        `Request #${i} (${causeType}) has a stacktrace with ${stackLen} items`
      );

      // if "stack" is array, check the details about the top stack frames
      if (Array.isArray(stack)) {
        stack.forEach((frame, j) => {
          is(
            stacktrace[j].functionName,
            frame.fn,
            `Request #${i} has the correct function on JS stack frame #${j}`
          );
          is(
            stacktrace[j].filename.split("/").pop(),
            frame.file,
            `Request #${i} has the correct file on JS stack frame #${j}`
          );
          is(
            stacktrace[j].lineNumber,
            frame.line,
            `Request #${i} has the correct line number on JS stack frame #${j}`
          );
          is(
            stacktrace[j].asyncCause,
            frame.asyncCause,
            `Request #${i} has the correct async cause on JS stack frame #${j}`
          );
        });
      }
    } else {
      is(stackLen, 0, `Request #${i} (${causeType}) has an empty stacktrace`);
    }
  });
}

/**
 * Retrieve the context menu element corresponding to the provided id, for the provided
 * netmonitor instance.
 */
function getContextMenuItem(monitor, id) {
  const Menu = require("devtools/client/framework/menu");
  return Menu.getMenuElementById(id, monitor.panelWin.document);
}

/**
 * Wait for DOM being in specific state. But, do not wait
 * for change if it's in the expected state already.
 */
async function waitForDOMIfNeeded(target, selector, expectedLength = 1) {
  return new Promise(resolve => {
    const elements = target.querySelectorAll(selector);
    if (elements.length == expectedLength) {
      resolve(elements);
    } else {
      waitForDOM(target, selector, expectedLength).then(elems => {
        resolve(elems);
      });
    }
  });
}

/**
 * Helper for blocking or unblocking a request via the list item's context menu.
 *
 * @param {Element} element
 *        Target request list item to be right clicked to bring up its context menu.
 * @param {Object} monitor
 *        The netmonitor instance used for retrieving a context menu element.
 * @param {Object} store
 *        The redux store (wait-service middleware required).
 * @param {String} action
 *        The action, block or unblock, to construct a corresponding context menu id.
 */
async function toggleBlockedUrl(element, monitor, store, action = "block") {
  EventUtils.sendMouseEvent({ type: "contextmenu" }, element);
  const contextMenuId = `request-list-context-${action}-url`;
  const contextBlockToggle = getContextMenuItem(monitor, contextMenuId);
  const onRequestComplete = waitForDispatch(
    store,
    "REQUEST_BLOCKING_UPDATE_COMPLETE"
  );
  contextBlockToggle.click();

  info(`Wait for selected request to be ${action}ed`);
  await onRequestComplete;
  info(`Selected request is now ${action}ed`);
}

/**
 * Find and click an element
 *
 * @param {Element} element
 *        Target element to be clicked
 * @param {Object} monitor
 *        The netmonitor instance used for retrieving the window.
 */

function clickElement(element, monitor) {
  EventUtils.synthesizeMouseAtCenter(element, {}, monitor.panelWin);
}

/**
 * Predicates used when sorting items.
 *
 * @param object first
 *        The first item used in the comparison.
 * @param object second
 *        The second item used in the comparison.
 * @return number
 *         <0 to sort first to a lower index than second
 *         =0 to leave first and second unchanged with respect to each other
 *         >0 to sort second to a lower index than first
 */

function compareValues(first, second) {
  if (first === second) {
    return 0;
  }
  return first > second ? 1 : -1;
}
