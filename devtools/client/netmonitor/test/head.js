/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);

var { Toolbox } = require("devtools/client/framework/toolbox");
const {
  decodeUnicodeUrl,
  getUrlBaseName,
  getUrlQuery,
  getUrlHost,
} = require("devtools/client/netmonitor/request-utils");

const EXAMPLE_URL = "http://example.com/browser/devtools/client/netmonitor/test/";
const HTTPS_EXAMPLE_URL = "https://example.com/browser/devtools/client/netmonitor/test/";

const API_CALLS_URL = EXAMPLE_URL + "html_api-calls-test-page.html";
const SIMPLE_URL = EXAMPLE_URL + "html_simple-test-page.html";
const NAVIGATE_URL = EXAMPLE_URL + "html_navigate-test-page.html";
const CONTENT_TYPE_URL = EXAMPLE_URL + "html_content-type-test-page.html";
const CONTENT_TYPE_WITHOUT_CACHE_URL = EXAMPLE_URL + "html_content-type-without-cache-test-page.html";
const CONTENT_TYPE_WITHOUT_CACHE_REQUESTS = 8;
const CYRILLIC_URL = EXAMPLE_URL + "html_cyrillic-test-page.html";
const STATUS_CODES_URL = EXAMPLE_URL + "html_status-codes-test-page.html";
const POST_DATA_URL = EXAMPLE_URL + "html_post-data-test-page.html";
const POST_JSON_URL = EXAMPLE_URL + "html_post-json-test-page.html";
const POST_RAW_URL = EXAMPLE_URL + "html_post-raw-test-page.html";
const POST_RAW_WITH_HEADERS_URL = EXAMPLE_URL + "html_post-raw-with-headers-test-page.html";
const PARAMS_URL = EXAMPLE_URL + "html_params-test-page.html";
const JSONP_URL = EXAMPLE_URL + "html_jsonp-test-page.html";
const JSON_LONG_URL = EXAMPLE_URL + "html_json-long-test-page.html";
const JSON_MALFORMED_URL = EXAMPLE_URL + "html_json-malformed-test-page.html";
const JSON_CUSTOM_MIME_URL = EXAMPLE_URL + "html_json-custom-mime-test-page.html";
const JSON_TEXT_MIME_URL = EXAMPLE_URL + "html_json-text-mime-test-page.html";
const JSON_B64_URL = EXAMPLE_URL + "html_json-b64.html";
const JSON_BASIC_URL = EXAMPLE_URL + "html_json-basic.html";
const SORTING_URL = EXAMPLE_URL + "html_sorting-test-page.html";
const FILTERING_URL = EXAMPLE_URL + "html_filter-test-page.html";
const INFINITE_GET_URL = EXAMPLE_URL + "html_infinite-get-page.html";
const CUSTOM_GET_URL = EXAMPLE_URL + "html_custom-get-page.html";
const SINGLE_GET_URL = EXAMPLE_URL + "html_single-get-page.html";
const STATISTICS_URL = EXAMPLE_URL + "html_statistics-test-page.html";
const CURL_URL = EXAMPLE_URL + "html_copy-as-curl.html";
const CURL_UTILS_URL = EXAMPLE_URL + "html_curl-utils.html";
const SEND_BEACON_URL = EXAMPLE_URL + "html_send-beacon.html";
const CORS_URL = EXAMPLE_URL + "html_cors-test-page.html";

const SIMPLE_SJS = EXAMPLE_URL + "sjs_simple-test-server.sjs";
const CONTENT_TYPE_SJS = EXAMPLE_URL + "sjs_content-type-test-server.sjs";
const HTTPS_CONTENT_TYPE_SJS = HTTPS_EXAMPLE_URL + "sjs_content-type-test-server.sjs";
const STATUS_CODES_SJS = EXAMPLE_URL + "sjs_status-codes-test-server.sjs";
const SORTING_SJS = EXAMPLE_URL + "sjs_sorting-test-server.sjs";
const HTTPS_REDIRECT_SJS = EXAMPLE_URL + "sjs_https-redirect-test-server.sjs";
const CORS_SJS_PATH = "/browser/devtools/client/netmonitor/test/sjs_cors-test-server.sjs";
const HSTS_SJS = EXAMPLE_URL + "sjs_hsts-test-server.sjs";

