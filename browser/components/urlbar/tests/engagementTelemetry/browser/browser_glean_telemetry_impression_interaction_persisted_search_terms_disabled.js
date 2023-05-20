/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test impression telemetry with persisted search terms disabled.

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
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        { reason: "pause" },
        { reason: "pause", interaction: "typed" },
      ]),
  });
});

add_task(async function persisted_search_terms_restarted_refined() {
  await doPersistedSearchTermsRestartedRefinedTest({
    enabled: false,
    trigger: () => waitForPauseImpression(),
    assert: expected =>
      assertImpressionTelemetry([
        { reason: "pause" },
        { reason: "pause", interaction: expected },
      ]),
  });
});

add_task(
  async function persisted_search_terms_restarted_refined_via_abandonment() {
    await doPersistedSearchTermsRestartedRefinedViaAbandonmentTest({
      enabled: false,
      trigger: () => waitForPauseImpression(),
      assert: expected =>
        assertImpressionTelemetry([
          { reason: "pause" },
          { reason: "pause" },
          { reason: "pause", interaction: expected },
        ]),
    });
  }
);
