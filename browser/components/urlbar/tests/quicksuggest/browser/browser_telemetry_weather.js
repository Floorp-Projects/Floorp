/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for weather suggestions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

const suggestion_type = "weather";
const match_type = "firefox-suggest";
const index = 1;
const position = index + 1;

const { TELEMETRY_SCALARS: WEATHER_SCALARS } = UrlbarProviderWeather;
const { WEATHER_SUGGESTION: suggestion, WEATHER_RS_DATA } = MerinoTestUtils;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Make sure quick actions are disabled because showing them in the top
      // sites view interferes with this test.
      ["browser.urlbar.suggest.quickactions", false],
    ],
  });

  await setUpTelemetryTest({
    remoteSettingsRecords: [
      {
        type: "weather",
        weather: WEATHER_RS_DATA,
      },
    ],
  });
  await MerinoTestUtils.initWeather();
  await updateTopSitesAndAwaitChanged();
});

add_task(async function () {
  await doTelemetryTest({
    index,
    suggestion,
    providerName: UrlbarProviderWeather.name,
    showSuggestion: async () => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: MerinoTestUtils.WEATHER_KEYWORD,
      });
    },
    teardown: async () => {
      // Picking the block button sets this pref to false and disables weather
      // suggestions. We need to flip it back to true and wait for the
      // suggestion to be fetched again before continuing to the next selectable
      // test. The view also also stay open, so close it afterward.
      if (!UrlbarPrefs.get("suggest.weather")) {
        await UrlbarTestUtils.promisePopupClose(window);
        gURLBar.handleRevert();
        let fetchPromise = QuickSuggest.weather.waitForFetches();
        UrlbarPrefs.clear("suggest.weather");
        await fetchPromise;

        // Wait for keywords to be re-synced from remote settings.
        await QuickSuggestTestUtils.forceSync();
      }
    },
    // impression-only
    impressionOnly: {
      scalars: {
        [WEATHER_SCALARS.IMPRESSION]: position,
      },
      event: {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "impression_only",
        extra: {
          suggestion_type,
          match_type,
          position: position.toString(),
        },
      },
    },
    // click
    click: {
      scalars: {
        [WEATHER_SCALARS.IMPRESSION]: position,
        [WEATHER_SCALARS.CLICK]: position,
      },
      event: {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "click",
        extra: {
          suggestion_type,
          match_type,
          position: position.toString(),
        },
      },
    },
    commands: [
      // not relevant
      {
        command: [
          "[data-l10n-id=firefox-suggest-command-dont-show-this]",
          "not_relevant",
        ],
        scalars: {
          [WEATHER_SCALARS.IMPRESSION]: position,
        },
        event: {
          category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
          method: "engagement",
          object: "other",
          extra: {
            suggestion_type,
            match_type,
            position: position.toString(),
          },
        },
      },
      // help
      {
        command: "help",
        scalars: {
          [WEATHER_SCALARS.IMPRESSION]: position,
          [WEATHER_SCALARS.HELP]: position,
        },
        event: {
          category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
          method: "engagement",
          object: "help",
          extra: {
            suggestion_type,
            match_type,
            position: position.toString(),
          },
        },
      },
    ],
  });
});

async function updateTopSitesAndAwaitChanged() {
  let url = "http://mochi.test:8888/topsite";
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(url);
  }

  info("Updating top sites and awaiting newtab-top-sites-changed");
  let changedPromise = TestUtils.topicObserved("newtab-top-sites-changed").then(
    () => info("Observed newtab-top-sites-changed")
  );
  await updateTopSites(sites => sites?.length);
  await changedPromise;
}
