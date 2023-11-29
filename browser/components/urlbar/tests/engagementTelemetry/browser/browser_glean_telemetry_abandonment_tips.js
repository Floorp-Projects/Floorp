/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for abandonment telemetry for tips using Glean.

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser-tips/head.js",
  this
);

add_setup(async function () {
  Services.fog.setMetricsFeatureConfig(
    JSON.stringify({ "urlbar.abandonment": false })
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.searchTips.test.ignoreShowLimits", true],
      ["browser.urlbar.showSearchTerms.featureGate", true],
    ],
  });
  const engine = await SearchTestUtils.promiseNewSearchEngine({
    url: "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/searchSuggestionEngine.xml",
  });
  const originalDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.moveEngine(engine, 0);

  registerCleanupFunction(async function () {
    Services.fog.setMetricsFeatureConfig("{}");
    await SpecialPowers.popPrefEnv();
    await Services.search.setDefault(
      originalDefaultEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
    resetSearchTipsProvider();
  });
});

add_task(async function tip_persist() {
  await doTest(async browser => {
    await showPersistSearchTip("test");
    gURLBar.focus();
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });

    assertAbandonmentTelemetry([{ results: "tip_persist" }]);
  });
});

add_task(async function mouse_down_with_tip() {
  await doTest(async browser => {
    await showPersistSearchTip("test");
    await UrlbarTestUtils.promisePopupClose(window, () => {
      // We intentionally turn off this a11y check, because the following click
      // is sent to test the telemetry behavior using an alternative way of the
      // urlbar dismissal, where other ways are accessible, therefore this test
      // can be ignored.
      AccessibilityUtils.setEnv({
        mustHaveAccessibleRule: false,
      });
      EventUtils.synthesizeMouseAtCenter(browser, {});
      AccessibilityUtils.resetEnv();
    });

    assertAbandonmentTelemetry([{ results: "tip_persist" }]);
  });
});

add_task(async function mouse_down_without_tip() {
  await doTest(async browser => {
    // We intentionally turn off this a11y check, because the following click
    // is sent to test the telemetry behavior using an alternative way of the
    // urlbar dismissal, where other ways are accessible, therefore this test
    // can be ignored.
    AccessibilityUtils.setEnv({
      mustHaveAccessibleRule: false,
    });
    EventUtils.synthesizeMouseAtCenter(browser, {});
    AccessibilityUtils.resetEnv();

    assertAbandonmentTelemetry([]);
  });
});

async function showPersistSearchTip(word) {
  await openPopup(word);
  await doEnter();
  await BrowserTestUtils.waitForCondition(async () => {
    for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
      const detail = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
      if (detail.result.payload?.type === "searchTip_persist") {
        return true;
      }
    }
    return false;
  });
}
