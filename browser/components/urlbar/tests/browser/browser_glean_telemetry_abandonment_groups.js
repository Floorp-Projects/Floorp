/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - groups
// - results
// - n_results

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();
});

add_task(async function groups_heuristics() {
  await doTest(async browser => {
    await openPopup("x");
    await doBlur();

    assertAbandonmentTelemetry([
      { groups: "heuristic", results: "search_engine" },
    ]);
  });
});

add_task(async function groups_adaptive_history() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async browser => {
    await PlacesTestUtils.addVisits(["https://example.com/test"]);
    await UrlbarUtils.addToInputHistory("https://example.com/test", "examp");
    await openPopup("exa");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,adaptive_history",
        results: "search_engine,history",
        n_results: 2,
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function groups_search_history() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async browser => {
    await UrlbarTestUtils.formHistory.add(["foofoo", "foobar"]);

    await openPopup("foo");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,search_history,search_history",
        results: "search_engine,search_history,search_history",
        n_results: 3,
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function groups_search_suggest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.maxHistoricalSearchSuggestions", 2],
    ],
  });

  await doTest(async browser => {
    await openPopup("foo");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,search_suggest,search_suggest",
        results: "search_engine,search_suggest,search_suggest",
        n_results: 3,
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function groups_top_pick() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", true]],
  });

  await doTest(async browser => {
    await openPopup("sponsored");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,top_pick,search_suggest,search_suggest",
        results: "search_engine,suggest_sponsor,search_suggest,search_suggest",
        n_results: 4,
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
  cleanupQuickSuggest();
});

add_task(async function groups_top_site() {
  await doTest(async browser => {
    await addTopSites("https://example.com/");
    await showResultByArrowDown();
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "top_site,suggested_index",
        results: "top_site,action",
        n_results: 2,
      },
    ]);
  });
});

add_task(async function group_remote_tab() {
  const remoteTab = await loadRemoteTab("https://example.com");

  await doTest(async browser => {
    await openPopup("example");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,remote_tab",
        results: "search_engine,remote_tab",
        n_results: 2,
      },
    ]);
  });

  await remoteTab.unload();
});

add_task(async function group_addon() {
  const addon = loadOmniboxAddon({ keyword: "omni" });
  await addon.startup();

  await doTest(async browser => {
    await openPopup("omni test");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "addon",
        results: "addon",
        n_results: 1,
      },
    ]);
  });

  await addon.unload();
});

add_task(async function group_general() {
  await doTest(async browser => {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "https://example.com/bookmark",
      title: "bookmark",
    });

    await openPopup("bookmark");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,suggested_index,general",
        results: "search_engine,action,bookmark",
        n_results: 3,
      },
    ]);
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });
  await doTest(async browser => {
    await PlacesTestUtils.addVisits("https://example.com/test");

    await openPopup("example");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,general",
        results: "search_engine,history",
        n_results: 2,
      },
    ]);
  });
  await SpecialPowers.popPrefEnv();
});

add_task(async function group_suggest() {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", false]],
  });

  await doTest(async browser => {
    await openPopup("nonsponsored");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,suggest",
        results: "search_engine,suggest_non_sponsor",
        n_results: 2,
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
  cleanupQuickSuggest();
});

add_task(async function group_about_page() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxRichResults", 3]],
  });

  await doTest(async browser => {
    await openPopup("about:");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,about_page,about_page",
        results: "search_engine,history,history",
        n_results: 3,
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function group_suggested_index() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.unitConversion.enabled", true]],
  });

  await doTest(async browser => {
    await openPopup("1m to cm");
    await doBlur();

    assertAbandonmentTelemetry([
      {
        groups: "heuristic,suggested_index",
        results: "search_engine,unit",
        n_results: 2,
      },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});