const HSTS_BASE_URL = EXAMPLE_URL;
const HSTS_PAGE_URL = CUSTOM_GET_URL;

const TEST_IMAGE = EXAMPLE_URL + "test-image.png";
const TEST_IMAGE_DATA_URI = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg==";

const FRAME_SCRIPT_UTILS_URL = "chrome://devtools/content/shared/frame-script-utils.js";

// All tests are asynchronous.
waitForExplicitFinish();

const gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
// To enable logging for try runs, just set the pref to true.
Services.prefs.setBoolPref("devtools.debugger.log", false);

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

// Always reset some prefs to their original values after the test finishes.
const gDefaultFilters = Services.prefs.getCharPref("devtools.netmonitor.filters");

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");

  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setCharPref("devtools.netmonitor.filters", gDefaultFilters);
  Services.prefs.clearUserPref("devtools.cache.disabled");
});

function waitForNavigation(aTarget) {
  let deferred = promise.defer();
  aTarget.once("will-navigate", () => {
    aTarget.once("navigate", () => {
      deferred.resolve();
    });
  });
  return deferred.promise;
}

function reconfigureTab(aTarget, aOptions) {
  let deferred = promise.defer();
  aTarget.activeTab.reconfigure(aOptions, deferred.resolve);
  return deferred.promise;
}

function toggleCache(aTarget, aDisabled) {
  let options = { cacheDisabled: aDisabled, performReload: true };
  let navigationFinished = waitForNavigation(aTarget);

  // Disable the cache for any toolbox that it is opened from this point on.
  Services.prefs.setBoolPref("devtools.cache.disabled", aDisabled);

  return reconfigureTab(aTarget, options).then(() => navigationFinished);
}

function initNetMonitor(aUrl, aWindow, aEnableCache) {
  info("Initializing a network monitor pane.");

  return Task.spawn(function* () {
    let tab = yield addTab(aUrl);
    info("Net tab added successfully: " + aUrl);

    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();
    info("Target remoted.");

    if (!aEnableCache) {
      info("Disabling cache and reloading page.");
      yield toggleCache(target, true);
      info("Cache disabled when the current and all future toolboxes are open.");
      // Remove any requests generated by the reload while toggling the cache to
      // avoid interfering with the test.
      isnot([...target.activeConsole.getNetworkEvents()].length, 0,
         "Request to reconfigure the tab was recorded.");
      target.activeConsole.clearNetworkRequests();
    }

    let toolbox = yield gDevTools.showToolbox(target, "netmonitor");
    info("Network monitor pane shown successfully.");

    let monitor = toolbox.getCurrentPanel();
    return {tab, monitor};
  });
}

function restartNetMonitor(monitor, newUrl) {
  info("Restarting the specified network monitor.");

  return Task.spawn(function* () {
    let tab = monitor.target.tab;
    let url = newUrl || tab.linkedBrowser.currentURI.spec;

    let onDestroyed = monitor.once("destroyed");
    yield removeTab(tab);
    yield onDestroyed;

    return initNetMonitor(url);
  });
}

function teardown(monitor) {
  info("Destroying the specified network monitor.");

  return Task.spawn(function* () {
    let tab = monitor.target.tab;

    let onDestroyed = monitor.once("destroyed");
    yield removeTab(tab);
    yield onDestroyed;
  });
}

