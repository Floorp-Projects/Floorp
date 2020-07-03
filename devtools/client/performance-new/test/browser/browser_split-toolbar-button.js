/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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

add_task(async function setup() {
  info(
    "Add the profiler button to the toolbar and ensure capturing a profile loads a local url."
  );
  await setProfilerFrontendUrl(
    "http://example.com/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );
  await makeSureProfilerPopupIsEnabled();
  button = document.getElementById("profiler-button");
});

add_task(async function click_icon() {
  info("Test that the profiler icon starts and captures a profile.");

  ok(!button.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");

  await EventUtils.synthesizeMouseAtCenter(button.icon, {});
  ok(isActive(), "should have started the profiler");

  await EventUtils.synthesizeMouseAtCenter(button.icon, {});
  await waitForProfileAndCloseTab();
});

add_task(async function click_dropmarker() {
  info("Test that the profiler icon dropmarker opens the panel.");

  ok(!button.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");

  const popupShownPromise = waitForProfilerPopupEvent("popupshown");
  await EventUtils.synthesizeMouseAtCenter(button.dropmarker, {});
  await popupShownPromise;

  info("Ensure the panel is open and the profiler still inactive.");
  ok(button.getAttribute("open") == "true", "panel should be open");
  ok(!isActive(), "profiler should still be inactive");
  await getElementByLabel(document, "Start Recording");

  info("Press Escape to close the panel.");
  const popupHiddenPromise = waitForProfilerPopupEvent("popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHiddenPromise;
  ok(!button.hasAttribute("open"), "panel should be closed");
});

add_task(async function space_key() {
  info("Test that the Space key starts and captures a profile.");

  ok(!button.hasAttribute("open"), "should start with the panel closed");
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

  ok(!button.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");
  forceFocus(button);

  info("Pressing Enter should start the profiler.");
  EventUtils.synthesizeKey("KEY_Enter");
  ok(isActive(), "should have started the profiler");

  info("Pressing Enter again to capture the profile.");
  EventUtils.synthesizeKey("KEY_Enter");
  await waitForProfileAndCloseTab();
});

add_task(async function arrowDown_key() {
  info("Test that ArrowDown key dropmarker opens the panel.");

  ok(!button.hasAttribute("open"), "should start with the panel closed");
  ok(!isActive(), "should start with the profiler inactive");
  forceFocus(button);

  info("Pressing the down arrow should open the panel.");
  const popupShownPromise = waitForProfilerPopupEvent("popupshown");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await popupShownPromise;
  ok(!isActive(), "profiler should still be inactive");
  ok(button.getAttribute("open") == "true", "panel should be open");

  info("Press Escape to close the panel.");
  const popupHiddenPromise = waitForProfilerPopupEvent("popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHiddenPromise;
  ok(!button.hasAttribute("open"), "panel should be closed");
});
