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
    },
  },
];

add_setup(async function () {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
    prefs: [
      ["quicksuggest.rustEnabled", true],
      ["suggest.quicksuggest.nonsponsored", true],
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
    "https://www.yelp.com/search?find_desc=RaMeN&find_loc=tOkYo"
  );
  Assert.equal(result.payload.title, "RaMeN iN tOkYo");
});
