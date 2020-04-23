/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from ../../../inspector/test/shared-head.js */

/* global waitUntilState, gBrowser */
/* exported addTestTab, checkTreeState, checkSidebarState, checkAuditState, selectRow,
            toggleRow, toggleMenuItem, addA11yPanelTestsTask, reload, navigate,
            openSimulationMenu, toggleSimulationOption, TREE_FILTERS_MENU_ID,
            PREFS_MENU_ID, checkHighlighted */

"use strict";

// Import framework's shared head.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

// Import inspector's shared head.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

// Load the shared Redux helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this
);

const {
  ORDERED_PROPS,
  PREF_KEYS,
} = require("devtools/client/accessibility/constants");

// Enable the Accessibility panel
Services.prefs.setBoolPref("devtools.accessibility.enabled", true);

const SIMULATION_MENU_BUTTON_ID = "#simulation-menu-button";
const TREE_FILTERS_MENU_ID = "accessibility-tree-filters-menu";
const PREFS_MENU_ID = "accessibility-tree-filters-prefs-menu";

const MENU_INDEXES = {
  [TREE_FILTERS_MENU_ID]: 0,
  [PREFS_MENU_ID]: 1,
};

/**
 * Enable accessibility service and wait for a11y init event.
 * @return {Object}  instance of accessibility service.
 */
async function initA11y() {
  if (Services.appinfo.accessibilityEnabled) {
    return Cc["@mozilla.org/accessibilityService;1"].getService(
      Ci.nsIAccessibilityService
    );
  }

  const initPromise = new Promise(resolve => {
    const observe = () => {
      Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
      resolve();
    };
    Services.obs.addObserver(observe, "a11y-init-or-shutdown");
  });

  const a11yService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService
  );
  await initPromise;
  return a11yService;
}

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

  const enableButton = doc.getElementById("accessibility-enable-button");
  // If enable button is not found, asume the tool is already enabled.
  if (enableButton) {
    EventUtils.sendMouseEvent({ type: "click" }, enableButton, win);
  }

  await waitUntilState(
    store,
    state =>
      state.accessibles.size === 1 &&
      state.details.accessible &&
      state.details.accessible.role === "document"
  );

  return {
    tab,
    browser: tab.linkedBrowser,
    panel,
    win,
    toolbox: panel._toolbox,
    doc,
    store,
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
  const shutdown = panel.accessibilityProxy.accessibilityFront.once("shutdown");
  const disableButton = await BrowserTestUtils.waitForCondition(
    () => doc.getElementById("accessibility-disable-button"),
    "Wait for the disable button."
  );
  EventUtils.sendMouseEvent({ type: "click" }, disableButton, win);
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
  const target = await TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "accessibility");
  return toolbox.getCurrentPanel();
}

/**
 * Compare text within the list of potential badges rendered for accessibility
 * tree row when its accessible object has accessibility failures.
 * @param {DOMNode} badges
 *        Container element that contains badge elements.
 * @param {Array|null} expected
 *        List of expected badge labels for failing accessibility checks.
 */
function compareBadges(badges, expected = []) {
  const badgeEls = badges ? [...badges.querySelectorAll(".badge")] : [];
  return (
    badgeEls.length === expected.length &&
    badgeEls.every((badge, i) => badge.textContent === expected[i])
  );
}

/**
 * Find an ancestor that is scrolled for a given DOMNode.
 *
 * @param {DOMNode} node
 *        DOMNode that to find an ancestor for that is scrolled.
 */
function closestScrolledParent(node) {
  if (node == null) {
    return null;
  }

  if (node.scrollHeight > node.clientHeight) {
    return node;
  }

  return closestScrolledParent(node.parentNode);
}

/**
 * Check if a given element is visible to the user and is not scrolled off
 * because of the overflow.
 *
 * @param   {Element} element
 *          Element to be checked whether it is visible and is not scrolled off.
 *
 * @returns {Boolean}
 *          True if the element is visible.
 */
function isVisible(element) {
  const { top, bottom } = element.getBoundingClientRect();
  const scrolledParent = closestScrolledParent(element.parentNode);
  const scrolledParentRect = scrolledParent
    ? scrolledParent.getBoundingClientRect()
    : null;
  return (
    !scrolledParent ||
    (top >= scrolledParentRect.top && bottom <= scrolledParentRect.bottom)
  );
}

