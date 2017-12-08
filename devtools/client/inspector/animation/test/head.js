/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../test/head.js */
// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js", this);

const COMMON_FRAME_SCRIPT_URL = "chrome://devtools/content/shared/frame-script-utils.js";
const FRAME_SCRIPT_URL = CHROME_URL_ROOT + "doc_frame_script.js";
const TAB_NAME = "newanimationinspector";

const ANIMATION_L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");

// Enable new animation inspector.
Services.prefs.setBoolPref("devtools.new-animationinspector.enabled", true);

// Auto clean-up when a test ends.
// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.new-animationinspector.enabled");
  Services.prefs.clearUserPref("devtools.toolsidebar-width.inspector");
});

/**
 * Open the toolbox, with the inspector tool visible and the animationinspector
 * sidebar selected.
 *
 * @return {Promise} that resolves when the inspector is ready.
 */
const openAnimationInspector = async function () {
  const { inspector, toolbox } = await openInspectorSidebarTab(TAB_NAME);
  await inspector.once("inspector-updated");
  const { animationinspector: animationInspector } = inspector;
  const panel = inspector.panelWin.document.getElementById("animation-container");
  return { animationInspector, toolbox, inspector, panel };
};

/**
 * Close the toolbox.
 *
 * @return {Promise} that resolves when the toolbox has closed.
 */
const closeAnimationInspector = async function () {
  const target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.closeToolbox(target);
};

/**
 * Some animation features are not enabled by default in release/beta channels
 * yet including:
 *   * parts of the Web Animations API (Bug 1264101), and
 *   * the frames() timing function (Bug 1379582).
 */
const enableAnimationFeatures = function () {
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["dom.animations-api.core.enabled", true],
      ["layout.css.frames-timing.enabled", true],
    ]}, resolve);
  });
};

/**
 * Add a new test tab in the browser and load the given url.
 *
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
const _addTab = addTab;
addTab = async function (url) {
  await enableAnimationFeatures();
  const tab = await _addTab(url);
  const browser = tab.linkedBrowser;
  info("Loading the helper frame script " + FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
  info("Loading the helper frame script " + COMMON_FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(COMMON_FRAME_SCRIPT_URL, false);
  return tab;
};

/**
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector and wait for the animations to be displayed
 *
 * @param {String|NodeFront}
 *        data The node to select
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not
 *        to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 *                   and animations of its subtree are properly displayed.
 */
const selectNodeAndWaitForAnimations = async function (data, inspector, reason = "test") {
  // We want to make sure the rest of the test waits for the animations to
  // be properly displayed (wait for all target DOM nodes to be previewed).
  const onUpdated = inspector.once("inspector-updated");
  await selectNode(data, inspector, reason);
  await onUpdated;
};

/**
 * Set the sidebar width by given parameter.
 *
 * @param {String} width
 *        Change sidebar width by given parameter.
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return {Promise} Resolves when the sidebar size changed.
 */
const setSidebarWidth = async function (width, inspector) {
  const onUpdated = inspector.toolbox.once("inspector-sidebar-resized");
  inspector.splitBox.setState({ width });
  await onUpdated;
};
