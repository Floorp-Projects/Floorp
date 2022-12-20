/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - search_mode

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();
});

add_task(async function search_mode_search_not_mode() {
  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ search_mode: "" }]);
  });
});

add_task(async function search_mode_search_engine() {
  await doTest(async browser => {
    await openPopup("x");
    await UrlbarTestUtils.enterSearchMode(window);
    await waitForPauseImpression();

    assertImpressionTelemetry([{ search_mode: "search_engine" }]);
  });
});

add_task(async function search_mode_bookmarks() {
  await doTest(async browser => {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "https://example.com/bookmark",
      title: "bookmark",
    });

    await openPopup("bookmark");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });
    await selectRowByURL("https://example.com/bookmark");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ search_mode: "bookmarks" }]);
  });
});

add_task(async function search_mode_history() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  await doTest(async browser => {
    await PlacesTestUtils.addVisits("https://example.com/test");

    await openPopup("example");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    });
    await selectRowByURL("https://example.com/test");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ search_mode: "history" }]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function search_mode_tabs() {
  const tab = BrowserTestUtils.addTab(gBrowser, "https://example.com/");

  await doTest(async browser => {
    await openPopup("example");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.TABS,
    });
    await selectRowByProvider("Places");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ search_mode: "tabs" }]);
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function search_mode_actions() {
  await doTest(async browser => {
    await openPopup("add");
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    });
    await selectRowByProvider("quickactions");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ search_mode: "actions" }]);
  });
});