/**
 * Check selected styling and visibility for a given row in the accessibility
 * tree.
 * @param   {DOMNode} row
 *          DOMNode for a given accessibility row.
 * @param   {Boolean} expected
 *          Expected selected state.
 *
 * @returns {Boolean}
 *          True if visibility and styling matches expected selected state.
 */
function checkSelected(row, expected) {
  if (!expected) {
    return true;
  }

  if (row.classList.contains("selected") !== expected) {
    return false;
  }

  return isVisible(row);
}

/**
 * Check level for a given row in the accessibility tree.
 * @param   {DOMNode} row
 *          DOMNode for a given accessibility row.
 * @param   {Boolean} expected
 *          Expected row level (aria-level).
 *
 * @returns {Boolean}
 *          True if the aria-level for the row is as expected.
 */
function checkLevel(row, expected) {
  if (!expected) {
    return true;
  }

  return parseInt(row.getAttribute("aria-level"), 10) === expected;
}

/**
 * Check the state of the accessibility tree.
 * @param  {document} doc       panel documnent.
 * @param  {Array}    expected  an array that represents an expected row list.
 */
async function checkTreeState(doc, expected) {
  info("Checking tree state.");
  const hasExpectedStructure = await BrowserTestUtils.waitForCondition(
    () =>
      [...doc.querySelectorAll(".treeRow")].every((row, i) => {
        const { role, name, badges, selected, level } = expected[i];
        return (
          row.querySelector(".treeLabelCell").textContent === role &&
          row.querySelector(".treeValueCell").textContent === name &&
          compareBadges(row.querySelector(".badges"), badges) &&
          checkSelected(row, selected) &&
          checkLevel(row, level)
        );
      }),
    "Wait for the right tree update."
  );

  ok(hasExpectedStructure, "Tree structure is correct.");
}

/**
 * Check if relations object matches what is expected. Note: targets are matched by their
 * name and role.
 * @param  {Object} relations  Relations to test.
 * @param  {Object} expected   Expected relations.
 * @return {Boolean}           True if relation types and their targers match what is
 *                             expected.
 */
function relationsMatch(relations, expected) {
  for (const relationType in expected) {
    let expTargets = expected[relationType];
    expTargets = Array.isArray(expTargets) ? expTargets : [expTargets];

    let targets = relations[relationType];
    targets = Array.isArray(targets) ? targets : [targets];

    for (const index in expTargets) {
      if (
        expTargets[index].name !== targets[index].name ||
        expTargets[index].role !== targets[index].role
      ) {
        return false;
      }
    }
  }

  return true;
}

/**
 * When comparing numerical values (for example contrast), we only care about the 2
 * decimal points.
 * @param  {String} _
 *         Key of the property that is parsed.
 * @param  {Any} value
 *         Value of the property that is parsed.
 * @return {Any}
 *         Newly formatted value in case of the numeric value.
 */
function parseNumReplacer(_, value) {
  if (typeof value === "number") {
    return value.toFixed(2);
  }

  return value;
}

/**
 * Check the state of the accessibility sidebar audit(checks).
 * @param  {Object} store         React store for the panel (includes store for
 *                                the audit).
 * @param  {Object} expectedState Expected state of the sidebar audit(checks).
 */
