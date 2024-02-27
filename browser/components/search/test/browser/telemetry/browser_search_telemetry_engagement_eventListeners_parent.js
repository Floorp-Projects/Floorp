/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests eventListeners property on parent elements in topDown searches.
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

// The impression doesn't change in these tests.
const IMPRESSION = {
  provider: "example",
  tagged: "true",
  partner_code: "ff",
  source: "unknown",
  is_shopping_page: "false",
  is_private: "false",
  shopping_tab_displayed: "false",
};

const SELECTOR = ".arrow";
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

add_task(async function test_listeners_not_provided() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: SELECTOR,
    expectEngagement: false,
    tab,
  });

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});

add_task(async function test_no_listeners() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: SELECTOR,
    expectEngagement: false,
    tab,
  });

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});

add_task(async function test_click_listener() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "click",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: ".arrow-next",
    tab,
  });
  await synthesizePageAction({
    selector: ".arrow-prev",
    tab,
  });

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
        {
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
      ],
    },
  ]);

  await cleanup();
});

/**
 * The click event is by far our most used event so by default, we translate
 * a "click" eventType to a "clicked" action. If no action is provided for
 * another type of event, nothing should be reported.
 */
add_task(async function test_event_with_no_default_action_parent() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "mousedown",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: SELECTOR,
    expectEngagement: false,
    tab,
  });

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});

add_task(async function test_event_no_default_action_with_override() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "mousedown",
          action: "clicked",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: SELECTOR,
    tab,
  });

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
  ]);

  await cleanup();
});

add_task(async function test_target_override() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "click",
          target: "custom_target",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: SELECTOR,
    tab,
  });

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: "clicked",
          target: "custom_target",
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_target_and_action_override() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "click",
          target: "custom_target",
          action: "custom_action",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: SELECTOR,
    tab,
  });

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: "custom_action",
          target: "custom_target",
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_multiple_listeners() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "click",
          action: "clicked",
        },
        {
          eventType: "mouseover",
          action: "mouseovered",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await synthesizePageAction({
    selector: SELECTOR,
    tab,
  });
  await synthesizePageAction({
    selector: SELECTOR,
    tab,
    event: {
      type: "mouseover",
    },
  });

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: "clicked",
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
        {
          action: "mouseovered",
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_condition() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "keydown",
          action: "keydowned",
          condition: "keydownEnter",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await SpecialPowers.spawn(tab.linkedBrowser, [SELECTOR], async function (s) {
    let el = content.document.querySelector(s);
    el.focus();
  });

  await EventUtils.synthesizeKey("A");
  /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 10));

  let pageActionPromise = waitForPageWithAction();
  await EventUtils.synthesizeKey("KEY_Enter");
  await pageActionPromise;

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
      engagements: [
        {
          action: "keydowned",
          target: SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS,
        },
      ],
    },
  ]);

  await cleanup();
});

add_task(async function test_condition_invalid() {
  await replaceIncludedProperty({
    parent: {
      selector: ".refined-search-buttons .arrow",
      skipCount: true,
      eventListeners: [
        {
          eventType: "keydown",
          action: "keydowned",
          condition: "noConditionExistsWithThisName",
        },
      ],
    },
  });

  let { tab, cleanup } = await openSerpInNewTab(SERP_URL);

  await SpecialPowers.spawn(tab.linkedBrowser, [SELECTOR], async function (s) {
    let el = content.document.querySelector(s);
    el.focus();
  });

  await EventUtils.synthesizeKey("A");
  /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 10));

  await EventUtils.synthesizeKey("KEY_Enter");
  /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 10));

  assertSERPTelemetry([
    {
      impression: IMPRESSION,
    },
  ]);

  await cleanup();
});
