/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* All top-level definitions here are exports.  */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

/* import-globals-from ../../inspector/shared/test/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/shared/test/head.js", this);

const TEST_BASE = "chrome://mochitests/content/browser/devtools/client/styleeditor/test/";
const TEST_BASE_HTTP = "http://example.com/browser/devtools/client/styleeditor/test/";
const TEST_BASE_HTTPS = "https://example.com/browser/devtools/client/styleeditor/test/";
const TEST_HOST = "mochi.test:8888";

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @param {Window} win The window to add the tab to (default: current window).
 * @return a promise that resolves to the tab object when the url is loaded
 */
var addTab = function (url, win) {
  info("Adding a new tab with URL: '" + url + "'");
  let def = defer();

  let targetWindow = win || window;
  let targetBrowser = targetWindow.gBrowser;

  let tab = targetBrowser.selectedTab = targetBrowser.addTab(url);
  targetBrowser.selectedBrowser.addEventListener("load", function onload() {
    targetBrowser.selectedBrowser.removeEventListener("load", onload, true);
    info("URL '" + url + "' loading complete");
    def.resolve(tab);
  }, true);

  return def.promise;
};

/**
 * Navigate the currently selected tab to a new URL and wait for it to load.
 * @param {String} url The url to be loaded in the current tab.
 * @return a promise that resolves when the page has fully loaded.
 */
var navigateTo = Task.async(function* (url) {
  info(`Navigating to ${url}`);
  let browser = gBrowser.selectedBrowser;

  let navigating = defer();
  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);
    navigating.resolve();
  }, true);

  browser.loadURI(url);

  yield navigating.promise;
});

var navigateToAndWaitForStyleSheets = Task.async(function* (url, ui) {
  let onReset = ui.once("stylesheets-reset");
  yield navigateTo(url);
  yield onReset;
});

var reloadPageAndWaitForStyleSheets = Task.async(function* (ui) {
  info("Reloading the page.");

  let onReset = ui.once("stylesheets-reset");
  let browser = gBrowser.selectedBrowser;
  yield ContentTask.spawn(browser, null, "() => content.location.reload()");
  yield onReset;
});

/**
 * Open the style editor for the current tab.
 */
var openStyleEditor = Task.async(function* (tab) {
  if (!tab) {
    tab = gBrowser.selectedTab;
  }
  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "styleeditor");
  let panel = toolbox.getPanel("styleeditor");
  let ui = panel.UI;

  return { toolbox, panel, ui };
});

/**
 * Creates a new tab in specified window navigates it to the given URL and
 * opens style editor in it.
 */
var openStyleEditorForURL = Task.async(function* (url, win) {
  let tab = yield addTab(url, win);
  let result = yield openStyleEditor(tab);
  result.tab = tab;
  return result;
});

/**
 * Send an async message to the frame script and get back the requested
 * computed style property.
 *
 * @param {String} selector
 *        The selector used to obtain the element.
 * @param {String} pseudo
 *        pseudo id to query, or null.
 * @param {String} name
 *        name of the property.
 */
var getComputedStyleProperty = function* (args) {
  return yield ContentTask.spawn(gBrowser.selectedBrowser, args,
    function ({selector, pseudo, name}) {
      let element = content.document.querySelector(selector);
      let style = content.getComputedStyle(element, pseudo);
      return style.getPropertyValue(name);
    }
  );
};
