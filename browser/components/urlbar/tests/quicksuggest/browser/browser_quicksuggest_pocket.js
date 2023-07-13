/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Browser tests for Pocket suggestions.

const REMOTE_SETTINGS_DATA = [
  {
    type: "pocket-suggestions",
    attachment: [
      {
        url: "https://example.com/pocket-suggestion",
        title: "Pocket Suggestion",
        description: "Pocket description",
        lowConfidenceKeywords: ["pocket suggestion"],
        highConfidenceKeywords: ["high"],
      },
    ],
  },
];

add_setup(async function () {
  // This must be done before enabling the feature (using the `featureGate`
  // pref) so that the mock remote settings are set up first. Also, don't pass
  // in the remote settings data yet; see below.
  await QuickSuggestTestUtils.ensureQuickSuggestInit();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.bestMatch.enabled", true],
      ["browser.urlbar.pocket.featureGate", true],
      // Disable search suggestions so we don't hit the network.
      ["browser.search.suggest.enabled", false],
    ],
  });

  // Now that the feature is enabled, set the remote settings data to force the
  // feature to sync so we can be sure syncing is done before starting the test.
  await QuickSuggestTestUtils.setRemoteSettingsResults(REMOTE_SETTINGS_DATA);
});

add_task(async function basic() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do a search.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "pocket suggestion",
    });

    // Check the result.
    Assert.equal(
      UrlbarTestUtils.getResultCount(window),
      2,
      "There should be two results"
    );

    let { element, result } = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      1
    );
    Assert.equal(
      result.providerName,
      UrlbarProviderQuickSuggest.name,
      "The result should be from the expected provider"
    );
    Assert.equal(
      result.payload.telemetryType,
      "pocket",
      "The result should be a Pocket result"
    );

    // Click it.
    const onLoad = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeMouseAtCenter(element.row, {});
    await onLoad;
    // Append utm parameters.
    let url = new URL(REMOTE_SETTINGS_DATA[0].attachment[0].url);
    url.searchParams.set("utm_medium", "firefox-desktop");
    url.searchParams.set("utm_source", "firefox-suggest");
    url.searchParams.set(
      "utm_campaign",
      "pocket-collections-in-the-address-bar"
    );
    url.searchParams.set("utm_content", "treatment");

    Assert.equal(gBrowser.currentURI.spec, url.href, "Expected page loaded");
  });
});

// Tests the "Show less frequently" command.
add_task(async function resultMenu_showLessFrequently() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.pocket.featureGate", true],
      ["browser.urlbar.pocket.showLessFrequentlyCount", 0],
    ],
  });

  const cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    pocketShowLessFrequentlyCap: 3,
  });

  // Sanity check.
  Assert.equal(UrlbarPrefs.get("pocketShowLessFrequentlyCap"), 3);
  Assert.equal(UrlbarPrefs.get("pocket.showLessFrequentlyCount"), 0);

  await doShowLessFrequently({
    input: "pocket s",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("pocket.showLessFrequentlyCount"), 1);

  await doShowLessFrequently({
    input: "pocket s",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("pocket.showLessFrequentlyCount"), 2);

  await doShowLessFrequently({
    input: "pocket s",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("pocket.showLessFrequentlyCount"), 3);

  await doShowLessFrequently({
    input: "pocket s",
    expected: {
      isSuggestionShown: false,
    },
  });

  await doShowLessFrequently({
    input: "pocket su",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: false,
    },
  });

  await cleanUpNimbus();
  await SpecialPowers.popPrefEnv();
});

async function doShowLessFrequently({ input, expected }) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: input,
  });

  if (!expected.isSuggestionShown) {
    for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
      const details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
      Assert.notEqual(
        details.result.payload.telemetryType,
        "pocket",
        `Pocket suggestion should be absent (checking index ${i})`
      );
    }

    return;
  }

  const resultIndex = 1;
  const details = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    resultIndex
  );
  Assert.equal(
    details.result.payload.telemetryType,
    "pocket",
    `Pocket suggestion should be present at expected index after ${input} search`
  );

  // Click the command.
  try {
    await UrlbarTestUtils.openResultMenuAndClickItem(
      window,
      "show_less_frequently",
      {
        resultIndex,
      }
    );
    Assert.ok(expected.isMenuItemShown);
    Assert.ok(
      gURLBar.view.isOpen,
      "The view should remain open clicking the command"
    );
    Assert.ok(
      details.element.row.hasAttribute("feedback-acknowledgment"),
      "Row should have feedback acknowledgment after clicking command"
    );
  } catch (e) {
    Assert.ok(!expected.isMenuItemShown);
    Assert.ok(
      !details.element.row.hasAttribute("feedback-acknowledgment"),
      "Row should not have feedback acknowledgment after clicking command"
    );
    Assert.equal(
      e.message,
      "Menu item not found for command: show_less_frequently"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);
}

