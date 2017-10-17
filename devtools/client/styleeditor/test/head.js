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

  return new Promise(resolve => {
    let targetWindow = win || window;
    let targetBrowser = targetWindow.gBrowser;

    let tab = targetBrowser.selectedTab = targetBrowser.addTab(url);
    BrowserTestUtils.browserLoaded(targetBrowser.selectedBrowser)
      .then(function () {
        info("URL '" + url + "' loading complete");
        resolve(tab);
      });
  });
};

/**
 * Navigate the currently selected tab to a new URL and wait for it to load.
 * @param {String} url The url to be loaded in the current tab.
 * @return a promise that resolves when the page has fully loaded.
 */
var navigateTo = function (url) {
  info(`Navigating to ${url}`);
  let browser = gBrowser.selectedBrowser;

  return new Promise(resolve => {
    browser.addEventListener("load", function () {
      resolve();
    }, {capture: true, once: true});

    browser.loadURI(url);
  });
};

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

  // The stylesheet list appears with an animation. Let this animation finish.
  let animations = ui._root.getAnimations({subtree: true});
  yield Promise.all(animations.map(a => a.finished));

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
