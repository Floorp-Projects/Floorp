/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */
/* import-globals-from ../../framework/test/shared-head.js */
/* import-globals-from ../../framework/test/head.js */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/head.js", this);

const JSON_VIEW_PREF = "devtools.jsonview.enabled";

// Enable JSON View for the test
Services.prefs.setBoolPref(JSON_VIEW_PREF, true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(JSON_VIEW_PREF);
});

// XXX move some API into devtools/framework/test/shared-head.js

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url
 *   The url to be loaded in the new tab.
 *
 * @param {Object} [optional]
 *   An object with the following optional properties:
 *   - appReadyState: The readyState of the JSON Viewer app that you want to
 *     wait for. Its value can be one of:
 *      - "uninitialized": The converter has started the request.
 *        If JavaScript is disabled, there will be no more readyState changes.
 *      - "loading": RequireJS started loading the scripts for the JSON Viewer.
 *        If the load timeouts, there will be no more readyState changes.
 *      - "interactive": The JSON Viewer app loaded, but possibly not all the JSON
 *        data has been received.
 *      - "complete" (default): The app is fully loaded with all the JSON.
 *   - docReadyState: The standard readyState of the document that you want to
 *     wait for. Its value can be one of:
 *      - "loading": The JSON data has not been completely loaded (but the app might).
 *      - "interactive": All the JSON data has been received.
 *      - "complete" (default): Since there aren't sub-resources like images,
 *        behaves as "interactive". Note the app might not be loaded yet.
 */
async function addJsonViewTab(url, {
  appReadyState = "complete",
  docReadyState = "complete",
} = {}) {
  info("Adding a new JSON tab with URL: '" + url + "'");
  let tabLoaded = addTab(url);
  let tab = gBrowser.selectedTab;
  let browser = tab.linkedBrowser;
  await Promise.race([tabLoaded, new Promise(resolve => {
    browser.webProgress.addProgressListener({
      QueryInterface: XPCOMUtils.generateQI(["nsIWebProgressListener",
                                             "nsISupportsWeakReference"]),
      onLocationChange(webProgress) {
        // Fires when the tab is ready but before completely loaded.
        webProgress.removeProgressListener(this);
        resolve();
      },
    }, Ci.nsIWebProgress.NOTIFY_LOCATION);
  })]);

  // Load devtools/shared/frame-script-utils.js
  getFrameScript();
  let rootDir = getRootDirectory(gTestPath);

  let data = {rootDir, appReadyState, docReadyState};
  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(browser, data, async function (data) {
    // Check if there is a JSONView object.
    let {JSONView} = content.window.wrappedJSObject;
    if (!JSONView) {
      throw new Error("The JSON Viewer did not load.");
    }

    // Load frame script with helpers for JSON View tests.
    let frameScriptUrl = data.rootDir + "doc_frame_script.js";
    Services.scriptloader.loadSubScript(frameScriptUrl, {}, "UTF-8");

    let docReadyStates = ["loading", "interactive", "complete"];
    let docReadyIndex = docReadyStates.indexOf(data.docReadyState);
    let appReadyStates = ["uninitialized", ...docReadyStates];
    let appReadyIndex = appReadyStates.indexOf(data.appReadyState);
    if (docReadyIndex < 0 || appReadyIndex < 0) {
      throw new Error("Invalid app or doc readyState parameter.");
    }

    // Wait until the document readyState suffices.
    let {document} = content.window;
    while (docReadyStates.indexOf(document.readyState) < docReadyIndex) {
      info(`DocReadyState is "${document.readyState}". Await "${data.docReadyState}"`);
      await new Promise(resolve => {
        document.addEventListener("readystatechange", resolve, {once: true});
      });
    }

    // Wait until the app readyState suffices.
    while (appReadyStates.indexOf(JSONView.readyState) < appReadyIndex) {
      info(`AppReadyState is "${JSONView.readyState}". Await "${data.appReadyState}"`);
      await new Promise(resolve => {
        content.addEventListener("AppReadyStateChange", resolve, {once: true});
      });
    }
  });

  return tab;
}

/**
 * Expanding a node in the JSON tree
 */
function clickJsonNode(selector) {
  info("Expanding node: '" + selector + "'");

  let browser = gBrowser.selectedBrowser;
  return BrowserTestUtils.synthesizeMouseAtCenter(selector, {}, browser);
}

/**
 * Select JSON View tab (in the content).
 */
function selectJsonViewContentTab(name) {
  info("Selecting tab: '" + name + "'");

  let browser = gBrowser.selectedBrowser;
  let selector = ".tabs-menu .tabs-menu-item." + name + " a";
  return BrowserTestUtils.synthesizeMouseAtCenter(selector, {}, browser);
}

function getElementCount(selector) {
  info("Get element count: '" + selector + "'");

  let data = {
    selector: selector
  };

  return executeInContent("Test:JsonView:GetElementCount", data)
  .then(result => {
    return result.count;
  });
}

function getElementText(selector) {
  info("Get element text: '" + selector + "'");

  let data = {
    selector: selector
  };

  return executeInContent("Test:JsonView:GetElementText", data)
  .then(result => {
    return result.text;
  });
}

function getElementAttr(selector, attr) {
  info("Get attribute '" + attr + "' for element '" + selector + "'");

  let data = {selector, attr};
  return executeInContent("Test:JsonView:GetElementAttr", data)
  .then(result => result.text);
}

function focusElement(selector) {
  info("Focus element: '" + selector + "'");

  let data = {
    selector: selector
  };

  return executeInContent("Test:JsonView:FocusElement", data);
}

/**
 * Send the string aStr to the focused element.
 *
 * For now this method only works for ASCII characters and emulates the shift
 * key state on US keyboard layout.
 */
function sendString(str, selector) {
  info("Send string: '" + str + "'");

  let data = {
    selector: selector,
    str: str
  };

  return executeInContent("Test:JsonView:SendString", data);
}

function waitForTime(delay) {
  return new Promise(resolve => setTimeout(resolve, delay));
}

function waitForFilter() {
  return executeInContent("Test:JsonView:WaitForFilter");
}

function normalizeNewLines(value) {
  return value.replace("(\r\n|\n)", "\n");
}

function evalInContent(code) {
  return executeInContent("Test:JsonView:Eval", {code})
  .then(result => result.result);
}
