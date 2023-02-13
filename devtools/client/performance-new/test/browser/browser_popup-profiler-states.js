/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test the states of the profiler button, e.g. inactive, active, and capturing."
  );
  await setProfilerFrontendUrl(
    "http://example.com",
    "/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );
  await makeSureProfilerPopupIsEnabled();

  const { toggleProfiler, captureProfile } = ChromeUtils.import(
    "resource://devtools/client/performance-new/shared/background.jsm.js"
  );

  const button = document.getElementById("profiler-button-button");
  if (!button) {
    throw new Error("Could not find the profiler button.");
  }

  info("The profiler button starts out inactive");
  await checkButtonState(button, {
    tooltip: "Record a performance profile",
    active: false,
    paused: false,
  });

  info("Toggling the profiler turns on the active state");
  toggleProfiler("aboutprofiling");
  await checkButtonState(button, {
    tooltip: "The profiler is recording a profile",
    active: true,
    paused: false,
  });

  info("Capturing a profile makes the button paused");
  captureProfile("aboutprofiling");

  // The state "capturing" can be very quick, so waiting for the tooltip
  // translation is racy. Let's only check the button's states.
  await checkButtonState(button, {
    active: false,
    paused: true,
  });

  await waitUntil(
    () => !button.classList.contains("profiler-paused"),
    "Waiting until the profiler is no longer paused"
  );

  await checkButtonState(button, {
    tooltip: "Record a performance profile",
    active: false,
    paused: false,
  });

  await checkTabLoadedProfile({
    initialTitle: "Waiting on the profile",
    successTitle: "Profile received",
    errorTitle: "Error",
  });
});

/**
 * This check dives into the implementation details of the button, mainly
 * because it's hard to provide a user-focused interpretation of button
 * stylings.
 */
async function checkButtonState(button, { tooltip, active, paused }) {
  is(
    button.classList.contains("profiler-active"),
    active,
    `The expected profiler button active state is: ${active}`
  );
  is(
    button.classList.contains("profiler-paused"),
    paused,
    `The expected profiler button paused state is: ${paused}`
  );

  if (tooltip) {
    // Let's also check the tooltip, but because the translation happens
    // asynchronously, we need a waiting mechanism.
    await getElementByTooltip(document, tooltip);
  }
}
