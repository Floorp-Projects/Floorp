/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for addon suggestions.

// The expected index of the addon suggestion.
const EXPECTED_RESULT_INDEX = 1;

// Allow more time for TV runs.
requestLongerTimeout(5);

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
        guid: "first@addon",
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
        guid: "second@addon",
      },
    },
    is_sponsored: true,
    is_top_pick: false,
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
        guid: "third@addon",
      },
    },
    is_top_pick: false,
  },
  {
    provider: "amo",
    icon: "https://example.com/fourth.svg",
    url: "https://example.com/fourth-addon",
    title: "Fourth Addon",
    description: "This is a fourth addon",
    custom_details: {
      amo: {
        rating: "4",
        number_of_ratings: "4",
        guid: "fourth@addon",
      },
    },
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.quicksuggest.remoteSettings.enabled", false],
    ],
  });

  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    merinoSuggestions: TEST_MERINO_SUGGESTIONS,
  });
});

add_task(async function basic() {
  // Treatment B (no stars) should be the default.
  Assert.equal(UrlbarPrefs.get("addonsUITreatment"), "b");

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
    const expectedUrl = makeExpectedUrl(merinoSuggestion.url);
    Assert.equal(url.textContent, expectedUrl);
    const title = row.querySelector(".urlbarView-dynamic-addons-title");
    Assert.equal(title.textContent, merinoSuggestion.title);
    const description = row.querySelector(
      ".urlbarView-dynamic-addons-description"
    );
    Assert.equal(description.textContent, merinoSuggestion.description);
    const ratingContainer = row.querySelector(
      ".urlbarView-dynamic-addons-ratingContainer"
    );
    Assert.ok(BrowserTestUtils.is_hidden(ratingContainer));
    const reviews = row.querySelector(".urlbarView-dynamic-addons-reviews");
    Assert.equal(reviews.textContent, "Recommended");

    const isTopPick = merinoSuggestion.is_top_pick ?? true;
    if (isTopPick) {
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
      expectedUrl
    );
    EventUtils.synthesizeMouseAtCenter(row, {});
    await onLoad;
    Assert.ok(true, "Expected page is loaded");

    await PlacesUtils.history.clear();
  }
});

add_task(async function ratings() {
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

  const cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    addonsUITreatment: "a",
  });

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

  await cleanUpNimbus();
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

  MerinoTestUtils.server.response.body.suggestions = TEST_MERINO_SUGGESTIONS;
  await SpecialPowers.popPrefEnv();
});

add_task(async function resultMenu_showLessFrequently() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.addons.showLessFrequentlyCount", 0]],
  });

  await QuickSuggestTestUtils.setConfig({
    show_less_frequently_cap: 3,
  });

  await doShowLessFrequently({
    input: "aaa b",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("addons.showLessFrequentlyCount"), 1);

  await doShowLessFrequently({
    input: "aaa b",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("addons.showLessFrequentlyCount"), 2);

  // The cap will be reached this time. Keep the view open so we can make sure
  // the command has been removed from the menu before it closes.
  await doShowLessFrequently({
    keepViewOpen: true,
    input: "aaa b",
    expected: {
      isSuggestionShown: true,
      isMenuItemShown: true,
    },
  });
  Assert.equal(UrlbarPrefs.get("addons.showLessFrequentlyCount"), 3);

  // Make sure the command has been removed.
  let menuitem = await UrlbarTestUtils.openResultMenuAndGetItem({
    window,
    command: "show_less_frequently",
    resultIndex: EXPECTED_RESULT_INDEX,
    openByMouse: true,
  });
  Assert.ok(!menuitem, "Menuitem should be absent before closing the view");
  gURLBar.view.resultMenu.hidePopup(true);
  await UrlbarTestUtils.promisePopupClose(window);

  await doShowLessFrequently({
    input: "aaa b",
    expected: {
      // The suggestion should not display since addons.showLessFrequentlyCount
      // is 3 and the substring (" b") after the first word ("aaa") is 2 chars
      // long.
      isSuggestionShown: false,
    },
  });

  await doShowLessFrequently({
    input: "aaa bb",
    expected: {
      // The suggestion should display, but item should not shown since the
      // addons.showLessFrequentlyCount reached to addonsShowLessFrequentlyCap
      // already.
      isSuggestionShown: true,
      isMenuItemShown: false,
    },
  });

  await QuickSuggestTestUtils.setConfig(QuickSuggestTestUtils.DEFAULT_CONFIG);
  await SpecialPowers.popPrefEnv();
});

