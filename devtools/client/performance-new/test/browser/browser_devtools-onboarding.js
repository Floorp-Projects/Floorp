/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ONBOARDING_PREF = "devtools.performance.new-panel-onboarding";

add_task(async function testWithOnboardingPreferenceFalse() {
  info("Test that the onboarding message is displayed as expected.");

  info("Test the onboarding message when the preference is false");
  await SpecialPowers.pushPrefEnv({
    set: [[ONBOARDING_PREF, false]],
  });
  await withDevToolsPanel(async document => {
    {
      // Wait for another UI element to be rendered before asserting the
      // onboarding message.
      await getActiveButtonFromText(document, "Start recording");
      ok(
        !isOnboardingDisplayed(document),
        "Onboarding message is not displayed"
      );
    }
  });
});

add_task(async function testWithOnboardingPreferenceTrue() {
  info("Test the onboarding message when the preference is true");
  await SpecialPowers.pushPrefEnv({
    set: [[ONBOARDING_PREF, true]],
  });

  await withDevToolsPanel(async document => {
    await waitUntil(
      () => isOnboardingDisplayed(document),
      "Waiting for the onboarding message to be displayed"
    );
    ok(true, "Onboarding message is displayed");
    await closeOnboardingMessage(document);
  });

  is(
    Services.prefs.getBoolPref(ONBOARDING_PREF),
    false,
    "onboarding preference should be false after closing the message"
  );
});

add_task(async function testWithOnboardingPreferenceNotSet() {
  info("Test the onboarding message when the preference is not set");
  await SpecialPowers.pushPrefEnv({
    clear: [[ONBOARDING_PREF]],
  });

  await withDevToolsPanel(async document => {
    await waitUntil(
      () => isOnboardingDisplayed(document),
      "Waiting for the onboarding message to be displayed"
    );
    ok(true, "Onboarding message is displayed");
    await closeOnboardingMessage(document);
  });

  is(
    Services.prefs.getBoolPref(ONBOARDING_PREF),
    false,
    "onboarding preference should be false after closing the message"
  );
});

/**
 * Helper to close the onboarding message by clicking on the close button.
 */
async function closeOnboardingMessage(document) {
  const closeButton = await getActiveButtonFromText(
    document,
    "Close the onboarding message"
  );
  info("Click the close button to hide the onboarding message.");
  closeButton.click();

  await waitUntil(
    () => !isOnboardingDisplayed(document),
    "Waiting for the onboarding message to disappear"
  );
}

function isOnboardingDisplayed(document) {
  return maybeGetElementFromDocumentByText(
    document,
    "Firefox Profiler is now integrated into Developer Tools"
  );
}
