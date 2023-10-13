/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for dynamic Wikipedia suggestions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.sys.mjs",
});

const { TELEMETRY_SCALARS } = UrlbarProviderQuickSuggest;

const MERINO_SUGGESTION = {
  block_id: 1,
  url: "https://example.com/dynamic-wikipedia",
  title: "Dynamic Wikipedia suggestion",
  click_url: "https://example.com/click",
  impression_url: "https://example.com/impression",
  advertiser: "dynamic-wikipedia",
  provider: "wikipedia",
  iab_category: "5 - Education",
};

const suggestion_type = "dynamic-wikipedia";
const match_type = "firefox-suggest";
const index = 1;
const position = index + 1;

add_setup(async function () {
  await setUpTelemetryTest({
    merinoSuggestions: [MERINO_SUGGESTION],
  });
});

add_task(async function () {
  await doTelemetryTest({
    index,
    suggestion: MERINO_SUGGESTION,
    // impression-only
    impressionOnly: {
      scalars: {
        [TELEMETRY_SCALARS.IMPRESSION_DYNAMIC_WIKIPEDIA]: position,
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
        [TELEMETRY_SCALARS.IMPRESSION_DYNAMIC_WIKIPEDIA]: position,
        [TELEMETRY_SCALARS.CLICK_DYNAMIC_WIKIPEDIA]: position,
        "urlbar.picked.dynamic_wikipedia": index.toString(),
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
      // dismiss
      {
        command: "dismiss",
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_DYNAMIC_WIKIPEDIA]: position,
          [TELEMETRY_SCALARS.BLOCK_DYNAMIC_WIKIPEDIA]: position,
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
      },
      // help
      {
        command: "help",
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_DYNAMIC_WIKIPEDIA]: position,
          [TELEMETRY_SCALARS.HELP_DYNAMIC_WIKIPEDIA]: position,
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
