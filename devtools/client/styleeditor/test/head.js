/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* All top-level definitions here are exports.  */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/shared/test/head.js",
  this
);

const TEST_BASE =
  "chrome://mochitests/content/browser/devtools/client/styleeditor/test/";
const TEST_BASE_HTTP =
  "http://example.com/browser/devtools/client/styleeditor/test/";
const TEST_BASE_HTTPS =
  "https://example.com/browser/devtools/client/styleeditor/test/";
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
    const targetWindow = win || window;
    const targetBrowser = targetWindow.gBrowser;

    const tab = (targetBrowser.selectedTab = BrowserTestUtils.addTab(
      targetBrowser,
      url
    ));
    BrowserTestUtils.browserLoaded(targetBrowser.selectedBrowser).then(
      function () {
        info("URL '" + url + "' loading complete");
        resolve(tab);
      }
    );
  });
};

var navigateToAndWaitForStyleSheets = async function (url, ui, editorCount) {
  const onClear = ui.once("stylesheets-clear");
  await navigateTo(url);
  await onClear;
  await waitUntil(() => ui.editors.length === editorCount);
};

var reloadPageAndWaitForStyleSheets = async function (ui, editorCount) {
  info("Reloading the page.");

  const onClear = ui.once("stylesheets-clear");
  let count = 0;
  const onAllEditorAdded = new Promise(res => {
    const off = ui.on("editor-added", editor => {
      count++;
      info(`Received ${editor.friendlyName} (${count}/${editorCount})`);
      if (count == editorCount) {
        res();
        off();
      }
    });
  });

  await reloadBrowser();
  await onClear;

  await onAllEditorAdded;
  info("All expected editors added");
};

/**
 * Open the style editor for the current tab.
 */
var openStyleEditor = async function (tab) {
  if (!tab) {
    tab = gBrowser.selectedTab;
  }
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "styleeditor",
  });
  const panel = toolbox.getPanel("styleeditor");
  const ui = panel.UI;

  return { toolbox, panel, ui };
};

/**
 * Creates a new tab in specified window navigates it to the given URL and
 * opens style editor in it.
 */
var openStyleEditorForURL = async function (url, win) {
  const tab = await addTab(url, win);
  const result = await openStyleEditor(tab);
  result.tab = tab;
  return result;
};

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
var getComputedStyleProperty = async function (args) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [args],
    function ({ selector, pseudo, name }) {
      const element = content.document.querySelector(selector);
      const style = content.getComputedStyle(element, pseudo);
      return style.getPropertyValue(name);
    }
  );
};

/**
 * Wait for "at-rules-list-changed" events to settle on StyleEditorUI.
 * Returns a promise that resolves the number of events caught while waiting.
 *
 * @param {StyleEditorUI} ui
 *        Current StyleEditorUI on which at-rules-list-changed events should be fired.
 * @param {Number} delay
 */
function waitForManyEvents(ui, delay) {
  return new Promise(resolve => {
    let timer;
    let count = 0;
    const onEvent = () => {
      count++;
      clearTimeout(timer);

      // Wait for some time to catch subsequent events.
      timer = setTimeout(() => {
        // Remove the listener and resolve.
        ui.off("at-rules-list-changed", onEvent);
        resolve(count);
      }, delay);
    };
    ui.on("at-rules-list-changed", onEvent);
  });
}

/**
 * Creates a new style sheet in the Style Editor

 * @param {StyleEditorUI} ui
 *        Current StyleEditorUI on which to simulate pressing the + button.
 * @param {Window} panelWindow
 *        The panelWindow property of the current Style Editor panel.
 */
function createNewStyleSheet(ui, panelWindow) {
  info("Creating a new stylesheet now");

  return new Promise(resolve => {
    ui.once("editor-added", editor => {
      editor.getSourceEditor().then(resolve);
    });

    waitForFocus(function () {
      // create a new style sheet
      const newButton = panelWindow.document.querySelector(
        ".style-editor-newButton"
      );
      ok(newButton, "'new' button exists");

      EventUtils.synthesizeMouseAtCenter(newButton, {}, panelWindow);
    }, panelWindow);
  });
}

/**
 * Returns the panel root element (StyleEditorUI._root)
 *
 * @param {StyleEditorPanel} panel
 * @returns {Element}
 */
function getRootElement(panel) {
  return panel.panelWindow.document.getElementById("style-editor-chrome");
}

/**
 * Returns the panel context menu element
 *
 * @param {StyleEditorPanel} panel
 * @returns {Element}
 */
function getContextMenuElement(panel) {
  return panel.panelWindow.document.getElementById("sidebar-context");
}
