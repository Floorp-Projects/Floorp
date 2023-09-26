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

const REMOTE_SETTINGS_RESULT = {
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

add_setup(async function () {
  await setUpTelemetryTest({
    remoteSettingsResults: [
      {
        type: "data",
        attachment: [REMOTE_SETTINGS_RESULT],
      },
    ],
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
      suggestion: REMOTE_SETTINGS_RESULT,
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
            suggested_index: -1,
            suggested_index_relative_to_group: true,
            improve_suggest_experience_checked,
            is_clicked: false,
            block_id: REMOTE_SETTINGS_RESULT.id,
            advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                is_clicked: true,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              },
            },
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
              payload: {
                match_type,
                position,
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                is_clicked: false,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              },
            },
            {
              type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
              payload: {
                match_type,
                position,
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
                iab_category: REMOTE_SETTINGS_RESULT.iab_category,
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
                suggested_index: -1,
                suggested_index_relative_to_group: true,
                improve_suggest_experience_checked,
                is_clicked: false,
                block_id: REMOTE_SETTINGS_RESULT.id,
                advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
  await QuickSuggestTestUtils.setConfig(
    QuickSuggestTestUtils.BEST_MATCH_CONFIG
  );
  await doTelemetryTest({
    index,
    suggestion: REMOTE_SETTINGS_RESULT,
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
          suggested_index: 1,
          suggested_index_relative_to_group: false,
          is_clicked: false,
          improve_suggest_experience_checked: false,
          block_id: REMOTE_SETTINGS_RESULT.id,
          advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              is_clicked: true,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
            payload: {
              match_type,
              position,
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
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
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              is_clicked: false,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
            payload: {
              match_type,
              position,
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
              iab_category: REMOTE_SETTINGS_RESULT.iab_category,
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
              suggested_index: 1,
              suggested_index_relative_to_group: false,
              is_clicked: false,
              improve_suggest_experience_checked: false,
              block_id: REMOTE_SETTINGS_RESULT.id,
              advertiser: REMOTE_SETTINGS_RESULT.advertiser,
            },
          },
        ],
      },
    },
  });
  await QuickSuggestTestUtils.setConfig(QuickSuggestTestUtils.DEFAULT_CONFIG);
  await SpecialPowers.popPrefEnv();
});
