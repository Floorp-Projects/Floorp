/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - interaction

add_setup(async function () {
  await initInteractionTest();
});

add_task(async function topsites() {
  await doTopsitesTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", interaction: "topsites" }]),
  });
});

add_task(async function typed() {
  await doTypedTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", interaction: "typed" }]),
  });

  await doTypedWithResultsPopupTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", interaction: "typed" }]),
  });
});

add_task(async function pasted() {
  await doPastedTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", interaction: "pasted" }]),
  });

  await doPastedWithResultsPopupTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", interaction: "pasted" }]),
  });
});

add_task(async function topsite_search() {
  await doTopsitesSearchTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        { reason: "pause", interaction: "topsite_search" },
      ]),
  });
});

add_task(async function returned_restarted_refined() {
  await doReturnedRestartedRefinedTest({
    trigger: () => waitForPauseImpression(),
    assert: expected =>
      assertImpressionTelemetry([
        { reason: "pause" },
        { reason: "pause", interaction: expected },
      ]),
  });
});