function waitForNetworkEvents(aMonitor, aGetRequests, aPostRequests = 0) {
  let deferred = promise.defer();

  let panel = aMonitor.panelWin;
  let events = panel.EVENTS;

  let progress = {};
  let genericEvents = 0;
  let postEvents = 0;

  let awaitedEventsToListeners = [
    ["UPDATING_REQUEST_HEADERS", onGenericEvent],
    ["RECEIVED_REQUEST_HEADERS", onGenericEvent],
    ["UPDATING_REQUEST_COOKIES", onGenericEvent],
    ["RECEIVED_REQUEST_COOKIES", onGenericEvent],
    ["UPDATING_REQUEST_POST_DATA", onPostEvent],
    ["RECEIVED_REQUEST_POST_DATA", onPostEvent],
    ["UPDATING_RESPONSE_HEADERS", onGenericEvent],
    ["RECEIVED_RESPONSE_HEADERS", onGenericEvent],
    ["UPDATING_RESPONSE_COOKIES", onGenericEvent],
    ["RECEIVED_RESPONSE_COOKIES", onGenericEvent],
    ["STARTED_RECEIVING_RESPONSE", onGenericEvent],
    ["UPDATING_RESPONSE_CONTENT", onGenericEvent],
    ["RECEIVED_RESPONSE_CONTENT", onGenericEvent],
    ["UPDATING_EVENT_TIMINGS", onGenericEvent],
    ["RECEIVED_EVENT_TIMINGS", onGenericEvent]
  ];

  function initProgressForURL(url) {
    if (progress[url]) return;
    progress[url] = {};
    awaitedEventsToListeners.forEach(([e]) => progress[url][e] = 0);
  }

  function updateProgressForURL(url, event) {
    initProgressForURL(url);
    progress[url][Object.keys(events).find(e => events[e] == event)] = 1;
  }

  function onGenericEvent(event, actor) {
    genericEvents++;
    maybeResolve(event, actor);
  }

  function onPostEvent(event, actor) {
    postEvents++;
    maybeResolve(event, actor);
  }

  function maybeResolve(event, actor) {
    info("> Network events progress: " +
      genericEvents + "/" + ((aGetRequests + aPostRequests) * 13) + ", " +
      postEvents + "/" + (aPostRequests * 2) + ", " +
      "got " + event + " for " + actor);

    let networkInfo =
      panel.NetMonitorController.webConsoleClient.getNetworkRequest(actor);
    let url = networkInfo.request.url;
    updateProgressForURL(url, event);

    // Uncomment this to get a detailed progress logging (when debugging a test)
    // info("> Current state: " + JSON.stringify(progress, null, 2));

    // There are 15 updates which need to be fired for a request to be
    // considered finished. The "requestPostData" packet isn't fired for
    // non-POST requests.
    if (genericEvents >= (aGetRequests + aPostRequests) * 13 &&
        postEvents >= aPostRequests * 2) {

      awaitedEventsToListeners.forEach(([e, l]) => panel.off(events[e], l));
      executeSoon(deferred.resolve);
    }
  }

  awaitedEventsToListeners.forEach(([e, l]) => panel.on(events[e], l));
  return deferred.promise;
}

/**
 * Convert a store record (model) to the rendered element. Tests that need to use
 * this should be rewritten - test the rendered markup at unit level, integration
 * mochitest should check only the store state.
 */
function getItemTarget(requestList, requestItem) {
  const items = requestList.mountPoint.querySelectorAll(".request-list-item");
  return [...items].find(el => el.dataset.id == requestItem.id);
}

