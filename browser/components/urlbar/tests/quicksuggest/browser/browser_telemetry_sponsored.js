/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for sponsored suggestions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.sys.mjs",
});

const { TELEMETRY_SCALARS } = UrlbarProviderQuickSuggest;

const suggestion = {
  id: 1,
  url: "https://example.com/sponsored",
  title: "Sponsored suggestion",
  keywords: ["sponsored"],
  click_url: "https://example.com/click",
  impression_url: "https://example.com/impression",
  advertiser: "testadvertiser",
};

const suggestion_type = "sponsored";
const index = 1;
const position = index + 1;

add_setup(async function() {
  await setUpTelemetryTest({
    suggestions: [suggestion],
  });
});

// sponsored
add_task(async function sponsored() {
  let match_type = "firefox-suggest";

  // Make sure `improve_suggest_experience_checked` is recorded correctly
  // depending on the value of the related pref.
  for (let improve_suggest_experience_checked of [false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.urlbar.quicksuggest.dataCollection.enabled",
          improve_suggest_experience_checked,
        ],
      ],
    });
    await doTelemetryTest({
      index,
      suggestion,
      // impression-only
      impressionOnly: {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
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
        ping: {
          type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
          payload: {
            match_type,
            position,
            improve_suggest_experience_checked,
            is_clicked: false,
            block_id: suggestion.id,
            advertiser: suggestion.advertiser,
          },
        },
      },
      selectables: {
        // click
        "urlbarView-row-inner": {
          scalars: {
            [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
            [TELEMETRY_SCALARS.CLICK_SPONSORED]: position,
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
          pings: [
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
              payload: {
                match_type,
                position,
                improve_suggest_experience_checked,
                is_clicked: true,
                block_id: suggestion.id,
                advertiser: suggestion.advertiser,
              },
            },
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
              payload: {
                match_type,
                position,
                improve_suggest_experience_checked,
                block_id: suggestion.id,
                advertiser: suggestion.advertiser,
              },
            },
          ],
        },
        // block
        "urlbarView-button-block": {
          scalars: {
            [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
            [TELEMETRY_SCALARS.BLOCK_SPONSORED]: position,
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
          pings: [
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
              payload: {
                match_type,
                position,
                improve_suggest_experience_checked,
                is_clicked: false,
                block_id: suggestion.id,
                advertiser: suggestion.advertiser,
              },
            },
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
              payload: {
                match_type,
                position,
                improve_suggest_experience_checked,
                block_id: suggestion.id,
                advertiser: suggestion.advertiser,
                iab_category: suggestion.iab_category,
              },
            },
          ],
        },
        // help
        "urlbarView-button-help": {
          scalars: {
            [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
            [TELEMETRY_SCALARS.HELP_SPONSORED]: position,
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
          pings: [
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
              payload: {
                match_type,
                position,
                improve_suggest_experience_checked,
                is_clicked: false,
                block_id: suggestion.id,
                advertiser: suggestion.advertiser,
              },
            },
          ],
        },
      },
    });
    await SpecialPowers.popPrefEnv();
  }
});

// sponsored best match
add_task(async function sponsoredBestMatch() {
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
        [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
        [TELEMETRY_SCALARS.IMPRESSION_SPONSORED_BEST_MATCH]: position,
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
      ping: {
        type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
        payload: {
          match_type,
          position,
          is_clicked: false,
          improve_suggest_experience_checked: false,
          block_id: suggestion.id,
          advertiser: suggestion.advertiser,
        },
      },
    },
    selectables: {
      // click
      "urlbarView-row-inner": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
          [TELEMETRY_SCALARS.IMPRESSION_SPONSORED_BEST_MATCH]: position,
          [TELEMETRY_SCALARS.CLICK_SPONSORED]: position,
          [TELEMETRY_SCALARS.CLICK_SPONSORED_BEST_MATCH]: position,
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
        pings: [
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
            payload: {
              match_type,
              position,
              is_clicked: true,
              improve_suggest_experience_checked: false,
              block_id: suggestion.id,
              advertiser: suggestion.advertiser,
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
            payload: {
              match_type,
              position,
              improve_suggest_experience_checked: false,
              block_id: suggestion.id,
              advertiser: suggestion.advertiser,
            },
          },
        ],
      },
      // block
      "urlbarView-button-block": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
          [TELEMETRY_SCALARS.IMPRESSION_SPONSORED_BEST_MATCH]: position,
          [TELEMETRY_SCALARS.BLOCK_SPONSORED]: position,
          [TELEMETRY_SCALARS.BLOCK_SPONSORED_BEST_MATCH]: position,
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
        pings: [
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
            payload: {
              match_type,
              position,
              is_clicked: false,
              improve_suggest_experience_checked: false,
              block_id: suggestion.id,
              advertiser: suggestion.advertiser,
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
            payload: {
              match_type,
              position,
              improve_suggest_experience_checked: false,
              block_id: suggestion.id,
              advertiser: suggestion.advertiser,
              iab_category: suggestion.iab_category,
            },
          },
        ],
      },
      // help
      "urlbarView-button-help": {
        scalars: {
          [TELEMETRY_SCALARS.IMPRESSION_SPONSORED]: position,
          [TELEMETRY_SCALARS.IMPRESSION_SPONSORED_BEST_MATCH]: position,
          [TELEMETRY_SCALARS.HELP_SPONSORED]: position,
          [TELEMETRY_SCALARS.HELP_SPONSORED_BEST_MATCH]: position,
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
        pings: [
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
            payload: {
              match_type,
              position,
              is_clicked: false,
              improve_suggest_experience_checked: false,
              block_id: suggestion.id,
              advertiser: suggestion.advertiser,
            },
          },
        ],
      },
    },
  });
  await SpecialPowers.popPrefEnv();
});
