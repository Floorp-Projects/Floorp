/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test the states of the profiler button, e.g. inactive, active, and capturing."
  );
  await setProfilerFrontendUrl(
    "http://example.com/browser/devtools/client/performance-new/test/browser/fake-frontend.html"
  );
  await makeSureProfilerPopupIsEnabled();

  const { toggleProfiler, captureProfile } = ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );

  const button = document.getElementById("profiler-button-button");
  if (!button) {
    throw new Error("Could not find the profiler button.");
  }

  info("The profiler button starts out inactive");
  checkButtonState(button, {
    tooltip: "Record a performance profile",
    active: false,
    paused: false,
  });

  info("Toggling the profiler turns on the active state");
  toggleProfiler("aboutprofiling");
  checkButtonState(button, {
    tooltip: "The profiler is recording a profile",
    active: true,
    paused: false,
  });

  info("Capturing a profile makes the button paused");
  captureProfile("aboutprofiling");
  checkButtonState(button, {
    tooltip: "The profiler is capturing a profile",
    active: false,
    paused: true,
  });

  waitUntil(
    () => !button.classList.contains("profiler-paused"),
    "Waiting until the profiler is no longer paused"
  );

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
function checkButtonState(button, { tooltip, active, paused }) {
  is(
    button.getAttribute("tooltiptext"),
    tooltip,
    `The tooltip for the button is "${tooltip}".`
  );
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
}
