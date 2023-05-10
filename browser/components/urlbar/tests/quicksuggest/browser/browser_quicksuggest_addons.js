/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for addon suggestions.

const TEST_MERINO_SUGGESTIONS = [
  {
    provider: "amo",
    icon: "https://example.com/first.svg",
    url: "https://example.com/first-addon",
    name: "First Addon",
    description: "This is a first addon",
    custom_details: {
      addons: {
        rating: "5",
        reviews: "1234567",
      },
    },
    is_top_pick: true,
  },
  {
    provider: "amo",
    icon: "https://example.com/second.png",
    url: "https://example.com/second-addon",
    name: "Second Addon",
    description: "This is a second addon",
    custom_details: {
      addons: {
        rating: "4.5",
        reviews: "123",
      },
    },
    is_sponsored: true,
  },
  {
    provider: "amo",
    icon: "https://example.com/third.svg",
    url: "https://example.com/third-addon",
    name: "Third Addon",
    description: "This is a third addon",
    custom_details: {
      addons: {
        rating: "0",
        reviews: "0",
      },
    },
  },
];

add_setup(async function() {
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
    Assert.equal(title.textContent, merinoSuggestion.name);
    const description = row.querySelector(
      ".urlbarView-dynamic-addons-description"
    );
    Assert.equal(description.textContent, merinoSuggestion.description);
    const reviews = row.querySelector(".urlbarView-dynamic-addons-reviews");
    Assert.equal(
      reviews.textContent,
      `${new Intl.NumberFormat().format(
        Number(merinoSuggestion.custom_details.addons.reviews)
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
    baseMerinoSuggestion.custom_details.addons.rating = rating;
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
