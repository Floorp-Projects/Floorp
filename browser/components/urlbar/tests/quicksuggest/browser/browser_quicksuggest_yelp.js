/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for Yelp suggestions.

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "yelp-suggestions",
    attachment: {
      subjects: ["ramen"],
      preModifiers: ["best"],
      postModifiers: ["delivery"],
      locationSigns: [{ keyword: "in", needLocation: true }],
      yelpModifiers: [],
    },
  },
];

add_setup(async function () {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
    prefs: [
      ["quicksuggest.rustEnabled", true],
      ["suggest.quicksuggest.sponsored", true],
      ["suggest.yelp", true],
      ["yelp.featureGate", true],
    ],
  });
});

add_task(async function basic() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "RaMeN iN tOkYo",
  });

  Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

  const { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    result.providerName,
    UrlbarProviderQuickSuggest.name,
    "The result should be from the expected provider"
  );
  Assert.equal(result.payload.provider, "Yelp");
  Assert.equal(
    result.payload.url,
    "https://www.yelp.com/search?find_desc=RaMeN&find_loc=tOkYo&utm_medium=partner&utm_source=mozilla"
  );
  Assert.equal(result.payload.title, "RaMeN iN tOkYo");
});

// Tests the "Show less frequently" result menu command.
add_task(async function resultMenu_show_less_frequently() {
  await doShowLessFrequently({
    minKeywordLength: 0,
    frequentlyCap: 0,
    initialFrequentlyCount: 2,
    testData: [
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "ramen",
        expected: {
          hasSuggestion: false,
        },
      },
    ],
  });

  await doShowLessFrequently({
    minKeywordLength: 0,
    frequentlyCap: 4,
    initialFrequentlyCount: 2,
    testData: [
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: false,
        },
      },
    ],
  });

  await doShowLessFrequently({
    minKeywordLength: 1,
    frequentlyCap: 4,
    initialFrequentlyCount: 2,
    testData: [
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "ramen",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: false,
        },
      },
    ],
  });
});

async function doShowLessFrequently({
  minKeywordLength,
  frequentlyCap,
  initialFrequentlyCount,
  testData,
}) {
  let cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    yelpMinKeywordLength: minKeywordLength,
    yelpShowLessFrequentlyCap: frequentlyCap,
  });
  UrlbarPrefs.set("yelp.showLessFrequentlyCount", initialFrequentlyCount);

  for (let { input, expected } of testData) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: input,
    });

    if (expected.hasSuggestion) {
      let resultIndex = 1;
      let details = await UrlbarTestUtils.getDetailsOfResultAt(
        window,
        resultIndex
      );
      Assert.equal(details.result.payload.provider, "Yelp");

      if (expected.hasShowLessItem) {
        // Click the command.
        let previousShowLessFrequentlyCount = UrlbarPrefs.get(
          "yelp.showLessFrequentlyCount"
        );
        await UrlbarTestUtils.openResultMenuAndClickItem(
          window,
          "show_less_frequentry",
          { resultIndex, openByMouse: true }
        );

        Assert.equal(
          UrlbarPrefs.get("yelp.showLessFrequentlyCount"),
          previousShowLessFrequentlyCount + 1
        );
      } else {
        let menuitem = await UrlbarTestUtils.openResultMenuAndGetItem({
          window,
          command: "show_less_frequently",
          resultIndex: 1,
          openByMouse: true,
        });
        Assert.ok(!menuitem);
      }
    } else {
      // Yelp suggestion should not be shown.
      for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
        let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
        Assert.notEqual(details.result.payload.provider, "Yelp");
      }
    }

    await UrlbarTestUtils.promisePopupClose(window);
  }

  await cleanUpNimbus();
  UrlbarPrefs.set("yelp.showLessFrequentlyCount", 0);
}

// Tests the "Don't show this again" result menu dismissal command.
add_task(async function resultMenu_not_interested() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ramen",
  });

  let resultCount = UrlbarTestUtils.getResultCount(window);
  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(details.result.payload.provider, "Yelp");

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(window, "not_interested", {
    resultIndex,
    openByMouse: true,
  });

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
        details.result.payload.provider !== "Yelp",
      "Tip result and Yelp result should not be present"
    );
  }

  Assert.ok(!UrlbarPrefs.get("suggest.yelp"));

  await UrlbarTestUtils.promisePopupClose(window);
  UrlbarPrefs.clear("suggest.yelp");
});

// Tests the row/group label.
add_task(async function rowLabel() {
  let tests = [
    { topPick: true, label: "Local recommendations" },
    { topPick: false, label: "Firefox Suggest" },
  ];

  for (let { topPick, label } of tests) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.quicksuggest.yelpPriority", topPick]],
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "ramen",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    const { element } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    const row = element.row;
    Assert.equal(row.getAttribute("label"), label);

    await UrlbarTestUtils.promisePopupClose(window);
    await SpecialPowers.popPrefEnv();
  }
});