// Tests the "Not interested" result menu dismissal command.
add_task(async function resultMenu_notInterested() {
  await doDismissTest("not_interested", true);
});

// Tests the "Not relevant" result menu dismissal command.
add_task(async function notRelevant() {
  await doDismissTest("not_relevant", false);
});

add_task(async function rowLabel() {
  // Addon suggestions should be shown as a best match regardless of
  // `browser.urlbar.bestMatch.enabled`, as long as `suggestion.is_top_pick` is
  // either true or undefined.
  const testCases = [
    {
      bestMatch: true,
      expected: "Firefox extension",
    },
    {
      bestMatch: false,
      expected: "Firefox extension",
    },
  ];

  for (const { bestMatch, expected } of testCases) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.bestMatch.enabled", bestMatch]],
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "only match the Merino suggestion",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    const { element } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    const row = element.row;
    Assert.equal(row.getAttribute("label"), expected);

    await SpecialPowers.popPrefEnv();
  }
});

async function doShowLessFrequently({ input, expected, keepViewOpen = false }) {
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

  const details = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    EXPECTED_RESULT_INDEX
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
        resultIndex: EXPECTED_RESULT_INDEX,
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

  if (!keepViewOpen) {
    await UrlbarTestUtils.promisePopupClose(window);
  }
}

async function doDismissTest(command, allDismissed) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "123",
  });

  const resultCount = UrlbarTestUtils.getResultCount(window);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    EXPECTED_RESULT_INDEX
  );
  Assert.equal(
    details.result.payload.dynamicType,
    "addons",
    "Addons suggestion should be present"
  );

  // Sanity check.
  Assert.ok(UrlbarPrefs.get("suggest.addons"));

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(
    window,
    ["[data-l10n-id=firefox-suggest-command-dont-show-this]", command],
    { resultIndex: EXPECTED_RESULT_INDEX, openByMouse: true }
  );

  Assert.equal(
    UrlbarPrefs.get("suggest.addons"),
    !allDismissed,
    "suggest.addons should be true iff all suggestions weren't dismissed"
  );
  Assert.equal(
    await QuickSuggest.blockedSuggestions.has(
      details.result.payload.originalUrl
    ),
    !allDismissed,
    "Suggestion URL should be blocked iff all suggestions weren't dismissed"
  );

  // The row should be a tip now.
  Assert.ok(gURLBar.view.isOpen, "The view should remain open after dismissal");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "The result count should not haved changed after dismissal"
  );
  details = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    EXPECTED_RESULT_INDEX
  );
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

  // Check tip title.
  let title = details.element.row.querySelector(".urlbarView-title");
  let titleL10nId = title.dataset.l10nId;
  if (allDismissed) {
    Assert.equal(titleL10nId, "firefox-suggest-dismissal-acknowledgment-all");
  } else {
    Assert.equal(titleL10nId, "firefox-suggest-dismissal-acknowledgment-one");
  }

  // Get the dismissal acknowledgment's "Got it" button and click it.
  let gotItButton = UrlbarTestUtils.getButtonForResultIndex(
    window,
    0,
    EXPECTED_RESULT_INDEX
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

  UrlbarPrefs.clear("suggest.addons");
  await QuickSuggest.blockedSuggestions.clear();
}

function makeExpectedUrl(originalUrl) {
  let url = new URL(originalUrl);
  url.searchParams.set("utm_medium", "firefox-desktop");
  url.searchParams.set("utm_source", "firefox-suggest");
  return url.href;
}
