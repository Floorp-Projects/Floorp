/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is the same value used by CustomizableUI tests.
const kForceOverflowWidthPx = 450;

function isActive() {
  return Services.profiler.IsActive();
}

/**
 * Force focus to an element that isn't focusable.
 * Toolbar buttons aren't focusable because if they were, clicking them would
 * focus them, which is undesirable. Therefore, they're only made focusable
 * when a user is navigating with the keyboard. This function forces focus as
 * is done during toolbar keyboard navigation.
 */
function forceFocus(elem) {
  elem.setAttribute("tabindex", "-1");
  elem.focus();
  elem.removeAttribute("tabindex");
}

async function waitForProfileAndCloseTab() {
  await waitUntil(
    () => !button.classList.contains("profiler-paused"),
    "Waiting until the profiler is no longer paused"
  );

  await checkTabLoadedProfile({
    initialTitle: "Waiting on the profile",
    successTitle: "Profile received",
    errorTitle: "Error",
  });
}
var button;
var dropmarker;

add_setup(async function () {
  info(
    "Add the profiler button to the toolbar and ensure capturing a profile loads a local url."
  );
  await setProfilerFrontendUrl(
    "http://example.com",
    "/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );
  await makeSureProfilerPopupIsEnabled();
  button = document.getElementById("profiler-button-button");
  dropmarker = document.getElementById("profiler-button-dropmarker");
});

add_task(async function click_icon() {
  info("Test that the profiler icon starts and captures a profile.");

  ok(!dropmarker.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");

  button.click();
  await getElementByTooltip(document, "The profiler is recording a profile");
  ok(isActive(), "should have started the profiler");

  button.click();
  // We're not testing for the tooltip "capturing a profile" because this might
  // be racy.
  await waitForProfileAndCloseTab();

  // Back to the inactive state.
  await getElementByTooltip(document, "Record a performance profile");
});

add_task(async function click_dropmarker() {
  info("Test that the profiler icon dropmarker opens the panel.");

  ok(!dropmarker.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");

  const popupShownPromise = waitForProfilerPopupEvent(window, "popupshown");
  dropmarker.click();
  await popupShownPromise;

  info("Ensure the panel is open and the profiler still inactive.");
  ok(dropmarker.getAttribute("open") == "true", "panel should be open");
  ok(!isActive(), "profiler should still be inactive");
  await getElementByLabel(document, "Start Recording");

  info("Press Escape to close the panel.");
  const popupHiddenPromise = waitForProfilerPopupEvent(window, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHiddenPromise;
  ok(!dropmarker.hasAttribute("open"), "panel should be closed");
});

add_task(async function click_overflowed_icon() {
  info("Test that the profiler icon opens the panel when overflowed.");

  const overflowMenu = document.getElementById("widget-overflow");
  const profilerPanel = document.getElementById("PanelUI-profiler");

  ok(!dropmarker.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");

  const navbar = document.getElementById(CustomizableUI.AREA_NAVBAR);
  ok(
    !navbar.hasAttribute("overflowing"),
    "Should start with a non-overflowing toolbar."
  );

  info("Force the toolbar to overflow.");
  const originalWindowWidth = window.outerWidth;
  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  await TestUtils.waitForCondition(() => navbar.hasAttribute("overflowing"));
  ok(navbar.hasAttribute("overflowing"), "Should have an overflowing toolbar.");

  info("Open the overflow menu.");
  const chevron = document.getElementById("nav-bar-overflow-button");
  chevron.click();
  await TestUtils.waitForCondition(() => overflowMenu.state == "open");

  info("Open the profiler panel.");
  button.click();
  await TestUtils.waitForCondition(() =>
    profilerPanel?.hasAttribute("visible")
  );

  info("Ensure the panel is open and the profiler still inactive.");
  ok(profilerPanel?.hasAttribute("visible"), "panel should be open");
  ok(!isActive(), "profiler should still be inactive");
  await getElementByLabel(document, "Start Recording");

  info("Press Escape to close the panel.");
  EventUtils.synthesizeKey("KEY_Escape");
  await TestUtils.waitForCondition(() => overflowMenu.state == "closed");
  ok(!dropmarker.hasAttribute("open"), "panel should be closed");

  info("Undo the forced toolbar overflow.");
  window.resizeTo(originalWindowWidth, window.outerHeight);
  return TestUtils.waitForCondition(() => !navbar.hasAttribute("overflowing"));
});

add_task(async function space_key() {
  info("Test that the Space key starts and captures a profile.");

  ok(!dropmarker.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");
  forceFocus(button);

  info("Press Space to start the profiler.");
  EventUtils.synthesizeKey(" ");
  ok(isActive(), "should have started the profiler");

  info("Press Space again to capture the profile.");
  EventUtils.synthesizeKey(" ");
  await waitForProfileAndCloseTab();
});

add_task(async function enter_key() {
  info("Test that the Enter key starts and captures a profile.");

  ok(!dropmarker.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");
  forceFocus(button);

  const isMacOS = Services.appinfo.OS === "Darwin";
  if (isMacOS) {
    // On macOS, pressing Enter on a focused toolbarbutton does not fire a
    // command event, so we do not expect Enter to start the profiler.
    return;
  }

  info("Pressing Enter should start the profiler.");
  EventUtils.synthesizeKey("KEY_Enter");
  ok(isActive(), "should have started the profiler");

  info("Pressing Enter again to capture the profile.");
  EventUtils.synthesizeKey("KEY_Enter");
  await waitForProfileAndCloseTab();
});
