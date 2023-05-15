/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for addon suggestions.

const TEST_MERINO_SUGGESTIONS = [
  {
    provider: "amo",
    icon: "https://example.com/first.svg",
    url: "https://example.com/first-addon",
    title: "First Addon",
    description: "This is a first addon",
    custom_details: {
      amo: {
        rating: "5",
        number_of_ratings: "1234567",
      },
    },
    is_top_pick: true,
  },
  {
    provider: "amo",
    icon: "https://example.com/second.png",
    url: "https://example.com/second-addon",
    title: "Second Addon",
    description: "This is a second addon",
    custom_details: {
      amo: {
        rating: "4.5",
        number_of_ratings: "123",
      },
    },
    is_sponsored: true,
  },
  {
    provider: "amo",
    icon: "https://example.com/third.svg",
    url: "https://example.com/third-addon",
    title: "Third Addon",
    description: "This is a third addon",
    custom_details: {
      amo: {
        rating: "0",
        number_of_ratings: "0",
      },
    },
  },
];

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.quicksuggest.remoteSettings.enabled", false],
      ["browser.urlbar.bestMatch.enabled", true],
    ],
  });

  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    merinoSuggestions: TEST_MERINO_SUGGESTIONS,
  });
});

add_task(async function basic() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.addons.featureGate", true]],
  });

  for (const merinoSuggestion of TEST_MERINO_SUGGESTIONS) {
    MerinoTestUtils.server.response.body.suggestions = [merinoSuggestion];

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "only match the Merino suggestion",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    const { element, result } = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      1
    );
    const row = element.row;
    const icon = row.querySelector(".urlbarView-dynamic-addons-icon");
    Assert.equal(icon.src, merinoSuggestion.icon);
    const url = row.querySelector(".urlbarView-dynamic-addons-url");
    Assert.equal(url.textContent, merinoSuggestion.url);
    const title = row.querySelector(".urlbarView-dynamic-addons-title");
    Assert.equal(title.textContent, merinoSuggestion.title);
    const description = row.querySelector(
      ".urlbarView-dynamic-addons-description"
    );
    Assert.equal(description.textContent, merinoSuggestion.description);
    const reviews = row.querySelector(".urlbarView-dynamic-addons-reviews");
    Assert.equal(
      reviews.textContent,
      `${new Intl.NumberFormat().format(
        Number(merinoSuggestion.custom_details.amo.number_of_ratings)
      )} reviews`
    );

    if (merinoSuggestion.is_top_pick) {
      Assert.equal(result.suggestedIndex, 1);
    } else if (merinoSuggestion.is_sponsored) {
      Assert.equal(
        result.suggestedIndex,
        UrlbarPrefs.get("quickSuggestSponsoredIndex")
      );
    } else {
      Assert.equal(
        result.suggestedIndex,
        UrlbarPrefs.get("quickSuggestNonSponsoredIndex")
      );
    }

    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      merinoSuggestion.url
    );
    EventUtils.synthesizeMouseAtCenter(row, {});
    await onLoad;
    Assert.ok(true, "Expected page is loaded");

    await PlacesUtils.history.clear();
  }

  await SpecialPowers.popPrefEnv();
});

add_task(async function ratings() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.addons.featureGate", true]],
  });

  const testRating = [
    "0",
    "0.24",
    "0.25",
    "0.74",
    "0.75",
    "1",
    "1.24",
    "1.25",
    "1.74",
    "1.75",
    "2",
    "2.24",
    "2.25",
    "2.74",
    "2.75",
    "3",
    "3.24",
    "3.25",
    "3.74",
    "3.75",
    "4",
    "4.24",
    "4.25",
    "4.74",
    "4.75",
    "5",
  ];
  const baseMerinoSuggestion = JSON.parse(
    JSON.stringify(TEST_MERINO_SUGGESTIONS[0])
  );

  for (const rating of testRating) {
    baseMerinoSuggestion.custom_details.amo.rating = rating;
    MerinoTestUtils.server.response.body.suggestions = [baseMerinoSuggestion];

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "only match the Merino suggestion",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    const { element } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);

    const ratingElements = element.row.querySelectorAll(
      ".urlbarView-dynamic-addons-rating"
    );
    Assert.equal(ratingElements.length, 5);

    for (let i = 0; i < ratingElements.length; i++) {
      const ratingElement = ratingElements[i];

      const distanceToFull = Number(rating) - i;
      let fill = "full";
      if (distanceToFull < 0.25) {
        fill = "empty";
      } else if (distanceToFull < 0.75) {
        fill = "half";
      }
      Assert.equal(ratingElement.getAttribute("fill"), fill);
    }
  }

  await SpecialPowers.popPrefEnv();
});

add_task(async function disable() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.addons.featureGate", false]],
  });

  // Restore AdmWikipedia suggestions.
  MerinoTestUtils.server.reset();
  // Add one Addon suggestion that is higher score than AdmWikipedia.
  MerinoTestUtils.server.response.body.suggestions.push(
    Object.assign({}, TEST_MERINO_SUGGESTIONS[0], { score: 2 })
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "only match the Merino suggestion",
  });
  Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

  const { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.payload.telemetryType, "adm_sponsored");

  await SpecialPowers.popPrefEnv();
});