function verifyRequestItemTarget(requestList, requestItem, aMethod, aUrl, aData = {}) {
  info("> Verifying: " + aMethod + " " + aUrl + " " + aData.toSource());
  // This bloats log sizes significantly in automation (bug 992485)
  // info("> Request: " + requestItem.toSource());

  let visibleIndex = requestList.visibleItems.indexOf(requestItem);

  info("Visible index of item: " + visibleIndex);

  let { fuzzyUrl, status, statusText, cause, type, fullMimeType,
        transferred, size, time, displayedStatus } = aData;

  let target = getItemTarget(requestList, requestItem);

  let unicodeUrl = decodeUnicodeUrl(aUrl);
  let name = getUrlBaseName(aUrl);
  let query = getUrlQuery(aUrl);
  let hostPort = getUrlHost(aUrl);
  let remoteAddress = requestItem.remoteAddress;

  if (fuzzyUrl) {
    ok(requestItem.method.startsWith(aMethod), "The attached method is correct.");
    ok(requestItem.url.startsWith(aUrl), "The attached url is correct.");
  } else {
    is(requestItem.method, aMethod, "The attached method is correct.");
    is(requestItem.url, aUrl, "The attached url is correct.");
  }

  is(target.querySelector(".requests-menu-method").textContent,
    aMethod, "The displayed method is correct.");

  if (fuzzyUrl) {
    ok(target.querySelector(".requests-menu-file").textContent.startsWith(
      name + (query ? "?" + query : "")), "The displayed file is correct.");
    ok(target.querySelector(".requests-menu-file").getAttribute("title").startsWith(unicodeUrl),
      "The tooltip file is correct.");
  } else {
    is(target.querySelector(".requests-menu-file").textContent,
      name + (query ? "?" + query : ""), "The displayed file is correct.");
    is(target.querySelector(".requests-menu-file").getAttribute("title"),
      unicodeUrl, "The tooltip file is correct.");
  }

  is(target.querySelector(".requests-menu-domain").textContent,
    hostPort, "The displayed domain is correct.");

  let domainTooltip = hostPort + (remoteAddress ? " (" + remoteAddress + ")" : "");
  is(target.querySelector(".requests-menu-domain").getAttribute("title"),
    domainTooltip, "The tooltip domain is correct.");

  if (status !== undefined) {
    let value = target.querySelector(".requests-menu-status-icon").getAttribute("data-code");
    let codeValue = target.querySelector(".requests-menu-status-code").textContent;
    let tooltip = target.querySelector(".requests-menu-status").getAttribute("title");
    info("Displayed status: " + value);
    info("Displayed code: " + codeValue);
    info("Tooltip status: " + tooltip);
    is(value, displayedStatus ? displayedStatus : status, "The displayed status is correct.");
    is(codeValue, status, "The displayed status code is correct.");
    is(tooltip, status + " " + statusText, "The tooltip status is correct.");
  }
  if (cause !== undefined) {
    let value = target.querySelector(".requests-menu-cause > .subitem-label").textContent;
    let tooltip = target.querySelector(".requests-menu-cause").getAttribute("title");
    info("Displayed cause: " + value);
    info("Tooltip cause: " + tooltip);
    is(value, cause.type, "The displayed cause is correct.");
    is(tooltip, cause.loadingDocumentUri, "The tooltip cause is correct.")
  }
  if (type !== undefined) {
    let value = target.querySelector(".requests-menu-type").textContent;
    let tooltip = target.querySelector(".requests-menu-type").getAttribute("title");
    info("Displayed type: " + value);
    info("Tooltip type: " + tooltip);
    is(value, type, "The displayed type is correct.");
    is(tooltip, fullMimeType, "The tooltip type is correct.");
  }
  if (transferred !== undefined) {
    let value = target.querySelector(".requests-menu-transferred").textContent;
    let tooltip = target.querySelector(".requests-menu-transferred").getAttribute("title");
    info("Displayed transferred size: " + value);
    info("Tooltip transferred size: " + tooltip);
    is(value, transferred, "The displayed transferred size is correct.");
    is(tooltip, transferred, "The tooltip transferred size is correct.");
  }
  if (size !== undefined) {
    let value = target.querySelector(".requests-menu-size").textContent;
    let tooltip = target.querySelector(".requests-menu-size").getAttribute("title");
    info("Displayed size: " + value);
    info("Tooltip size: " + tooltip);
    is(value, size, "The displayed size is correct.");
    is(tooltip, size, "The tooltip size is correct.");
  }
  if (time !== undefined) {
    let value = target.querySelector(".requests-menu-timings-total").textContent;
    let tooltip = target.querySelector(".requests-menu-timings-total").getAttribute("title");
    info("Displayed time: " + value);
    info("Tooltip time: " + tooltip);
    ok(~~(value.match(/[0-9]+/)) >= 0, "The displayed time is correct.");
    ok(~~(tooltip.match(/[0-9]+/)) >= 0, "The tooltip time is correct.");
  }

  if (visibleIndex != -1) {
    if (visibleIndex % 2 == 0) {
      ok(target.classList.contains("even"), "Item should have 'even' class.");
      ok(!target.classList.contains("odd"), "Item shouldn't have 'odd' class.");
    } else {
      ok(!target.classList.contains("even"), "Item shouldn't have 'even' class.");
      ok(target.classList.contains("odd"), "Item should have 'odd' class.");
    }
  }
}

