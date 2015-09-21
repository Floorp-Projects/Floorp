/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
var { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
var { gDevTools } = Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm", {});
var { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var { CurlUtils } = Cu.import("resource:///modules/devtools/client/shared/Curl.jsm", {});
var promise = require("promise");
var NetworkHelper = require("devtools/shared/webconsole/network-helper");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");

const EXAMPLE_URL = "http://example.com/browser/devtools/client/netmonitor/test/";

const API_CALLS_URL = EXAMPLE_URL + "html_api-calls-test-page.html";
const SIMPLE_URL = EXAMPLE_URL + "html_simple-test-page.html";
const NAVIGATE_URL = EXAMPLE_URL + "html_navigate-test-page.html";
const CONTENT_TYPE_URL = EXAMPLE_URL + "html_content-type-test-page.html";
const CONTENT_TYPE_WITHOUT_CACHE_URL = EXAMPLE_URL + "html_content-type-without-cache-test-page.html";
const CYRILLIC_URL = EXAMPLE_URL + "html_cyrillic-test-page.html";
const STATUS_CODES_URL = EXAMPLE_URL + "html_status-codes-test-page.html";
const POST_DATA_URL = EXAMPLE_URL + "html_post-data-test-page.html";
const POST_RAW_URL = EXAMPLE_URL + "html_post-raw-test-page.html";
const POST_RAW_WITH_HEADERS_URL = EXAMPLE_URL + "html_post-raw-with-headers-test-page.html";
const PARAMS_URL = EXAMPLE_URL + "html_params-test-page.html";
const JSONP_URL = EXAMPLE_URL + "html_jsonp-test-page.html";
const JSON_LONG_URL = EXAMPLE_URL + "html_json-long-test-page.html";
const JSON_MALFORMED_URL = EXAMPLE_URL + "html_json-malformed-test-page.html";
const JSON_CUSTOM_MIME_URL = EXAMPLE_URL + "html_json-custom-mime-test-page.html";
const JSON_TEXT_MIME_URL = EXAMPLE_URL + "html_json-text-mime-test-page.html";
const SORTING_URL = EXAMPLE_URL + "html_sorting-test-page.html";
const FILTERING_URL = EXAMPLE_URL + "html_filter-test-page.html";
const INFINITE_GET_URL = EXAMPLE_URL + "html_infinite-get-page.html";
const CUSTOM_GET_URL = EXAMPLE_URL + "html_custom-get-page.html";
const SINGLE_GET_URL = EXAMPLE_URL + "html_single-get-page.html";
const STATISTICS_URL = EXAMPLE_URL + "html_statistics-test-page.html";
const CURL_URL = EXAMPLE_URL + "html_copy-as-curl.html";
const CURL_UTILS_URL = EXAMPLE_URL + "html_curl-utils.html";
const SEND_BEACON_URL = EXAMPLE_URL + "html_send-beacon.html";

const SIMPLE_SJS = EXAMPLE_URL + "sjs_simple-test-server.sjs";
const CONTENT_TYPE_SJS = EXAMPLE_URL + "sjs_content-type-test-server.sjs";
const STATUS_CODES_SJS = EXAMPLE_URL + "sjs_status-codes-test-server.sjs";
const SORTING_SJS = EXAMPLE_URL + "sjs_sorting-test-server.sjs";
const HTTPS_REDIRECT_SJS = EXAMPLE_URL + "sjs_https-redirect-test-server.sjs";
const CORS_SJS_PATH = "/browser/devtools/client/netmonitor/test/sjs_cors-test-server.sjs";

const TEST_IMAGE = EXAMPLE_URL + "test-image.png";
const TEST_IMAGE_DATA_URI = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg==";

const FRAME_SCRIPT_UTILS_URL = "chrome://devtools/content/shared/frame-script-utils.js"

DevToolsUtils.testing = true;
SimpleTest.registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

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
  Services.prefs.clearUserPref("devtools.dump.emit");
});

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  let deferred = promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
  let browser = tab.linkedBrowser;

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetBrowser.removeTab(aTab);
}

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
};

function toggleCache(aTarget, aDisabled) {
  let options = { cacheDisabled: aDisabled, performReload: true };
  let navigationFinished = waitForNavigation(aTarget);

  // Disable the cache for any toolbox that it is opened from this point on.
  Services.prefs.setBoolPref("devtools.cache.disabled", aDisabled);

  return reconfigureTab(aTarget, options).then(() => navigationFinished);
}

