/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from ../../../inspector/test/shared-head.js */

/* global waitUntilState, gBrowser */
/* exported addTestTab, checkTreeState, checkSidebarState, selectRow,
            toggleRow, addA11yPanelTestsTask, reload, navigate */

"use strict";

// Import framework's shared head.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

// Import inspector's shared head.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this);

// Load the shared Redux helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

const { ORDERED_PROPS } = require("devtools/client/accessibility/constants");

// Enable the Accessibility panel
Services.prefs.setBoolPref("devtools.accessibility.enabled", true);

/**
 * Wait for accessibility service to shut down. We consider it shut down when
 * an "a11y-init-or-shutdown" event is received with a value of "0".
 */
function shutdownA11y() {
  if (!Services.appinfo.accessibilityEnabled) {
    return Promise.resolve();
  }

  // Force collections to speed up accessibility service shutdown.
  Cu.forceGC();
  Cu.forceCC();
  Cu.forceShrinkingGC();

  return new Promise(resolve => {
    const observe = (subject, topic, data) => {
      if (data === "0") {
        Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
        resolve();
      }
    };
    // This event is coming from Gecko accessibility module when the
    // accessibility service is shutdown or initialzied. We attempt to shutdown
    // accessibility service naturally if there are no more XPCOM references to
    // a11y related objects (after GC/CC).
    Services.obs.addObserver(observe, "a11y-init-or-shutdown");
  });
}

registerCleanupFunction(async () => {
  info("Cleaning up...");
  await shutdownA11y();
  Services.prefs.clearUserPref("devtools.accessibility.enabled");
});

const EXPANDABLE_PROPS = ["actions", "states", "attributes"];

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
  const panel = await initAccessibilityPanel(tab);
  const win = panel.panelWin;
  const doc = win.document;
  const store = win.view.store;

  EventUtils.sendMouseEvent({ type: "click" },
    doc.getElementById("accessibility-enable-button"), win);

  await waitUntilState(store, state =>
    state.accessibles.size === 1 && state.details.accessible &&
    state.details.accessible.role === "document");

  // Wait for inspector load here to avoid protocol errors on shutdown, since
  // accessibility panel test can be too fast.
  await win.gToolbox.loadTool("inspector");

  return {
    tab,
    browser: tab.linkedBrowser,
    panel,
    win,
    toolbox: panel._toolbox,
    doc,
    store
  };
}

/**
 * Turn off accessibility features from within the panel. We call it before the
 * cleanup function to make sure that the panel is still present.
 */
async function disableAccessibilityInspector(env) {
  const { doc, win, panel } = env;
  // Disable accessibility service through the panel and wait for the shutdown
  // event.
  const shutdown = panel._front.once("shutdown");
  EventUtils.sendMouseEvent({ type: "click" },
    doc.getElementById("accessibility-disable-button"), win);
  await shutdown;
}

/**
 * Open the Accessibility panel for the given tab.
 *
 * @param {Element} tab
 *        Optional tab element for which you want open the Accessibility panel.
 *        The default tab is taken from the global variable |tab|.
 * @return a promise that is resolved once the panel is open.
 */
async function initAccessibilityPanel(tab = gBrowser.selectedTab) {
  const target = TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "accessibility");
  return toolbox.getCurrentPanel();
}

/**
 * Check the state of the accessibility tree.
 * @param  {document} doc       panel documnent.
 * @param  {Array}    expected  an array that represents an expected row list.
 */
async function checkTreeState(doc, expected) {
  info("Checking tree state.");
  const hasExpectedStructure = await BrowserTestUtils.waitForCondition(() =>
    [...doc.querySelectorAll(".treeRow")].every((row, i) =>
      row.querySelector(".treeLabelCell").textContent === expected[i].role &&
      row.querySelector(".treeValueCell").textContent === expected[i].name),
    "Wait for the right tree update.");

  ok(hasExpectedStructure, "Tree structure is correct.");
}

/**
 * Check the state of the accessibility sidebar.
 * @param  {Object} store         React store for the panel (includes store for
 *                                the sidebar).
 * @param  {Object} expectedState Expected state of the sidebar.
 */
async function checkSidebarState(store, expectedState) {
  info("Checking sidebar state.");
  await waitUntilState(store, ({ details }) => {
    for (const key of ORDERED_PROPS) {
      const expected = expectedState[key];
      if (expected === undefined) {
        continue;
      }

      if (EXPANDABLE_PROPS.includes(key)) {
        if (JSON.stringify(details.accessible[key]) !== JSON.stringify(expected)) {
          return false;
        }
      } else if (details.accessible && details.accessible[key] !== expected) {
        return false;
      }
    }

    ok(true, "Sidebar state is correct.");
    return true;
  });
}

