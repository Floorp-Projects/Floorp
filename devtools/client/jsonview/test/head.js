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
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addJsonViewTab(url) {
  info("Adding a new JSON tab with URL: '" + url + "'");

  let deferred = promise.defer();
  addTab(url).then(tab => {
    let browser = tab.linkedBrowser;

    // Load devtools/shared/frame-script-utils.js
    getFrameScript();

    // Load frame script with helpers for JSON View tests.
    let rootDir = getRootDirectory(gTestPath);
    let frameScriptUrl = rootDir + "doc_frame_script.js";
    browser.messageManager.loadFrameScript(frameScriptUrl, false);

    // Resolve if the JSONView is fully loaded or wait
    // for an initialization event.
    if (content.window.wrappedJSObject.jsonViewInitialized) {
      deferred.resolve(tab);
    } else {
      waitForContentMessage("Test:JsonView:JSONViewInitialized").then(() => {
        deferred.resolve(tab);
      });
    }
  });

  return deferred.promise;
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
  let deferred = promise.defer();
  setTimeout(deferred.resolve, delay);
  return deferred.promise;
}

function waitForFilter() {
  return executeInContent("Test:JsonView:WaitForFilter");
}

function normalizeNewLines(value) {
  return value.replace("(\r\n|\n)", "\n");
}
