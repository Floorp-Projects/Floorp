/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - search_mode

add_setup(async function () {
  await initSearchModeTest();
  // Increase the pausing time to ensure entering search mode.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.urlbar.searchEngagementTelemetry.pauseImpressionIntervalMs",
        1000,
      ],
    ],
  });
});

add_task(async function not_search_mode() {
  await doNotSearchModeTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", search_mode: "" }]),
  });
});

add_task(async function search_engine() {
  await doSearchEngineTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        { reason: "pause", search_mode: "search_engine" },
      ]),
  });
});

add_task(async function bookmarks() {
  await doBookmarksTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        { reason: "pause", search_mode: "bookmarks" },
      ]),
  });
});

add_task(async function history() {
  await doHistoryTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", search_mode: "history" }]),
  });
});

add_task(async function tabs() {
  await doTabTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", search_mode: "tabs" }]),
  });
});

add_task(async function actions() {
  await doActionsTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([{ reason: "pause", search_mode: "actions" }]),
  });
});
