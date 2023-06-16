/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test engagement telemetry with persisted search terms disabled.

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/engagementTelemetry/browser/head-interaction.js",
  this
);

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_setup(async function () {
  await initInteractionTest();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", false]],
  });
});

add_task(async function persisted_search_terms() {
  await doPersistedSearchTermsTest({
    trigger: () => doEnter(),
    assert: () =>
      assertEngagementTelemetry([
        { interaction: "typed" },
        { interaction: "typed" },
      ]),
  });
});

add_task(async function persisted_search_terms_restarted_refined() {
  await doPersistedSearchTermsRestartedRefinedTest({
    enabled: false,
    trigger: () => doEnter(),
    assert: expected =>
      assertEngagementTelemetry([
        { interaction: "typed" },
        { interaction: expected },
      ]),
  });
});

add_task(
  async function persisted_search_terms_restarted_refined_via_abandonment() {
    await doPersistedSearchTermsRestartedRefinedViaAbandonmentTest({
      enabled: false,
      trigger: () => doEnter(),
      assert: expected =>
        assertEngagementTelemetry([
          { interaction: "typed" },
          { interaction: expected },
        ]),
    });
  }
);
