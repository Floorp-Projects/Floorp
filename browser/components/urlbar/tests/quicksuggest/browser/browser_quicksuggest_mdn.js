/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for mdn suggestions.

const REMOTE_SETTINGS_DATA = [
  {
    type: "mdn-suggestions",
    attachment: [
      {
        url: "https://example.com/array-filter",
        title: "Array.prototype.filter()",
        description:
          "The filter() method creates a shallow copy of a portion of a given array, filtered down to just the elements from the given array that pass the test implemented by the provided function.",
        keywords: ["array"],
      },
    ],
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.quicksuggest.nonsponsored", true],
      ["browser.urlbar.quicksuggest.remoteSettings.enabled", true],
      ["browser.urlbar.bestMatch.enabled", true],
      ["browser.urlbar.suggest.mdn", true],
    ],
  });

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsResults: REMOTE_SETTINGS_DATA,
  });
});

add_task(async function basic() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.mdn.featureGate", true]],
  });
  await waitForSuggestions();

  const suggestion = REMOTE_SETTINGS_DATA[0].attachment[0];
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: suggestion.keywords[0],
  });
  Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

  const { element, result } = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    1
  );
  Assert.equal(
    result.providerName,
    UrlbarProviderQuickSuggest.name,
    "The result should be from the expected provider"
  );
  Assert.equal(result.payload.provider, "MDNSuggestions");

  const onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    suggestion.url
  );
  EventUtils.synthesizeMouseAtCenter(element.row, {});
  await onLoad;
  Assert.ok(true, "Expected page is loaded");

  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
});

add_task(async function disable() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.mdn.featureGate", false]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "array",
  });
  Assert.equal(UrlbarTestUtils.getResultCount(window), 1);

  const { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.providerName, "HeuristicFallback");

  await SpecialPowers.popPrefEnv();
});

async function waitForSuggestions() {
  const keyword = REMOTE_SETTINGS_DATA[0].attachment[0].keywords[0];
  const feature = QuickSuggest.getFeature("MDNSuggestions");
  await TestUtils.waitForCondition(async () => {
    const suggestions = await feature.queryRemoteSettings(keyword);
    return !!suggestions.length;
  }, "Waiting for MDNSuggestions to serve remote settings suggestions");
}
