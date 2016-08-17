/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */
/* import-globals-from ../../../test/head.js */

"use strict";

// Load Web Console head.js, it implements helper console test API
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/webconsole/test/head.js", this);

const FRAME_SCRIPT_UTILS_URL =
  "chrome://devtools/content/shared/frame-script-utils.js";

const NET_INFO_PREF = "devtools.webconsole.filter.networkinfo";
const NET_XHR_PREF = "devtools.webconsole.filter.netxhr";

// Enable XHR logging for the test
Services.prefs.setBoolPref(NET_INFO_PREF, true);
Services.prefs.setBoolPref(NET_XHR_PREF, true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(NET_INFO_PREF, true);
  Services.prefs.clearUserPref(NET_XHR_PREF, true);
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addTestTab(url) {
  info("Adding a new JSON tab with URL: '" + url + "'");

  return Task.spawn(function* () {
    let tab = yield addTab(url);

    // Load devtools/shared/frame-script-utils.js
    loadCommonFrameScript(tab);

    // Open the Console panel
    let hud = yield openConsole();

    return {
      tab: tab,
      browser: tab.linkedBrowser,
      hud: hud
    };
  });
}

/**
 *
 * @param hud
 * @param options
 */
function executeAndInspectXhr(hud, options) {
  hud.jsterm.clearOutput();

  options.queryString = options.queryString || "";

  // Execute XHR in the content scope.
  performRequestsInContent({
    method: options.method,
    url: options.url + options.queryString,
    body: options.body,
    nocache: options.nocache,
    requestHeaders: options.requestHeaders
  });

  return Task.spawn(function* () {
    // Wait till the appropriate Net log appears in the Console panel.
    let rules = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: options.url,
        category: CATEGORY_NETWORK,
        severity: SEVERITY_INFO,
        isXhr: true,
      }]
    });

    // The log is here, get its parent element (className: 'message').
    let msg = [...rules[0].matched][0];
    let body = msg.querySelector(".message-body");

    // Open XHR HTTP details body and wait till the UI fetches
    // all necessary data from the backend. All RPD requests
    // needs to be finished before we can continue testing.
    yield synthesizeMouseClickSoon(hud, body);
    yield waitForBackend(msg);
    let netInfoBody = body.querySelector(".netInfoBody");
    ok(netInfoBody, "Net info body must exist");
    return netInfoBody;
  });
}

/**
 * Wait till XHR data are fetched from the backend (i.e. there are
 * no pending RDP requests.
 */
function waitForBackend(element) {
  if (!element.hasAttribute("loading")) {
    return;
  }
  return once(element, "netlog-no-pending-requests", true);
}

/**
 * Select specific tab in XHR info body.
 *
 * @param netInfoBody The main XHR info body
 * @param tabId Tab ID (possible values: 'headers', 'cookies', 'params',
 *   'post', 'response');
 *
 * @returns Tab body element.
 */
function selectNetInfoTab(hud, netInfoBody, tabId) {
  let tab = netInfoBody.querySelector(".tabs-menu-item." + tabId);
  ok(tab, "Tab must exist " + tabId);

  // Click to select specified tab and wait till its
  // UI is populated with data from the backend.
  // There must be no pending RDP requests before we can
  // continue testing the UI.
  return Task.spawn(function* () {
    yield synthesizeMouseClickSoon(hud, tab);
    let msg = getAncestorByClass(netInfoBody, "message");
    yield waitForBackend(msg);
    let tabBody = netInfoBody.querySelector("." + tabId + "TabBox");
    ok(tabBody, "Tab body must exist");
    return tabBody;
  });
}

/**
 * Return parent node with specified class.
 *
 * @param node A child element
 * @param className Specified class name.
 *
 * @returns A parent element.
 */
function getAncestorByClass(node, className) {
  for (let parent = node; parent; parent = parent.parentNode) {
    if (parent.classList && parent.classList.contains(className)) {
      return parent;
    }
  }
  return null;
}

/**
 * Synthesize asynchronous click event (with clean stack trace).
 */
function synthesizeMouseClickSoon(hud, element) {
  return new Promise((resolve) => {
    executeSoon(() => {
      EventUtils.synthesizeMouse(element, 2, 2, {}, hud.iframeWindow);
      resolve();
    });
  });
}

/**
 * Execute XHR in the content scope.
 */
function performRequestsInContent(requests) {
  info("Performing requests in the context of the content.");
  return executeInContent("devtools:test:xhr", requests);
}

function executeInContent(name, data = {}, objects = {},
  expectResponse = true) {
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  }

  return Promise.resolve();
}

function waitForContentMessage(name) {
  info("Expecting message " + name + " from content");

  let mm = gBrowser.selectedBrowser.messageManager;

  return new Promise((resolve) => {
    mm.addMessageListener(name, function onMessage(msg) {
      mm.removeMessageListener(name, onMessage);
      resolve(msg.data);
    });
  });
}

function loadCommonFrameScript(tab) {
  let browser = tab ? tab.linkedBrowser : gBrowser.selectedBrowser;
  browser.messageManager.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
}
