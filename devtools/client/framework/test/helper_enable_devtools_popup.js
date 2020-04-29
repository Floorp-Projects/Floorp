/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { listenOnce } = require("devtools/shared/async-utils");

/**
 * Helpers dedicated to the browser_enable_devtools_popup* tests.
 * Those tests usually test the same exact things, but using different
 * configurations.
 */

function openDevToolsWithKey(key, modifiers) {
  const onToolboxReady = gDevTools.once("toolbox-ready");
  EventUtils.synthesizeKey(key, modifiers);
  return onToolboxReady;
}
/* exported openDevToolsWithKey */

function closeDevToolsWithKey(toolbox, key, modifiers) {
  const onToolboxDestroyed = toolbox.once("destroyed");
  EventUtils.synthesizeKey(key, modifiers);
  return onToolboxDestroyed;
}
/* exported closeDevToolsWithKey */

/**
 * The popup element might still be in its template wrapper.
 */
function unwrapEnableDevToolsPopup(tab) {
  const panelWrapper = tab.ownerDocument.getElementById(
    "wrapper-enable-devtools-popup"
  );
  if (panelWrapper) {
    info("Unwrapping enable devtools popup");
    panelWrapper.replaceWith(panelWrapper.content);
  }
}

/**
 * Test if F12 is currently disabled:
 * - press F12 -> popup is displayed
 * - press F12 again -> popup is hidden
 * - no toolbox was opened during the process
 */
async function checkF12IsDisabled(tab) {
  unwrapEnableDevToolsPopup(tab);

  const popup = tab.ownerDocument.getElementById("enable-devtools-popup");
  is(popup.state, "closed", "The enable devtools popup is initially hidden");

  const failOnToolboxReady = () => {
    ok(false, "The devtools toolbox should not open");
  };
  gDevTools.on("toolbox-ready", failOnToolboxReady);

  info("Press F12 and wait for the enable devtools popup to be displayed");
  const onPopupShown = listenOnce(popup, "popupshown");
  EventUtils.synthesizeKey("VK_F12");
  await onPopupShown;
  is(popup.state, "open", "The enable devtools popup is now visible");

  info("Press F12 again and wait for the enable devtools popup to hide");
  const onPopupHidden = listenOnce(popup, "popuphidden");
  EventUtils.synthesizeKey("VK_F12");
  await onPopupHidden;
  is(popup.state, "closed", "The enable devtools popup is hidden again");

  gDevTools.off("toolbox-ready", failOnToolboxReady);
}
/* exported checkF12IsDisabled */

/**
 * Test that DevTools can be open with another keyboard shortcut than F12.
 * The enable-devtools popup should not be displayed.
 */
async function openDevToolsWithInspectorKey(tab) {
  unwrapEnableDevToolsPopup(tab);

  info("Open DevTools via another shortcut (only F12 should be disabled)");
  const popup = tab.ownerDocument.getElementById("enable-devtools-popup");

  // We are going to use F12 but the popup should never show up.
  const failOnPopupShown = () => {
    ok(false, "The enable devtools popup should not be displayed");
  };
  popup.addEventListener("popupshown", failOnPopupShown);

  const toolbox = await openDevToolsWithKey("I", {
    accelKey: true,
    shiftKey: !navigator.userAgent.match(/Mac/),
    altKey: navigator.userAgent.match(/Mac/),
  });

  is(popup.state, "closed", "The enable devtools popup is still hidden");
  popup.removeEventListener("popupshown", failOnPopupShown);

  return toolbox;
}
/* exported openDevToolsWithInspectorKey */

/**
 * Test that the toolbox can be closed with F12, without triggering the popup.
 */
async function closeDevToolsWithF12(tab, toolbox) {
  unwrapEnableDevToolsPopup(tab);

  const popup = tab.ownerDocument.getElementById("enable-devtools-popup");

  // We are going to use F12 but the popup should never show up.
  const failOnPopupShown = () => {
    ok(false, "The enable devtools popup should not be displayed");
  };
  popup.addEventListener("popupshown", failOnPopupShown);

  info("Press F12 and wait for the toolbox to be destroyed");
  await closeDevToolsWithKey(toolbox, "VK_F12");
  is(popup.state, "closed", "The enable devtools popup is still hidden");

  popup.removeEventListener("popupshown", failOnPopupShown);
}
/* exported closeDevToolsWithF12 */

/**
 * Test if F12 is enabled:
 * - press F12 -> toolbox opens
 * - press F12 -> toolbox closes
 * - no enable devtools popup was opened during the process
 */
async function checkF12IsEnabled(tab) {
  unwrapEnableDevToolsPopup(tab);

  const popup = tab.ownerDocument.getElementById("enable-devtools-popup");

  // We are going to use F12 several times, but the popup should never show up.
  // Add a listener on popupshown to make sure this doesn't happen
  const failOnPopupShown = () => {
    ok(false, "The enable devtools popup should not be displayed");
  };
  popup.addEventListener("popupshown", failOnPopupShown);

  info("Check that F12 can now open the toolbox.");
  const toolbox = await openDevToolsWithKey("VK_F12");
  is(popup.state, "closed", "The enable devtools popup is still hidden");

  info("Press F12 and wait for the toolbox to be destroyed");
  await closeDevToolsWithKey(toolbox, "VK_F12");
  is(popup.state, "closed", "The enable devtools popup is still hidden");

  // cleanup
  popup.removeEventListener("popupshown", failOnPopupShown);
}
/* exported checkF12IsEnabled */