/**
 * Helper function for waiting for an event to fire before resolving a promise.
 * Example: waitFor(aMonitor.panelWin, aMonitor.panelWin.EVENTS.TAB_UPDATED);
 *
 * @param object subject
 *        The event emitter object that is being listened to.
 * @param string eventName
 *        The name of the event to listen to.
 * @return object
 *        Returns a promise that resolves upon firing of the event.
 */
function waitFor(subject, eventName) {
  let deferred = promise.defer();
  subject.once(eventName, deferred.resolve);
  return deferred.promise;
}

/**
 * Tests if a button for a filter of given type is the only one checked.
 *
 * @param string filterType
 *        The type of the filter that should be the only one checked.
 */
function testFilterButtons(monitor, filterType) {
  let doc = monitor.panelWin.document;
  let target = doc.querySelector("#requests-menu-filter-" + filterType + "-button");
  ok(target, `Filter button '${filterType}' was found`);
  let buttons = [...doc.querySelectorAll(".menu-filter-button")];
  ok(buttons.length > 0, "More than zero filter buttons were found");

  // Only target should be checked.
  let checkStatus = buttons.map(button => button == target ? 1 : 0);
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
function testFilterButtonsCustom(aMonitor, aIsChecked) {
  let doc = aMonitor.panelWin.document;
  let buttons = doc.querySelectorAll(".menu-filter-button");
  for (let i = 0; i < aIsChecked.length; i++) {
    let button = buttons[i];
    if (aIsChecked[i]) {
      is(button.classList.contains("checked"), true,
        "The " + button.id + " button should have a 'checked' class.");
      is(button.getAttribute("aria-pressed"), "true",
        "The " + button.id + " button should set 'aria-pressed' = true.");
    } else {
      is(button.classList.contains("checked"), false,
        "The " + button.id + " button should not have a 'checked' class.");
      is(button.getAttribute("aria-pressed"), "false",
        "The " + button.id + " button should set 'aria-pressed' = false.");
    }
  }
}

/**
 * Loads shared/frame-script-utils.js in the specified tab.
 *
 * @param tab
 *        Optional tab to load the frame script in. Defaults to the current tab.
 */
function loadCommonFrameScript(tab) {
  let browser = tab ? tab.linkedBrowser : gBrowser.selectedBrowser;

  browser.messageManager.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
}

/**
 * Perform the specified requests in the context of the page content.
 *
 * @param Array requests
 *        An array of objects specifying the requests to perform. See
 *        shared/frame-script-utils.js for more information.
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
 *        shared/frame-script-utils.js
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
function executeInContent(name, data = {}, objects = {}, expectResponse = true) {
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  } else {
    return promise.resolve();
  }
}

/**
 * Wait for a content -> chrome message on the message manager (the window
 * messagemanager is used).
 * @param {String} name The message name
 * @return {Promise} A promise that resolves to the response data when the
 * message has been received
 */
function waitForContentMessage(name) {
  let mm = gBrowser.selectedBrowser.messageManager;

  let def = promise.defer();
  mm.addMessageListener(name, function onMessage(msg) {
    mm.removeMessageListener(name, onMessage);
    def.resolve(msg);
  });
  return def.promise;
}

/**
 * Open the requestMenu menu and return all of it's items in a flat array
 * @param {netmonitorPanel} netmonitor
 * @param {Event} event mouse event with screenX and screenX coordinates
 * @return An array of MenuItems
 */
function openContextMenuAndGetAllItems(netmonitor, event) {
  let menu = netmonitor.RequestsMenu.contextMenu.open(event);

  // Flatten all menu items into a single array to make searching through it easier
  let allItems = [].concat.apply([], menu.items.map(function addItem(item) {
    if (item.submenu) {
      return addItem(item.submenu.items);
    }
    return item;
  }));

  return allItems;
}