// Tests the "Not interested" result menu dismissal command.
add_task(async function resultMenu_notInterested() {
  await doDismissTest("not_interested");

  // Re-enable suggestions and wait until PocketSuggestions syncs them from
  // remote settings again.
  UrlbarPrefs.set("suggest.pocket", true);
  let feature = QuickSuggest.getFeature("PocketSuggestions");
  await TestUtils.waitForCondition(async () => {
    let suggestions = await feature.queryRemoteSettings("pocket suggestion");
    return !!suggestions.length;
  }, "Waiting for PocketSuggestions to serve remote settings suggestions");
});

// Tests the "Not relevant" result menu dismissal command.
add_task(async function notRelevant() {
  await doDismissTest("not_relevant");
});

async function doDismissTest(command) {
  // Do a search.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "pocket suggestion",
  });

  // Check the result.
  let resultCount = 2;
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "There should be two results"
  );

  let resultIndex = 1;
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    resultIndex
  );
  Assert.equal(
    result.providerName,
    UrlbarProviderQuickSuggest.name,
    "The result should be from the expected provider"
  );
  Assert.equal(
    result.payload.telemetryType,
    "pocket",
    "The result should be a Pocket result"
  );

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(
    window,
    ["[data-l10n-id=firefox-suggest-command-dont-show-this]", command],
    { resultIndex, openByMouse: true }
  );

  // The row should be a tip now.
  Assert.ok(gURLBar.view.isOpen, "The view should remain open after dismissal");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "The result count should not haved changed after dismissal"
  );
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "Row should be a tip after dismissal"
  );
  Assert.equal(
    details.result.payload.type,
    "dismissalAcknowledgment",
    "Tip type should be dismissalAcknowledgment"
  );
  Assert.ok(
    !details.element.row.hasAttribute("feedback-acknowledgment"),
    "Row should not have feedback acknowledgment after dismissal"
  );

  // Get the dismissal acknowledgment's "Got it" button and click it.
  let gotItButton = UrlbarTestUtils.getButtonForResultIndex(
    window,
    0,
    resultIndex
  );
  Assert.ok(gotItButton, "Row should have a 'Got it' button");
  EventUtils.synthesizeMouseAtCenter(gotItButton, {}, window);

  // The view should remain open and the tip row should be gone.
  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open clicking the 'Got it' button"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount - 1,
    "The result count should be one less after clicking 'Got it' button"
  );
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      details.type != UrlbarUtils.RESULT_TYPE.TIP &&
        details.result.payload.telemetryType !== "pocket",
      "Tip result and suggestion should not be present"
    );
  }

  gURLBar.handleRevert();

  // Do the search again.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "pocket suggestion",
  });
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      details.type != UrlbarUtils.RESULT_TYPE.TIP &&
        details.result.payload.telemetryType !== "pocket",
      "Tip result and suggestion should not be present"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);
  await QuickSuggest.blockedSuggestions.clear();
}

// Tests row labels.
add_task(async function rowLabel() {
  const testCases = [
    // high confidence keyword best match
    {
      searchString: "high",
      expected: "Recommended reads",
    },
    // low confidence keyword non-best match
    {
      searchString: "pocket suggestion",
      expected: "Firefox Suggest",
    },
  ];

  for (const { searchString, expected } of testCases) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    const { element } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    const row = element.row;
    Assert.equal(row.getAttribute("label"), expected);
  }
});

// Tests visibility of "Show less frequently" menu.
add_task(async function showLessFrequentlyMenuVisibility() {
  const testCases = [
    // high confidence keyword best match
    {
      searchString: "high",
      expected: false,
    },
    // low confidence keyword non-best match
    {
      searchString: "pocket suggestion",
      expected: true,
    },
  ];

  for (const { searchString, expected } of testCases) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    const resultIndex = 1;
    const details = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      resultIndex
    );
    Assert.equal(
      details.result.payload.telemetryType,
      "pocket",
      "Pocket suggestion should be present at expected index"
    );

    const menuitem = await UrlbarTestUtils.openResultMenuAndGetItem({
      resultIndex,
      openByMouse: false,
      command: "show_less_frequently",
      window,
    });
    Assert.equal(!!menuitem, expected);

    gURLBar.view.resultMenu.hidePopup(true);
  }

  await UrlbarTestUtils.promisePopupClose(window);
});