async function checkAuditState(store, expectedState) {
  info("Checking audit state.");
  await waitUntilState(store, ({ details }) => {
    const { audit } = details;

    for (const key in expectedState) {
      const expected = expectedState[key];
      if (expected && typeof expected === "object") {
        if (
          JSON.stringify(audit[key], parseNumReplacer) !==
          JSON.stringify(expected, parseNumReplacer)
        ) {
          return false;
        }
      } else if (audit && audit[key] !== expected) {
        return false;
      }
    }

    ok(true, "Audit state is correct.");
    return true;
  });
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

      if (key === "relations") {
        if (!relationsMatch(details.relations, expected)) {
          return false;
        }
      } else if (EXPANDABLE_PROPS.includes(key)) {
        if (
          JSON.stringify(details.accessible[key]) !== JSON.stringify(expected)
        ) {
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
 * Check the state of the accessibility related prefs.
 * @param  {Document} doc
 *         accessibility inspector panel document.
 * @param  {Object}   toolbarPrefValues
 *         Expected state of the panel prefs as well as the redux state that
 *         keeps track of it. Includes:
 *         - SCROLL_INTO_VIEW (devtools.accessibility.scroll-into-view)
 * @param  {Object}   store
 *         React store for the panel (includes store for the sidebar).
 */
async function checkToolbarPrefsState(doc, toolbarPrefValues, store) {
  info("Checking toolbar prefs state.");
  const [hasExpectedStructure] = await Promise.all([
    // Check that appropriate preferences are set as expected.
    BrowserTestUtils.waitForCondition(() => {
      return Object.keys(toolbarPrefValues).every(
        name =>
          Services.prefs.getBoolPref(PREF_KEYS[name], false) ===
          toolbarPrefValues[name]
      );
    }, "Wait for the right prefs state."),
    // Check that ui state is set as expected.
    waitUntilState(store, ({ ui }) => {
      for (const name in toolbarPrefValues) {
        if (ui[name] !== toolbarPrefValues[name]) {
          return false;
        }
      }

      ok(true, "UI pref state is correct.");
      return true;
    }),
  ]);
  ok(hasExpectedStructure, "Prefs state is correct.");
}

/**
 * Check the state of the accessibility checks toolbar.
 * @param  {Object} store
 *         React store for the panel (includes store for the sidebar).
 * @param  {Object} activeToolbarFilters
 *         Expected active state of the filters in the toolbar.
 */
async function checkToolbarState(doc, activeToolbarFilters) {
  info("Checking toolbar state.");
  const hasExpectedStructure = await BrowserTestUtils.waitForCondition(
    () =>
      [
        ...doc.querySelectorAll("#accessibility-tree-filters-menu .command"),
      ].every(
        (filter, i) =>
          (activeToolbarFilters[i] ? "true" : null) ===
          filter.getAttribute("aria-checked")
      ),
    "Wait for the right toolbar state."
  );

  ok(hasExpectedStructure, "Toolbar state is correct.");
}

/**
 * Check the state of the simulation button and menu components.
 * @param  {Object} doc         Panel document.
 * @param  {Object} expected    Expected states of the simulation components:
 *                              menuVisible, buttonActive, checkedOptionIndices (Optional)
 */
async function checkSimulationState(doc, expected) {
  const { buttonActive, checkedOptionIndices } = expected;
  const simulationMenuOptions = doc
    .querySelector(SIMULATION_MENU_BUTTON_ID + "-menu")
    .querySelectorAll(".menuitem");

  // Check simulation menu button state
  is(
    doc.querySelector(SIMULATION_MENU_BUTTON_ID).className,
    `devtools-button toolbar-menu-button simulation${
      buttonActive ? " active" : ""
    }`,
    `Simulation menu button contains ${buttonActive ? "active" : "base"} class.`
  );

  // Check simulation menu options states, if specified
  if (checkedOptionIndices) {
    simulationMenuOptions.forEach((menuListItem, index) => {
      const isChecked = checkedOptionIndices.includes(index);
      const button = menuListItem.firstChild;

      is(
        button.getAttribute("aria-checked"),
        isChecked ? "true" : null,
        `Simulation option ${index} is ${isChecked ? "" : "not "}selected.`
      );
    });
  }
}

/**
 * Focus accessibility properties tree in the a11y inspector sidebar. If focused for the
 * first time, the tree will select first rendered node as defult selection for keyboard
 * purposes.
 *
 * @param  {Document} doc  accessibility inspector panel document.
 */
async function focusAccessibleProperties(doc) {
  const tree = doc.querySelector(".tree");
  if (doc.activeElement !== tree) {
    tree.focus();
    await BrowserTestUtils.waitForCondition(
      () => tree.querySelector(".node.focused"),
      "Tree selected."
    );
  }
}

/**
 * Select accessibility property in the sidebar.
 * @param  {Document} doc  accessibility inspector panel document.
 * @param  {String} id     id of the property to be selected.
 * @return {DOMNode}       Node that corresponds to the selected accessibility property.
 */
async function selectProperty(doc, id) {
  const win = doc.defaultView;
  let selected = false;
  let node;

  await focusAccessibleProperties(doc);
  await BrowserTestUtils.waitForCondition(() => {
    node = doc.getElementById(`${id}`);
    if (node) {
      if (selected) {
        return node.firstChild.classList.contains("focused");
      }

      EventUtils.sendMouseEvent({ type: "click" }, node, win);
      selected = true;
    } else {
      const tree = doc.querySelector(".tree");
      tree.scrollTop = parseFloat(win.getComputedStyle(tree).height);
    }

    return false;
  });

  return node;
}

/**
 * Select tree row.
 * @param  {document} doc       panel documnent.
 * @param  {Number}   rowNumber number of the row/tree node to be selected.
 */
function selectRow(doc, rowNumber) {
  info(`Selecting row ${rowNumber}.`);
  EventUtils.sendMouseEvent(
    { type: "click" },
    doc.querySelectorAll(".treeRow")[rowNumber],
    doc.defaultView
  );
}

/**
 * Toggle an expandable tree row.
 * @param  {document} doc       panel documnent.
 * @param  {Number}   rowNumber number of the row/tree node to be toggled.
 */
async function toggleRow(doc, rowNumber) {
  const win = doc.defaultView;
  const row = doc.querySelectorAll(".treeRow")[rowNumber];
  const twisty = row.querySelector(".theme-twisty");
  const expected = !twisty.classList.contains("open");

  info(`${expected ? "Expanding" : "Collapsing"} row ${rowNumber}.`);

  EventUtils.sendMouseEvent({ type: "click" }, twisty, win);
  await BrowserTestUtils.waitForCondition(
    () =>
      !twisty.classList.contains("devtools-throbber") &&
      expected === twisty.classList.contains("open"),
    "Twisty updated."
  );
}

/**
 * Toggle a specific menu item based on its index in the menu.
 * @param  {document} toolboxDoc
 *         toolbox document.
 * @param  {document} doc
 *         panel document.
 * @param  {String} menuId
 *         The id of the menu (menuId passed to the MenuButton component)
 * @param  {Number}   menuItemIndex
 *         index of the menu item to be toggled.
 */
async function toggleMenuItem(doc, toolboxDoc, menuId, menuItemIndex) {
  const toolboxWin = toolboxDoc.defaultView;
  const panelWin = doc.defaultView;

  const menuButton = doc.querySelectorAll(".toolbar-menu-button")[
    MENU_INDEXES[menuId]
  ];
  ok(menuButton, "Expected menu button");

  const menuEl = toolboxDoc.getElementById(menuId);
  const menuItem = menuEl.querySelectorAll(".command")[menuItemIndex];
  ok(menuItem, "Expected menu item");

  const expected =
    menuItem.getAttribute("aria-checked") === "true" ? null : "true";

  // Make the menu visible first.
  EventUtils.synthesizeMouseAtCenter(menuButton, {}, panelWin);
  await BrowserTestUtils.waitForCondition(
    () => !!menuItem.offsetParent,
    "Menu item is visible."
  );

  EventUtils.synthesizeMouseAtCenter(menuItem, {}, toolboxWin);
  await BrowserTestUtils.waitForCondition(
    () => expected === menuItem.getAttribute("aria-checked"),
    "Menu item updated."
  );
}

async function openSimulationMenu(doc) {
  doc.querySelector(SIMULATION_MENU_BUTTON_ID).click();

  await BrowserTestUtils.waitForCondition(() =>
    doc
      .querySelector(SIMULATION_MENU_BUTTON_ID + "-menu")
      .classList.contains("tooltip-visible")
  );
}

async function toggleSimulationOption(doc, optionIndex) {
  const simulationMenu = doc.querySelector(SIMULATION_MENU_BUTTON_ID + "-menu");
  simulationMenu.querySelectorAll(".menuitem")[optionIndex].firstChild.click();

  await BrowserTestUtils.waitForCondition(
    () => !simulationMenu.classList.contains("tooltip-visible")
  );
}

async function findAccessibleFor(
  {
    toolbox: { target },
    panel: {
      accessibilityProxy: { accessibleWalkerFront },
    },
  },
  selector
) {
  const domWalker = (await target.getFront("inspector")).walker;
  const node = await domWalker.querySelector(domWalker.rootNode, selector);
  return accessibleWalkerFront.getAccessibleFor(node);
}

async function selectAccessibleForNode(env, selector) {
  const { panel, win } = env;
  const front = await findAccessibleFor(env, selector);
  const { EVENTS } = win;
  const onSelected = win.once(EVENTS.NEW_ACCESSIBLE_FRONT_SELECTED);
  panel.selectAccessible(front);
  await onSelected;
}

/**
 * Iterate over setups/tests structure and test the state of the
 * accessibility panel.
 * @param  {JSON}   tests
 *         test data that has the format of:
 *         {
 *           desc     {String}    description for better logging
 *           setup    {Function}  An optional setup that needs to be
 *                                performed before the state of the
 *                                tree and the sidebar can be checked
 *           expected {JSON}      An expected states for parts of
 *                                accessibility panel:
 *            - tree:                 state of the accessibility tree widget
 *            - sidebar:              state of the accessibility panel sidebar
 *            - audit:                state of the audit redux state of the
 *                                    panel
 *            - toolbarPrefValues:    state of the accessibility panel
 *                                    toolbar prefs and corresponding user
 *                                    preferences.
 *            - activeToolbarFilters: state of the accessibility panel
 *                                    toolbar filters.
 *         }
 * @param  {Object} env
 *         contains all relevant environment objects (same structure as the
 *         return value of 'addTestTab' funciton)
 */
async function runA11yPanelTests(tests, env) {
  for (const { desc, setup, expected } of tests) {
    info(desc);

    if (setup) {
      await setup(env);
    }

    const {
      tree,
      sidebar,
      audit,
      toolbarPrefValues,
      activeToolbarFilters,
      simulation,
    } = expected;
    if (tree) {
      await checkTreeState(env.doc, tree);
    }

    if (sidebar) {
      await checkSidebarState(env.store, sidebar);
    }

    if (activeToolbarFilters) {
      await checkToolbarState(env.doc, activeToolbarFilters);
    }

    if (toolbarPrefValues) {
      await checkToolbarPrefsState(env.doc, toolbarPrefValues, env.store);
    }

    if (typeof audit !== "undefined") {
      await checkAuditState(env.store, audit);
    }

    if (simulation) {
      await checkSimulationState(env.doc, simulation);
    }
  }
}

/**
 * Build a valid URL from an HTML snippet.
 * @param  {String}  uri      HTML snippet
 * @param  {Object}  options  options for the test
 * @return {String}     built URL
 */
function buildURL(uri, options = {}) {
  if (options.remoteIframe) {
    const srcURL = new URL(`http://example.net/document-builder.sjs`);
    srcURL.searchParams.append(
      "html",
      `<html>
        <head>
          <meta charset="utf-8"/>
          <title>Accessibility Panel Test (OOP)</title>
        </head>
        <body>${uri}</body>
      </html>`
    );
    uri = `<iframe title="Accessibility Panel Test (OOP)" src="${srcURL.href}"/>`;
  }

  return `data:text/html;charset=UTF-8,${encodeURIComponent(uri)}`;
}

/**
 * Add a test task based on the test structure and a test URL.
 * @param  {JSON}   tests    test data that has the format of:
 *                    {
 *                      desc     {String}    description for better logging
 *                      setup   {Function}   An optional setup that needs to be
 *                                           performed before the state of the
 *                                           tree and the sidebar can be checked
 *                      expected {JSON}      An expected states for the tree and
 *                                           the sidebar
 *                    }
 * @param {String}  uri      test URL
 * @param {String}  msg      a message that is printed for the test
 * @param {Object}  options  options for the test
 */
function addA11yPanelTestsTask(tests, uri, msg, options) {
  addA11YPanelTask(msg, uri, env => runA11yPanelTests(tests, env), options);
}

/**
 * A wrapper function around add_task that sets up the test environment, runs
 * the test and then disables accessibility tools.
 * @param {String}   msg    a message that is printed for the test
 * @param {String}   uri    test URL
 * @param {Function} task   task function containing the tests.
 * @param {Object}   options  options for the test
 */
function addA11YPanelTask(msg, uri, task, options = {}) {
  add_task(async function a11YPanelTask() {
    info(msg);
    const env = await addTestTab(buildURL(uri, options));
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
  executeSoon(() => target.reload());
  return once(target, waitForTargetEvent);
}

/**
 * Wait and check that the state of the accessibility tab in the toolbox is
 * correct.
 * @param {Object}   toolbox
 *                   DevTools toolbox to be checked.
 * @param {Boolean}  expected
 *                   Expected highlighted state of the accessibility tab.
 */
async function checkHighlighted(toolbox, expected) {
  await BrowserTestUtils.waitForCondition(async function() {
    const isHighlighted = await toolbox.isToolHighlighted("accessibility");
    return isHighlighted === expected;
  });
}
