/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests browser quick suggestions.
 */

const TEST_URL = "http://example.com/quicksuggest";

const REMOTE_SETTINGS_RESULTS = [
  {
    id: 1,
    url: `${TEST_URL}?q=frabbits`,
    title: "frabbits",
    keywords: ["fra", "frab"],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
    iab_category: "22 - Shopping",
    icon: "1234",
  },
  {
    id: 2,
    url: `${TEST_URL}?q=nonsponsored`,
    title: "Non-Sponsored",
    keywords: ["nonspon"],
    click_url: "http://click.reporting.test.com/nonsponsored",
    impression_url: "http://impression.reporting.test.com/nonsponsored",
    advertiser: "Wikipedia",
    iab_category: "5 - Education",
    icon: "1234",
  },
];

const MERINO_NAVIGATIONAL_SUGGESTION = {
  url: "https://example.com/navigational-suggestion",
  title: "Navigational suggestion",
  provider: "top_picks",
  is_sponsored: false,
  score: 0.25,
  block_id: 0,
  is_top_pick: true,
};

const MERINO_DYNAMIC_WIKIPEDIA_SUGGESTION = {
  url: "https://example.com/dynamic-wikipedia",
  title: "Dynamic Wikipedia suggestion",
  click_url: "https://example.com/click",
  impression_url: "https://example.com/impression",
  advertiser: "dynamic-wikipedia",
  provider: "wikipedia",
  iab_category: "5 - Education",
  block_id: 1,
};

add_setup(async function () {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: REMOTE_SETTINGS_RESULTS,
      },
    ],
    merinoSuggestions: [],
  });

  // Disable Merino so we trigger only remote settings suggestions.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
});

// Tests a sponsored result and keyword highlighting.
add_tasks_with_rust(async function sponsored() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index: 1,
    isSponsored: true,
    url: `${TEST_URL}?q=frabbits`,
  });
  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);
  Assert.equal(
    row.querySelector(".urlbarView-title").firstChild.textContent,
    "fra",
    "The part of the keyword that matches users input is not bold."
  );
  Assert.equal(
    row.querySelector(".urlbarView-title > strong").textContent,
    "b",
    "The auto completed section of the keyword is bolded."
  );
  await UrlbarTestUtils.promisePopupClose(window);
});

// Tests a non-sponsored result.
add_tasks_with_rust(async function nonSponsored() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "nonspon",
  });
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index: 1,
    isSponsored: false,
    url: `${TEST_URL}?q=nonsponsored`,
  });
  await UrlbarTestUtils.promisePopupClose(window);
});

// Tests sponsored priority feature.
add_tasks_with_rust(async function sponsoredPriority() {
  const cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    quickSuggestSponsoredPriority: true,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index: 1,
    isSponsored: true,
    isBestMatch: true,
    url: `${TEST_URL}?q=frabbits`,
  });

  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);
  Assert.equal(
    row.querySelector(".urlbarView-title").firstChild.textContent,
    "fra",
    "The part of the keyword that matches users input is not bold."
  );
  Assert.equal(
    row.querySelector(".urlbarView-title > strong").textContent,
    "b",
    "The auto completed section of the keyword is bolded."
  );

  // Group label.
  let before = window.getComputedStyle(row, "::before");
  Assert.equal(before.content, "attr(label)", "::before.content is enabled");
  Assert.equal(
    row.getAttribute("label"),
    "Top pick",
    "Row has 'Top pick' group label"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  await cleanUpNimbus();
});

// Tests sponsored priority feature does not affect to non-sponsored suggestion.
add_tasks_with_rust(
  async function sponsoredPriorityButNotSponsoredSuggestion() {
    const cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
      quickSuggestSponsoredPriority: true,
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "nonspon",
    });
    await QuickSuggestTestUtils.assertIsQuickSuggest({
      window,
      index: 1,
      isSponsored: false,
      url: `${TEST_URL}?q=nonsponsored`,
    });

    let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);
    let before = window.getComputedStyle(row, "::before");
    Assert.equal(before.content, "attr(label)", "::before.content is enabled");
    Assert.equal(
      row.getAttribute("label"),
      "Firefox Suggest",
      "Row has general group label for quick suggest"
    );

    await UrlbarTestUtils.promisePopupClose(window);
    await cleanUpNimbus();
  }
);

// Tests the "Manage" result menu for sponsored suggestion.
add_tasks_with_rust(async function resultMenu_manage_sponsored() {
  await doManageTest({
    input: "fra",
    index: 1,
  });
});

// Tests the "Manage" result menu for non-sponsored suggestion.
add_tasks_with_rust(async function resultMenu_manage_nonSponsored() {
  await doManageTest({
    input: "nonspon",
    index: 1,
  });
});

// Tests the "Manage" result menu for Navigational suggestion.
add_tasks_with_rust(async function resultMenu_manage_navigational() {
  // Enable Merino.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  MerinoTestUtils.server.response.body.suggestions = [
    MERINO_NAVIGATIONAL_SUGGESTION,
  ];

  await doManageTest({
    input: "test",
    index: 1,
  });

  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
});

// Tests the "Manage" result menu for Dynamic Wikipedia suggestion.
add_tasks_with_rust(async function resultMenu_manage_dynamicWikipedia() {
  // Enable Merino.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  MerinoTestUtils.server.response.body.suggestions = [
    MERINO_DYNAMIC_WIKIPEDIA_SUGGESTION,
  ];

  await doManageTest({
    input: "test",
    index: 1,
  });

  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
});