function initNetMonitor(aUrl, aWindow, aEnableCache) {
  info("Initializing a network monitor pane.");

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    info("Net tab added successfully: " + aUrl);

    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();
    info("Target remoted.");

    if(!aEnableCache) {
      yield toggleCache(target, true);
      info("Cache disabled when the current and all future toolboxes are open.");
      // Remove any requests generated by the reload while toggling the cache to
      // avoid interfering with the test.
      isnot([...target.activeConsole.getNetworkEvents()].length, 0,
         "Request to reconfigure the tab was recorded.");
      target.activeConsole.clearNetworkRequests();
    }

    let toolbox = yield gDevTools.showToolbox(target, "netmonitor");
    info("Netork monitor pane shown successfully.");

    let debuggee = tab.linkedBrowser.contentWindow.wrappedJSObject;
    let monitor = toolbox.getCurrentPanel();
    return [tab, debuggee, monitor];
  });
}

function restartNetMonitor(aMonitor, aNewUrl) {
  info("Restarting the specified network monitor.");

  let deferred = promise.defer();
  let tab = aMonitor.target.tab;
  let url = aNewUrl || tab.linkedBrowser.contentWindow.wrappedJSObject.location.href;

  aMonitor.once("destroyed", () => initNetMonitor(url).then(deferred.resolve));
  removeTab(tab);

  return deferred.promise;
}

