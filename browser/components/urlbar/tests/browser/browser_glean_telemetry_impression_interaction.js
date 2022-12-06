/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of impression telemetry.
// - interaction

/* import-globals-from head-glean.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-glean.js",
  this
);

add_setup(async function() {
  await setup();
});

add_task(async function interaction_topsites() {
  await doTest(async browser => {
    await addTopSites("https://example.com/");
    await showResultByArrowDown();
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "topsites" }]);
  });
});

add_task(async function interaction_typed() {
  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "typed" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "typed" }]);
  });
});

add_task(async function interaction_pasted() {
  await doTest(async browser => {
    await doPaste("www.example.com");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "pasted" }]);
  });

  await doTest(async browser => {
    await showResultByArrowDown();
    await doPaste("x");
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "pasted" }]);
  });
});

add_task(async function interaction_topsite_search() {
  // TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=1804010
  // assertImpressionTelemetry([{ interaction: "topsite_search" }]);
});

add_task(async function interaction_returned() {
  await doTest(async browser => {
    await addTopSites("https://example.com/");

    gURLBar.value = "example.com";
    gURLBar.setPageProxyState("invalid");
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      document.getElementById("Browser:OpenLocation").doCommand();
    });
    await UrlbarTestUtils.promiseSearchComplete(window);
    await waitForPauseImpression();

    assertImpressionTelemetry([{ reason: "pause", interaction: "returned" }]);
  });
});

add_task(async function interaction_restarted() {
  await doTest(async browser => {
    await openPopup("search");
    await waitForPauseImpression();
    await doBlur();
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      document.getElementById("Browser:OpenLocation").doCommand();
    });
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    await waitForPauseImpression();

    assertImpressionTelemetry([
      { reason: "pause", interaction: "typed" },
      { reason: "pause", interaction: "restarted" },
    ]);
  });
});

add_task(async function interaction_refined() {
  // The following tests do a second search immediately
  // after an initial search. The showSearchTerms feature has to be
  // disabled as subsequent searches from a default SERP are a
  // a persisted_search_terms interaction.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", false]],
  });

  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();
    await doEnter();

    await openPopup("x y");
    await waitForPauseImpression();

    assertImpressionTelemetry([
      { reason: "pause", interaction: "typed" },
      { reason: "pause", interaction: "refined" },
    ]);
  });

  await doTest(async browser => {
    await openPopup("x y");
    await waitForPauseImpression();
    await doEnter();

    await openPopup("x");
    await waitForPauseImpression();

    assertImpressionTelemetry([
      { reason: "pause", interaction: "typed" },
      { reason: "pause", interaction: "refined" },
    ]);
  });

  await doTest(async browser => {
    await openPopup("x");
    await waitForPauseImpression();
    await doEnter();

    await openPopup("y z");
    await waitForPauseImpression();

    assertImpressionTelemetry([
      { reason: "pause", interaction: "typed" },
      { reason: "pause", interaction: "typed" },
    ]);
  });

  await doTest(async browser => {
    await openPopup("x y");
    await waitForPauseImpression();
    await doEnter();

    await openPopup("x y");
    await waitForPauseImpression();

    assertImpressionTelemetry([
      { reason: "pause", interaction: "typed" },
      { reason: "pause", interaction: "typed" },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function interaction_persisted_search_terms() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.showSearchTerms.featureGate", true],
      ["browser.urlbar.showSearchTerms.enabled", true],
      ["browser.search.widget.inNavBar", false],
    ],
  });

  await doTest(async browser => {
    await openPopup("keyword");
    await waitForPauseImpression();
    await doEnter();

    await openPopup("keyword");
    await waitForPauseImpression();

    assertImpressionTelemetry([
      { reason: "pause", interaction: "typed" },
      { reason: "pause", interaction: "persisted_search_terms" },
    ]);
  });

  await SpecialPowers.popPrefEnv();
});
