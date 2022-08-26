/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */
/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

// DOM panel actions.
const constants = require("devtools/client/dom/content/constants");

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dom.enabled", true);

// Enable the DOM panel
Services.prefs.setBoolPref("devtools.dom.enabled", true);

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.dom.enabled");
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url
 *        The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when
 *        the url is loaded
 */
async function addTestTab(url) {
  info("Adding a new test tab with URL: '" + url + "'");

  const tab = await addTab(url);

  // Select the DOM panel and wait till it's initialized.
  const panel = await initDOMPanel(tab);

  // FETCH_PROPERTIES should be fired during the call to initDOMPanel
  // But note that this behavior changed during a change in webconsole
  // initialization. So this might be racy.
  const doc = panel.panelWin.document;
  const nodes = [...doc.querySelectorAll(".treeLabel")];
  ok(!!nodes.length, "The DOM panel is already populated");

  return {
    tab,
    browser: tab.linkedBrowser,
    panel,
  };
}

/**
 * Open the DOM panel for the given tab.
 *
 * @param {Element} tab
 *        Optional tab element for which you want open the DOM panel.
 *        The default tab is taken from the global variable |tab|.
 * @return a promise that is resolved once the web console is open.
 */
async function initDOMPanel(tab) {
  tab = tab || gBrowser.selectedTab;
  const toolbox = await gDevTools.showToolboxForTab(tab, { toolId: "dom" });
  const panel = toolbox.getCurrentPanel();
  return panel;
}

/**
 * Synthesize asynchronous click event (with clean stack trace).
 */
function synthesizeMouseClickSoon(panel, element) {
  return new Promise(resolve => {
    executeSoon(() => {
      EventUtils.synthesizeMouse(element, 2, 2, {}, panel.panelWin);
      resolve();
    });
  });
}

/**
 * Returns tree row with specified label.
 */
function getRowByLabel(panel, text) {
  const doc = panel.panelWin.document;
  const labels = [...doc.querySelectorAll(".treeLabel")];
  const label = labels.find(node => node.textContent == text);
  return label ? label.closest(".treeRow") : null;
}

/**
 * Returns tree row with specified index.
 */
function getRowByIndex(panel, id) {
  const doc = panel.panelWin.document;
  const labels = [...doc.querySelectorAll(".treeLabel")];
  const label = labels.find((node, i) => i == id);
  return label ? label.closest(".treeRow") : null;
}

/**
 * Returns the children (tree row text) of the specified object name as an
 * array.
 */
function getAllRowsForLabel(panel, text) {
  let rootObjectLevel;
  let node;
  const result = [];
  const doc = panel.panelWin.document;
  const nodes = [...doc.querySelectorAll(".treeLabel")];

  // Find the label (object name) for which we want the children. We remove
  // nodes from the start of the array until we reach the property. The children
  // are then at the start of the array.
  while (true) {
    node = nodes.shift();

    if (!node || node.textContent === text) {
      rootObjectLevel = node.getAttribute("data-level");
      break;
    }
  }

  // Return an empty array if the node is not found.
  if (!node) {
    return result;
  }

  // Now get the children.
  for (node of nodes) {
    const level = node.getAttribute("data-level");

    if (level > rootObjectLevel) {
      result.push({
        name: normalizeTreeValue(node.textContent),
        value: normalizeTreeValue(
          node.parentNode.nextElementSibling.textContent
        ),
      });
    } else {
      break;
    }
  }

  return result;
}

/**
 * Strings in the tree are in the form ""a"" and numbers in the form "1". We
 * normalize these values by converting ""a"" to "a" and "1" to 1.
 *
 * @param  {String} value
 *         The value to normalize.
 * @return {String|Number}
 *         The normalized value.
 */
function normalizeTreeValue(value) {
  if (value === `""`) {
    return "";
  }
  if (value.startsWith(`"`) && value.endsWith(`"`)) {
    return value.substr(1, value.length - 2);
  }
  if (isFinite(value) && parseInt(value, 10) == value) {
    return parseInt(value, 10);
  }

  return value;
}

/**
 * Expands elements with given label and waits till
 * children are received from the backend.
 */
function expandRow(panel, labelText) {
  const row = getRowByLabel(panel, labelText);
  return synthesizeMouseClickSoon(panel, row).then(() => {
    // Wait till children (properties) are fetched
    // from the backend.
    const store = getReduxStoreFromPanel(panel);
    return waitForDispatch(store, "FETCH_PROPERTIES");
  });
}

function refreshPanel(panel) {
  const doc = panel.panelWin.document;
  const button = doc.querySelector("#dom-refresh-button");
  return synthesizeMouseClickSoon(panel, button).then(() => {
    // Wait till children (properties) are fetched
    // from the backend.
    const store = getReduxStoreFromPanel(panel);
    return waitForDispatch(store, "FETCH_PROPERTIES");
  });
}

function getReduxStoreFromPanel(panel) {
  return panel.panelWin.view.mainFrame.store;
}
