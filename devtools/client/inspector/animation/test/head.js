/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../test/head.js */
// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js", this);

const TAB_NAME = "newanimationinspector";

// Enable new animation inspector.
Services.prefs.setBoolPref("devtools.new-animationinspector.enabled", true);

// Auto clean-up when a test ends.
// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.new-animationinspector.enabled");
});

/**
 * Open the toolbox, with the inspector tool visible and the animationinspector
 * sidebar selected.
 *
 * @return {Promise} that resolves when the inspector is ready.
 */
const openAnimationInspector = async function () {
  const { inspector, toolbox } = await openInspectorSidebarTab(TAB_NAME);
  const { animationinspector: animationInspector } = inspector;
  const panel = inspector.panelWin.document.getElementById("animation-container");
  return { toolbox, inspector, animationInspector, panel };
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
