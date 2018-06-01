/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */
/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js", this);

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
function addTestTab(url) {
  info("Adding a new test tab with URL: '" + url + "'");

  return new Promise(resolve => {
    addTab(url).then(tab => {
      // Load devtools/shared/test/frame-script-utils.js
      loadFrameScriptUtils();

      // Select the DOM panel and wait till it's initialized.
      initDOMPanel(tab).then(panel => {
        waitForDispatch(panel, "FETCH_PROPERTIES").then(() => {
          resolve({
            tab: tab,
            browser: tab.linkedBrowser,
            panel: panel
          });
        });
      });
    });
  });
}

/**
 * Open the DOM panel for the given tab.
 *
 * @param {Element} tab
 *        Optional tab element for which you want open the DOM panel.
 *        The default tab is taken from the global variable |tab|.
 * @return a promise that is resolved once the web console is open.
 */
function initDOMPanel(tab) {
  return new Promise(resolve => {
    const target = TargetFactory.forTab(tab || gBrowser.selectedTab);
    gDevTools.showToolbox(target, "dom").then(toolbox => {
      const panel = toolbox.getCurrentPanel();
      resolve(panel);
    });
  });
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
        value: normalizeTreeValue(node.parentNode.nextElementSibling.textContent)
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
    return waitForDispatch(panel, "FETCH_PROPERTIES");
  });
}

function evaluateJSAsync(panel, expression) {
  return new Promise(resolve => {
    panel.target.activeConsole.evaluateJSAsync(expression, res => {
      resolve(res);
    });
  });
}

function refreshPanel(panel) {
  const doc = panel.panelWin.document;
  const button = doc.querySelector("#dom-refresh-button");
  return synthesizeMouseClickSoon(panel, button).then(() => {
    // Wait till children (properties) are fetched
    // from the backend.
    return waitForDispatch(panel, "FETCH_PROPERTIES");
  });
}

// Redux related API, use from shared location
// as soon as bug 1261076 is fixed.

// Wait until an action of `type` is dispatched. If it's part of an
// async operation, wait until the `status` field is "done" or "error"
function _afterDispatchDone(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      // Normally we would use `services.WAIT_UNTIL`, but use the
      // internal name here so tests aren't forced to always pass it
      // in
      type: "@@service/waitUntil",
      predicate: action => {
        if (action.type === type) {
          return action.status ?
            (action.status === "end" || action.status === "error") :
            true;
        }
        return false;
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      }
    });
  });
}

function waitForDispatch(panel, type, eventRepeat = 1) {
  const store = panel.panelWin.view.mainFrame.store;
  const actionType = constants[type];
  let count = 0;

  return (async function() {
    info("Waiting for " + type + " to dispatch " + eventRepeat + " time(s)");
    while (count < eventRepeat) {
      await _afterDispatchDone(store, actionType);
      count++;
      info(type + " dispatched " + count + " time(s)");
    }
  })();
}