function teardown(aMonitor) {
  info("Destroying the specified network monitor.");

  let deferred = promise.defer();
  let tab = aMonitor.target.tab;

  aMonitor.once("destroyed", () => executeSoon(deferred.resolve));
  removeTab(tab);

  return deferred.promise;
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
      panel.NetMonitorController.webConsoleClient.getNetworkRequest(actor)
    let url = networkInfo.request.url;
    updateProgressForURL(url, event);

    info("> Current state: " + JSON.stringify(progress, null, 2));

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

function verifyRequestItemTarget(aRequestItem, aMethod, aUrl, aData = {}) {
  info("> Verifying: " + aMethod + " " + aUrl + " " + aData.toSource());
  // This bloats log sizes significantly in automation (bug 992485)
  //info("> Request: " + aRequestItem.attachment.toSource());

  let requestsMenu = aRequestItem.ownerView;
  let widgetIndex = requestsMenu.indexOfItem(aRequestItem);
  let visibleIndex = requestsMenu.visibleItems.indexOf(aRequestItem);

  info("Widget index of item: " + widgetIndex);
  info("Visible index of item: " + visibleIndex);

  let { fuzzyUrl, status, statusText, type, fullMimeType,
        transferred, size, time, fromCache } = aData;
  let { attachment, target } = aRequestItem

  let uri = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
  let unicodeUrl = NetworkHelper.convertToUnicode(unescape(aUrl));
  let name = NetworkHelper.convertToUnicode(unescape(uri.fileName || uri.filePath || "/"));
  let query = NetworkHelper.convertToUnicode(unescape(uri.query));
  let hostPort = uri.hostPort;
  let remoteAddress = attachment.remoteAddress;

  if (fuzzyUrl) {
    ok(attachment.method.startsWith(aMethod), "The attached method is correct.");
    ok(attachment.url.startsWith(aUrl), "The attached url is correct.");
  } else {
    is(attachment.method, aMethod, "The attached method is correct.");
    is(attachment.url, aUrl, "The attached url is correct.");
  }

  is(target.querySelector(".requests-menu-method").getAttribute("value"),
    aMethod, "The displayed method is correct.");

  if (fuzzyUrl) {
    ok(target.querySelector(".requests-menu-file").getAttribute("value").startsWith(
      name + (query ? "?" + query : "")), "The displayed file is correct.");
    ok(target.querySelector(".requests-menu-file").getAttribute("tooltiptext").startsWith(unicodeUrl),
      "The tooltip file is correct.");
  } else {
    is(target.querySelector(".requests-menu-file").getAttribute("value"),
      name + (query ? "?" + query : ""), "The displayed file is correct.");
    is(target.querySelector(".requests-menu-file").getAttribute("tooltiptext"),
      unicodeUrl, "The tooltip file is correct.");
  }

  is(target.querySelector(".requests-menu-domain").getAttribute("value"),
    hostPort, "The displayed domain is correct.");

  let domainTooltip = hostPort + (remoteAddress ? " (" + remoteAddress + ")" : "");
  is(target.querySelector(".requests-menu-domain").getAttribute("tooltiptext"),
    domainTooltip, "The tooltip domain is correct.");

  if (status !== undefined) {
    let value = target.querySelector(".requests-menu-status").getAttribute("code");
    let codeValue = target.querySelector(".requests-menu-status-code").getAttribute("value");
    let tooltip = target.querySelector(".requests-menu-status-and-method").getAttribute("tooltiptext");
    info("Displayed status: " + value);
    info("Displayed code: " + codeValue);
    info("Tooltip status: " + tooltip);
    is(value, fromCache ? "cached" : status, "The displayed status is correct.");
    is(codeValue, status, "The displayed status code is correct.");
    is(tooltip, status + " " + statusText, "The tooltip status is correct.");
  }
  if (type !== undefined) {
    let value = target.querySelector(".requests-menu-type").getAttribute("value");
    let tooltip = target.querySelector(".requests-menu-type").getAttribute("tooltiptext");
    info("Displayed type: " + value);
    info("Tooltip type: " + tooltip);
    is(value, type, "The displayed type is correct.");
    is(tooltip, fullMimeType, "The tooltip type is correct.");
  }
  if (transferred !== undefined) {
    let value = target.querySelector(".requests-menu-transferred").getAttribute("value");
    let tooltip = target.querySelector(".requests-menu-transferred").getAttribute("tooltiptext");
    info("Displayed transferred size: " + value);
    info("Tooltip transferred size: " + tooltip);
    is(value, transferred, "The displayed transferred size is correct.");
    is(tooltip, transferred, "The tooltip transferred size is correct.");
  }
  if (size !== undefined) {
    let value = target.querySelector(".requests-menu-size").getAttribute("value");
    let tooltip = target.querySelector(".requests-menu-size").getAttribute("tooltiptext");
    info("Displayed size: " + value);
    info("Tooltip size: " + tooltip);
    is(value, size, "The displayed size is correct.");
    is(tooltip, size, "The tooltip size is correct.");
  }
  if (time !== undefined) {
    let value = target.querySelector(".requests-menu-timings-total").getAttribute("value");
    let tooltip = target.querySelector(".requests-menu-timings-total").getAttribute("tooltiptext");
    info("Displayed time: " + value);
    info("Tooltip time: " + tooltip);
    ok(~~(value.match(/[0-9]+/)) >= 0, "The displayed time is correct.");
    ok(~~(tooltip.match(/[0-9]+/)) >= 0, "The tooltip time is correct.");
  }

  if (visibleIndex != -1) {
    if (visibleIndex % 2 == 0) {
      ok(aRequestItem.target.hasAttribute("even"),
        aRequestItem.value + " should have 'even' attribute.");
      ok(!aRequestItem.target.hasAttribute("odd"),
        aRequestItem.value + " shouldn't have 'odd' attribute.");
    } else {
      ok(!aRequestItem.target.hasAttribute("even"),
        aRequestItem.value + " shouldn't have 'even' attribute.");
      ok(aRequestItem.target.hasAttribute("odd"),
        aRequestItem.value + " should have 'odd' attribute.");
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
function waitFor (subject, eventName) {
  let deferred = promise.defer();
  subject.once(eventName, deferred.resolve);
  return deferred.promise;
}

/**
 * Tests if a button for a filter of given type is the only one checked.
 *
 * @param string aFilterType
 *        The type of the filter that should be the only one checked.
 */
function testFilterButtons(aMonitor, aFilterType) {
  let doc = aMonitor.panelWin.document;
  let target = doc.querySelector("#requests-menu-filter-" + aFilterType + "-button");
  let buttons = doc.querySelectorAll(".requests-menu-footer-button");

  // Only target should be checked.
  let checkStatus = [...buttons].map(button => button == target ? 1 : 0);
  testFilterButtonsCustom(aMonitor, checkStatus);
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
  let buttons = doc.querySelectorAll(".requests-menu-footer-button");
  for (let i = 0; i < aIsChecked.length; i++) {
    let button = buttons[i];
    if (aIsChecked[i]) {
      is(button.hasAttribute("checked"), true,
        "The " + button.id + " button should have a 'checked' attribute.");
    } else {
      is(button.hasAttribute("checked"), false,
        "The " + button.id + " button should not have a 'checked' attribute.");
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
  return executeInContent("devtools:test:xhr", requests)
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
function executeInContent(name, data={}, objects={}, expectResponse=true) {
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
