/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const A11Y_INDICATOR_ENABLED_PREF = "accessibility.indicator.enabled";

/**
 * Test various pref and UI properties based on whether the accessibility
 * indicator is enabled and the accessibility service is initialized.
 * @param  {Object}  win      browser window to check the indicator in.
 * @param  {Boolean} enabled  pref flag for accessibility indicator.
 * @param  {Boolean} active   whether accessibility service is started or not.
 */
function testIndicatorState(win, enabled, active) {
  is(Services.prefs.getBoolPref(A11Y_INDICATOR_ENABLED_PREF), enabled,
    `Indicator is ${enabled ? "enabled" : "disabled"}.`);
  is(Services.appinfo.accessibilityEnabled, active,
    `Accessibility service is ${active ? "enabled" : "disabled"}.`);

  let visible = enabled && active;
  is(win.document.documentElement.hasAttribute("accessibilitymode"), visible,
    `accessibilitymode flag is ${visible ? "set" : "unset"}.`);

  // Browser UI has 2 indicators in markup for OSX and Windows but only 1 is
  // shown depending on whether the titlebar is enabled.
  let expectedVisibleCount = visible ? 1 : 0;
  let visibleCount = 0;
  [...win.document.querySelectorAll(".accessibility-indicator")].forEach(indicator =>
    win.getComputedStyle(indicator).getPropertyValue("display") !== "none" &&
    visibleCount++);
  is(expectedVisibleCount, visibleCount,
    `Indicator is ${visible ? "visible" : "invisible"}.`);
}

/**
 * Instantiate accessibility service and wait for event associated with its
 * startup, if necessary.
 */
async function initAccessibilityService() {
  let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    Ci.nsIAccessibilityService);

  if (!Services.appinfo.accessibilityEnabled) {
    await new Promise(resolve => {
      let observe = (subject, topic, data) => {
        Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
        // "1" indicates that the accessibility service is initialized.
        data === "1" && resolve();
      };
      Services.obs.addObserver(observe, "a11y-init-or-shutdown");
    });
  }

  return accService;
}

/**
 * If accessibility service is not yet disabled, wait for the event associated
 * with its shutdown.
 */
async function shutdownAccessibilityService() {
  if (Services.appinfo.accessibilityEnabled) {
    await new Promise(resolve => {
      let observe = (subject, topic, data) => {
        Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
        // "1" indicates that the accessibility service is shutdown.
        data === "0" && resolve();
      };
      Services.obs.addObserver(observe, "a11y-init-or-shutdown");
    });
  }
}

/**
 * Force garbage collection.
 */
function forceGC() {
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
}

add_task(async function test_accessibility_indicator() {
  info("Test default accessibility indicator state.");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  testIndicatorState(window, true, false);
  testIndicatorState(newWin, true, false);

  info("Enable accessibility and ensure the indicator is shown in all windows.");
  let accService = await initAccessibilityService(); // eslint-disable-line no-unused-vars
  testIndicatorState(window, true, true);
  testIndicatorState(newWin, true, true);

  info("Open a new window and ensure the indicator is shown there by default.");
  let dynamicWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  testIndicatorState(dynamicWin, true, true);
  await BrowserTestUtils.closeWindow(dynamicWin);

  info("Preff off accessibility indicator.");
  Services.prefs.setBoolPref(A11Y_INDICATOR_ENABLED_PREF, false);
  testIndicatorState(window, false, true);
  testIndicatorState(newWin, false, true);
  dynamicWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  testIndicatorState(dynamicWin, false, true);

  info("Preff on accessibility indicator.");
  Services.prefs.setBoolPref(A11Y_INDICATOR_ENABLED_PREF, true);
  testIndicatorState(window, true, true);
  testIndicatorState(newWin, true, true);
  testIndicatorState(dynamicWin, true, true);

  info("Disable accessibility and ensure the indicator is hidden in all windows.");
  accService = undefined;
  forceGC();
  await shutdownAccessibilityService();
  testIndicatorState(window, true, false);
  testIndicatorState(newWin, true, false);
  testIndicatorState(dynamicWin, true, false);

  Services.prefs.clearUserPref(A11Y_INDICATOR_ENABLED_PREF);
  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.closeWindow(dynamicWin);
});