add_task(async function resultMenu_showLessFrequently() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.addons.featureGate", true],
      ["browser.urlbar.addons.minKeywordLength", 0],
    ],
  });
  MerinoTestUtils.server.response.body.suggestions = TEST_MERINO_SUGGESTIONS;
  const cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    addonsKeywordsMinimumLengthCap: 3,
  });

  // Sanity check.
  Assert.equal(UrlbarPrefs.get("addonsKeywordsMinimumLengthCap"), 3);
  Assert.equal(UrlbarPrefs.get("addonsKeywordsMinimumLength"), 0);
  Assert.equal(UrlbarPrefs.get("addons.minKeywordLength"), 0);

  await doShowLessFrequently({
    input: "12",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("addons.minKeywordLength"), 1);

  await doShowLessFrequently({
    input: "12",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("addons.minKeywordLength"), 2);

  await doShowLessFrequently({
    input: "12",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("addons.minKeywordLength"), 3);

  await doShowLessFrequently({
    input: "12",
    expected: {
      // The suggestion should not display since addons.minKeywordLength is 3
      // and the text length is 2,
      isSuggestionShown: false,
    },
  });

  await doShowLessFrequently({
    input: "123",
    expected: {
      // The suggestion should display, but item should not shown since the
      // addons.minKeywordLength reached to addonsKeywordsMinimumLengthCap
      // already.
      isSuggestionShown: true,
      isMenuItemShown: false,
    },
  });

  await cleanUpNimbus;
  await SpecialPowers.popPrefEnv();
});

add_task(async function resultMenu_showLessFrequentlyWithNimbusMinimumLength() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.addons.featureGate", true],
      ["browser.urlbar.addons.minKeywordLength", 0],
    ],
  });
  MerinoTestUtils.server.response.body.suggestions = TEST_MERINO_SUGGESTIONS;
  const cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    addonsKeywordsMinimumLengthCap: 3,
    addonsKeywordsMinimumLength: 2,
  });

  // Sanity check.
  Assert.equal(UrlbarPrefs.get("addonsKeywordsMinimumLengthCap"), 3);
  Assert.equal(UrlbarPrefs.get("addonsKeywordsMinimumLength"), 2);
  Assert.equal(UrlbarPrefs.get("addons.minKeywordLength"), 0);

  await doShowLessFrequently({
    input: "12",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("addons.minKeywordLength"), 3);

  await doShowLessFrequently({
    input: "12",
    expected: {
      // The suggestion should not display since addons.minKeywordLength is 3
      // and the text length is 2,
      isSuggestionShown: false,
    },
  });

  await doShowLessFrequently({
    input: "123",
    expected: {
      // The suggestion should display, but item should not shown since the
      // addons.minKeywordLength reached to addonsKeywordsMinimumLengthCap
      // already.
      isSuggestionShown: true,
      isMenuItemShown: false,
    },
  });

  await cleanUpNimbus;
  await SpecialPowers.popPrefEnv();
});

// Tests the "Not interested" result menu dismissal command.
add_task(async function resultMenu_notInterested() {
  await doDismissTest("not_interested");
});

// Tests the "Not relevant" result menu dismissal command.
add_task(async function notRelevant() {
  await doDismissTest("not_relevant");
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
        details.result.payload.dynamicType,
        "addons",
        `Addons suggestion should be absent (checking index ${i})`
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
    details.result.payload.dynamicType,
    "addons",
    `Addons suggestion should be present at expected index after ${input} search`
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

async function doDismissTest(command) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.addons.featureGate", true]],
  });

  const cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    addonsKeywordsMinimumLengthCap: 0,
    addonsKeywordsMinimumLength: 0,
  });

  // Sanity check.
  Assert.equal(UrlbarPrefs.get("addonsKeywordsMinimumLengthCap"), 0);
  Assert.equal(UrlbarPrefs.get("addonsKeywordsMinimumLength"), 0);
  Assert.equal(UrlbarPrefs.get("addons.minKeywordLength"), 0);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "123",
  });

  const resultCount = UrlbarTestUtils.getResultCount(window);
  const resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.result.payload.dynamicType,
    "addons",
    "Addons suggestion should be present"
  );

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(
    window,
    ["[data-l10n-id=firefox-suggest-command-dont-show-this]", command],
    { resultIndex, openByMouse: true }
  );

  Assert.ok(
    !UrlbarPrefs.get("addons.featureGate"),
    "addons.featureGate pref should be set to false after dismissal"
  );

  // The row should be a tip now.
  Assert.ok(gURLBar.view.isOpen, "The view should remain open after dismissal");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "The result count should not haved changed after dismissal"
  );
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
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
    "0",
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
        details.result.payload.dynamicType !== "addons",
      "Tip result and addon result should not be present"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);

  await cleanUpNimbus;
  await SpecialPowers.popPrefEnv();
  UrlbarPrefs.clear("addons.featureGate");
}
