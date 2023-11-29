/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests that Glean handles empty request IDs properly.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.sys.mjs",
});

const { TELEMETRY_SCALARS } = UrlbarProviderQuickSuggest;

const MERINO_RESULT = {
  block_id: 1,
  url: "https://example.com/sponsored",
  title: "Sponsored suggestion",
  keywords: ["sponsored"],
  click_url: "https://example.com/click",
  impression_url: "https://example.com/impression",
  advertiser: "testadvertiser",
  iab_category: "22 - Shopping",
  provider: "adm",
  is_sponsored: true,
};

const suggestion_type = "sponsored";
const index = 1;
const position = index + 1;

// Trying to avoid timeouts in TV mode.
requestLongerTimeout(3);

add_setup(async function () {
  await setUpTelemetryTest({
    merinoSuggestions: [MERINO_RESULT],
  });
  MerinoTestUtils.server.response.body.request_id = "";
});

// sponsored
add_task(async function sponsored() {
  let match_type = "firefox-suggest";
  let source = "merino";

  let improve_suggest_experience_checked = true;

  await doTelemetryTest({
    index,
    suggestion: MERINO_RESULT,
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
          source,
          match_type,
          position,
          suggested_index: -1,
          suggested_index_relative_to_group: true,
          improve_suggest_experience_checked,
          is_clicked: false,
          block_id: MERINO_RESULT.block_id,
          advertiser: MERINO_RESULT.advertiser,
          request_id: "",
        },
      },
    },
    // click
    click: {
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
            source,
            match_type,
            position,
            suggested_index: -1,
            suggested_index_relative_to_group: true,
            improve_suggest_experience_checked,
            is_clicked: true,
            block_id: MERINO_RESULT.block_id,
            advertiser: MERINO_RESULT.advertiser,
            request_id: "",
          },
        },
        {
          type: CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
          payload: {
            source,
            match_type,
            position,
            suggested_index: -1,
            suggested_index_relative_to_group: true,
            improve_suggest_experience_checked,
            block_id: MERINO_RESULT.block_id,
            advertiser: MERINO_RESULT.advertiser,
            request_id: "",
          },
        },
      ],
    },
    commands: [
      // dismiss
      {
        command: "dismiss",
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
              source,
              match_type,
              position,
              suggested_index: -1,
              suggested_index_relative_to_group: true,
              improve_suggest_experience_checked,
              is_clicked: false,
              block_id: MERINO_RESULT.block_id,
              advertiser: MERINO_RESULT.advertiser,
              request_id: "",
            },
          },
          {
            type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
            payload: {
              source,
              match_type,
              position,
              suggested_index: -1,
              suggested_index_relative_to_group: true,
              improve_suggest_experience_checked,
              block_id: MERINO_RESULT.block_id,
              advertiser: MERINO_RESULT.advertiser,
              iab_category: MERINO_RESULT.iab_category,
              request_id: "",
            },
          },
        ],
      },
      // help
      {
        command: "help",
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
              source,
              match_type,
              position,
              suggested_index: -1,
              suggested_index_relative_to_group: true,
              improve_suggest_experience_checked,
              is_clicked: false,
              block_id: MERINO_RESULT.block_id,
              advertiser: MERINO_RESULT.advertiser,
              request_id: "",
            },
          },
        ],
      },
    ],
  });
});
