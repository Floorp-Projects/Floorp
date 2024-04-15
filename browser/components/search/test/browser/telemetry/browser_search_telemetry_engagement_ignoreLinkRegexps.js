/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests ignoreLinkRegexps property in search telemetry that explicitly results
 * in our network code ignoring the link. The main reason for doing so is for
 * rare situations where we need to find a components from a topDown approach
 * but it loads a page in the network process.
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
    ignoreLinkRegexps: [
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_searchbox_with_content.html\?s=test&page=images/,
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_searchbox_with_content.html\?s=test&page=shopping/,
    ],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
    hrefToComponentMapAugmentation: [
      {
        action: "clicked_something",
        target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        url: "https://example.org/browser/browser/components/search/test/browser/telemetry/searchTelemetryAd_searchbox.html",
      },
    ],
  },
];

// The impression doesn't change in these tests.
const IMPRESSION = {
  provider: "example",
  tagged: "true",
  partner_code: "ff",
  source: "unknown",
  is_shopping_page: "false",
  is_private: "false",
  shopping_tab_displayed: "false",
  is_signed_in: "false",
};

const SERP_URL = getSERPUrl("searchTelemetryAd_searchbox_with_content.html");

async function replaceIncludedProperty(included) {
  TEST_PROVIDER_INFO[0].components = [
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
      included,
      topDown: true,
    },
  ];
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

add_task(async function test_click_link_1_matching_ignore_link_regexps() {
  resetTelemetry();

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#images",
    {},
    tab.linkedBrowser
  );
  await promise;

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});

add_task(async function test_click_link_2_matching_ignore_link_regexps() {
  resetTelemetry();

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#shopping",
    {},
    tab.linkedBrowser
  );
  await promise;

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      abandonment: {
        reason: SearchSERPTelemetryUtils.ABANDONMENTS.NAVIGATION,
      },
    },
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});

add_task(async function test_click_link_3_not_matching_ignore_link_regexps() {
  resetTelemetry();

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#extra",
    {},
    tab.linkedBrowser
  );
  await promise;

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: "clicked",
          target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
        },
      ],
    },
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});

add_task(async function test_click_listener_with_ignore_link_regexps() {
  resetTelemetry();

  TEST_PROVIDER_INFO[0].components = [
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
      topDown: true,
      included: {
        parent: {
          selector: "nav a",
          skipCount: true,
          eventListeners: [
            {
              eventType: "click",
              action: "clicked",
            },
          ],
        },
      },
    },
    {
      type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
      default: true,
    },
  ];
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#images",
    {},
    tab.linkedBrowser
  );
  await promise;

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: "clicked",
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
      ],
    },
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});
