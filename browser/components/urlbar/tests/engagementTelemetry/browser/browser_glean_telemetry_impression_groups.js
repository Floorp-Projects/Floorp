/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - groups
// - results
// - n_results

add_setup(async function () {
  await initGroupTest();
  // Increase the pausing time to ensure to ready for all suggestions.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.urlbar.searchEngagementTelemetry.pauseImpressionIntervalMs",
        500,
      ],
    ],
  });
});

add_task(async function heuristics() {
  await doHeuristicsTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        { reason: "pause", groups: "heuristic", results: "search_engine" },
      ]),
  });
});

add_task(async function adaptive_history() {
  await doAdaptiveHistoryTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,adaptive_history",
          results: "search_engine,history",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function search_history() {
  await doSearchHistoryTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,search_history,search_history",
          results: "search_engine,search_history,search_history",
          n_results: 3,
        },
      ]),
  });
});

add_task(async function recent_search() {
  await doRecentSearchTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "recent_search,suggested_index",
          results: "recent_search,action",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function search_suggest() {
  await doSearchSuggestTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,search_suggest,search_suggest",
          results: "search_engine,search_suggest,search_suggest",
          n_results: 3,
        },
      ]),
  });

  await doTailSearchSuggestTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,search_suggest",
          results: "search_engine,search_suggest",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function top_pick() {
  await doTopPickTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,top_pick,search_suggest,search_suggest",
          results:
            "search_engine,merino_top_picks,search_suggest,search_suggest",
          n_results: 4,
        },
      ]),
  });
});

add_task(async function top_site() {
  await doTopSiteTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "top_site,suggested_index",
          results: "top_site,action",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function clipboard() {
  await doClipboardTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "general,suggested_index",
          results: "clipboard,action",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function remote_tab() {
  await doRemoteTabTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,remote_tab",
          results: "search_engine,remote_tab",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function addon() {
  await doAddonTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "addon",
          results: "addon",
          n_results: 1,
        },
      ]),
  });
});

add_task(async function general() {
  await doGeneralBookmarkTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,suggested_index,general",
          results: "search_engine,action,bookmark",
          n_results: 3,
        },
      ]),
  });

  await doGeneralHistoryTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,general",
          results: "search_engine,history",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function suggest() {
  await doSuggestTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          groups: "heuristic,suggest",
          results: "search_engine,rs_adm_nonsponsored",
          n_results: 2,
        },
      ]),
  });
});

add_task(async function about_page() {
  await doAboutPageTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,about_page,about_page",
          results: "search_engine,history,history",
          n_results: 3,
        },
      ]),
  });
});

add_task(async function suggested_index() {
  await doSuggestedIndexTest({
    trigger: () => waitForPauseImpression(),
    assert: () =>
      assertImpressionTelemetry([
        {
          reason: "pause",
          groups: "heuristic,suggested_index",
          results: "search_engine,unit",
          n_results: 2,
        },
      ]),
  });
});