/**
 * Select tree row.
 * @param  {document} doc       panel documnent.
 * @param  {Number}   rowNumber number of the row/tree node to be selected.
 */
function selectRow(doc, rowNumber) {
  info(`Selecting row ${rowNumber}.`);
  EventUtils.sendMouseEvent({ type: "click" },
    doc.querySelectorAll(".treeRow")[rowNumber], doc.defaultView);
}

/**
 * Toggle an expandable tree row.
 * @param  {document} doc       panel documnent.
 * @param  {Number}   rowNumber number of the row/tree node to be toggled.
 */
async function toggleRow(doc, rowNumber) {
  const win = doc.defaultView;
  const twisty = doc.querySelectorAll(".theme-twisty")[rowNumber];
  const expected = !twisty.classList.contains("open");

  info(`${expected ? "Expanding" : "Collapsing"} row ${rowNumber}.`);

  EventUtils.sendMouseEvent({ type: "click" }, twisty, win);
  await BrowserTestUtils.waitForCondition(() =>
    !twisty.classList.contains("devtools-throbber") &&
    expected === twisty.classList.contains("open"), "Twisty updated.");
}

/**
 * Iterate over actions/tests structure and test the state of the
 * accessibility panel.
 * @param  {JSON}   tests test data that has the format of:
 *                    {
 *                      desc     {String}    description for better logging
 *                      action   {Function}  An optional action that needs to be
 *                                           performed before the state of the
 *                                           tree and the sidebar can be checked
 *                      expected {JSON}      An expected states for the tree and
 *                                           the sidebar
 *                    }
 * @param  {Object} env  contains all relevant environment objects (same
 *                       structure as the return value of 'addTestTab' funciton)
 */
async function runA11yPanelTests(tests, env) {
  for (const { desc, action, expected } of tests) {
    info(desc);

    if (action) {
      await action(env);
    }

    const { tree, sidebar } = expected;
    if (tree) {
      await checkTreeState(env.doc, tree);
    }

    if (sidebar) {
      await checkSidebarState(env.store, sidebar);
    }
  }
}

/**
 * Build a valid URL from an HTML snippet.
 * @param  {String} uri HTML snippet
 * @return {String}     built URL
 */
function buildURL(uri) {
  return `data:text/html;charset=UTF-8,${encodeURIComponent(uri)}`;
}

/**
 * Add a test task based on the test structure and a test URL.
 * @param  {JSON}   tests  test data that has the format of:
 *                    {
 *                      desc     {String}    description for better logging
 *                      action   {Function}  An optional action that needs to be
 *                                           performed before the state of the
 *                                           tree and the sidebar can be checked
 *                      expected {JSON}      An expected states for the tree and
 *                                           the sidebar
 *                    }
 * @param {String}  uri    test URL
 * @param {String}  msg    a message that is printed for the test
 */
function addA11yPanelTestsTask(tests, uri, msg) {
  addA11YPanelTask(msg, uri, env => runA11yPanelTests(tests, env));
}

/**
 * A wrapper function around add_task that sets up the test environment, runs
 * the test and then disables accessibility tools.
 * @param {String}   msg    a message that is printed for the test
 * @param {String}   uri    test URL
 * @param {Function} task   task function containing the tests.
 */
function addA11YPanelTask(msg, uri, task) {
  add_task(async function a11YPanelTask() {
    info(msg);
    const env = await addTestTab(buildURL(uri));
    await task(env);
    await disableAccessibilityInspector(env);
  });
}

/**
 * Reload panel target.
 * @param  {Object} target             Panel target.
 * @param  {String} waitForTargetEvent Event to wait for after reload.
 */
function reload(target, waitForTargetEvent = "navigate") {
  executeSoon(() => target.activeTab.reload());
  return once(target, waitForTargetEvent);
}

/**
 * Navigate to a new URL within the panel target.
 * @param  {Object} target             Panel target.
 * @param  {Srting} url                URL to navigate to.
 * @param  {String} waitForTargetEvent Event to wait for after reload.
 */
function navigate(target, url, waitForTargetEvent = "navigate") {
  executeSoon(() => target.activeTab.navigateTo(url));
  return once(target, waitForTargetEvent);
}
