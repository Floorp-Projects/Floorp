/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests skipCount property on elements in the children.
 */

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
  },
];

const IMPRESSION = {
  provider: "example",
  tagged: "true",
  partner_code: "ff",
  source: "unknown",
  is_shopping_page: "false",
  is_private: "false",
  shopping_tab_displayed: "false",
};

const SERP_URL = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");

async function replaceIncludedProperty(included) {
  let components = [
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
      included,
      topDown: true,
    },
  ];
  TEST_PROVIDER_INFO[0].components = components;
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
}

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    Services.telemetry.canRecordExtended = oldCanRecord;
    resetTelemetry();
  });
});

// For older clients, skipCount won't be available.
add_task(async function test_skip_count_not_provided() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons",
    },
    children: [
      {
        selector: "a",
      },
    ],
  });

  let { cleanup } = await openSerpInNewTab(SERP_URL);

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_skip_count_is_false() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons",
    },
    children: [
      {
        selector: "a",
        skipCount: false,
      },
    ],
  });

  let { cleanup } = await openSerpInNewTab(SERP_URL);

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: [
        {
          component: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
          ads_loaded: "1",
          ads_visible: "1",
          ads_hidden: "0",
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_skip_count_is_true() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons",
    },
    children: [
      {
        selector: "a",
        skipCount: true,
      },
    ],
  });

  let { cleanup } = await openSerpInNewTab(SERP_URL);

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      adImpressions: [],
    },
  ]);

  await cleanup();
});
