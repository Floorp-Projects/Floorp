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
 * @param {Number} timeout [optional]
 *   The maximum number of milliseconds allowed before the initialization of the
 *   JSON Viewer once the tab has been loaded. If exceeded, the initialization
 *   will be considered to have failed, and the returned promise will be rejected.
 *   If this parameter is not passed or is negative, it will be ignored.
 */
async function addJsonViewTab(url, timeout = -1) {
  info("Adding a new JSON tab with URL: '" + url + "'");

  let tab = await addTab(url);
  let browser = tab.linkedBrowser;

  // Load devtools/shared/frame-script-utils.js
  getFrameScript();

  // Load frame script with helpers for JSON View tests.
  let rootDir = getRootDirectory(gTestPath);
  let frameScriptUrl = rootDir + "doc_frame_script.js";
  browser.messageManager.loadFrameScript(frameScriptUrl, false);

  // Check if there is a JSONView object.
  if (!content.window.wrappedJSObject.JSONView) {
    throw new Error("JSON Viewer did not load.");
  }

  // Resolve if the JSONView is fully loaded.
  if (content.window.wrappedJSObject.JSONView.initialized) {
    return tab;
  }

  // Otherwise wait for an initialization event, possibly with a time limit.
  const onJSONViewInitialized =
    waitForContentMessage("Test:JsonView:JSONViewInitialized")
    .then(() => tab);

  if (!(timeout >= 0)) {
    return onJSONViewInitialized;
  }

  if (content.window.document.readyState !== "complete") {
    await waitForContentMessage("Test:JsonView:load");
  }

  let onTimeout = new Promise((_, reject) =>
    setTimeout(() => reject(new Error("JSON Viewer did not load.")), timeout));

  return Promise.race([onJSONViewInitialized, onTimeout]);
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
