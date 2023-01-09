/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for navigational suggestions, a.k.a.
 * navigational top picks.
 */

"use strict";

const { TELEMETRY_SCALARS } = UrlbarProviderQuickSuggest;

const suggestion = {
  id: 1,
  url: "https://example.com/navigational-suggestion",
  title: "Navigational suggestion",
  keywords: ["nav"],
  iab_category: "5 - Education",
  is_top_pick: true,
};

const suggestion_type = "nonsponsored";
const index = 1;
const position = index + 1;

add_setup(async function() {
  // When `bestMatch.enabled` is true, any suggestion whose keyword is as long
  // as the threshold defined in the quick suggest config will automatically
  // become a best match. For navigational suggestions, best matches should be
  // totally determined by the presence of `is_top_pick` in the suggestion, so
  // delete the `best_match` part of the config.
  let config = QuickSuggestTestUtils.DEFAULT_CONFIG;
  delete config.best_match;

  await setUpTelemetryTest({
    config,
    suggestions: [suggestion],
  });
});

// non-best match (`bestMatch.enabled` = false)
add_task(async function navigational() {
  let match_type = "firefox-suggest";
  await doTelemetryTest({
    index,
    suggestion,
    // impression-only
    impressionOnly: {
      scalars: {
        [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
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
      ping: null,
    },
    selectables: {
      // click
      "urlbarView-row-inner": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.CLICK_NONSPONSORED]: position,
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
        pings: [],
      },
      // block
      "urlbarView-button-block": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.BLOCK_NONSPONSORED]: position,
        },
        event: {
          category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
          method: "engagement",
          object: "block",
          extra: {
            suggestion_type,
            match_type,
            position: position.toString(),
          },
        },
        pings: [],
      },
      // help
      "urlbarView-button-help": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.HELP_NONSPONSORED]: position,
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
        pings: [],
      },
    },
  });
});

// best match (`bestMatch.enabled` = true)
add_task(async function navigationalBestMatch() {
  let match_type = "best-match";
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", true]],
  });
  await doTelemetryTest({
    index,
    suggestion,
    // impression-only
    impressionOnly: {
      scalars: {
        [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
        [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED_BEST_MATCH]: position,
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
      ping: null,
    },
    selectables: {
      // click
      "urlbarView-row-inner": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED_BEST_MATCH]: position,
          [TELEMETRY_SCALARS.CLICK_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.CLICK_NONSPONSORED_BEST_MATCH]: position,
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
        pings: [],
      },
      // block
      "urlbarView-button-block": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED_BEST_MATCH]: position,
          [TELEMETRY_SCALARS.BLOCK_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.BLOCK_NONSPONSORED_BEST_MATCH]: position,
        },
        event: {
          category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
          method: "engagement",
          object: "block",
          extra: {
            suggestion_type,
            match_type,
            position: position.toString(),
          },
        },
        pings: [],
      },
      // help
      "urlbarView-button-help": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED_BEST_MATCH]: position,
          [TELEMETRY_SCALARS.HELP_NONSPONSORED]: position,
          [TELEMETRY_SCALARS.HELP_NONSPONSORED_BEST_MATCH]: position,
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
        pings: [],
      },
    },
  });
  await SpecialPowers.popPrefEnv();
});
