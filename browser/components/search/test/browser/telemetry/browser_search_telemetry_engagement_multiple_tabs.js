/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures that recorded telemetry is consistent even with multiple
 * tabs opened and closed.
 */

"use strict";

const TEST_PROVIDER_INFO = [
  {
    telemetryId: "example",
    searchPageRegexp:
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_/,
    queryParamNames: ["s"],
    codeParamName: "abc",
    taggedCodes: ["ff"],
    adServerAttributes: ["mozAttr"],
    nonAdsLinkRegexps: [
      /^https:\/\/example.org\/browser\/browser\/components\/search\/test\/browser\/telemetry\/searchTelemetryAd_nonAdsLink_redirect/,
    ],
    extraAdServersRegexps: [/^https:\/\/example\.com\/ad/],
    components: [
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
        included: {
          parent: {
            selector: "form",
          },
          children: [
            {
              // This isn't contained in any of the HTML examples but the
              // presence of the entry ensures that if it is not found during
              // a topDown examination, the next element in the array is
              // inspected and found.
              selector: "textarea",
            },
            {
              selector: "input",
            },
          ],
          related: {
            selector: "div",
          },
        },
        topDown: true,
      },
      {
        type: SearchSERPTelemetryUtils.COMPONENTS.AD_LINK,
        default: true,
      },
    ],
  },
];

// Deliberately make the web isolated process count as small as possible
// so that we don't have to create a ton of tabs to reuse a process.
const MAX_IPC = 1;
const TABS_TO_OPEN = 2;

add_setup(async function () {
  SearchSERPTelemetry.overrideSearchTelemetryForTests(TEST_PROVIDER_INFO);
  await waitForIdle();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.log", true],
      ["browser.search.serpEventTelemetry.enabled", true],
      ["dom.ipc.processCount.webIsolated", MAX_IPC],
    ],
  });

  registerCleanupFunction(async () => {
    SearchSERPTelemetry.overrideSearchTelemetryForTests();
    resetTelemetry();
  });
});

async function do_test(tab, impressionId, switchTab) {
  if (switchTab) {
    await BrowserTestUtils.switchTab(gBrowser, tab);
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "input",
    {},
    tab.linkedBrowser
  );

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "a#images",
    {},
    tab.linkedBrowser
  );

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, true);

  await Services.fog.testFlushAllChildren();
  let engagements = Glean.serp.engagement.testGetValue() ?? [];
  Assert.equal(engagements.length, 2, "Should have two events recorded.");

  Assert.deepEqual(
    engagements[0].extra,
    {
      action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
      target: SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
      impression_id: impressionId,
    },
    "Search box engagement event should match."
  );
  Assert.deepEqual(
    engagements[1].extra,
    {
      action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
      target: SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK,
      impression_id: impressionId,
    },
    "Non ads page engagement event should match."
  );
  resetTelemetry();
}

// This test deliberately opens a lot of tabs to ensure SERPs share the
// same process. It interacts with the page to ensure the engagement
// has the correct recording, especially the impression id which can be out of
// sync if data in the child process isn't cached properly.
add_task(async function test_multiple_tabs_forward() {
  resetTelemetry();

  let tabs = [];
  let pid;

  // Open multiple tabs.
  for (let index = 0; index < TABS_TO_OPEN; ++index) {
    let url = getSERPUrl(
      "searchTelemetryAd_searchbox_with_content.html",
      `hello+world+${index}`
    );
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    await waitForPageWithAdImpressions();
    tabs.push(tab);
    let currentPid = E10SUtils.getBrowserPids(tab.linkedBrowser).at(0);
    if (pid == null) {
      pid = currentPid;
    } else {
      Assert.ok(pid == currentPid, "The process ID should be the same.");
    }
  }

  // Extract the impression IDs.
  await Services.fog.testFlushAllChildren();
  let recordedImpressions = Glean.serp.impression.testGetValue() ?? [];
  let impressionIds = recordedImpressions.map(
    impression => impression.extra.impression_id
  );

  // Reset telemetry because we're not concerned about inspecting every
  // impression event.
  resetTelemetry();

  for (let index = 0; index < TABS_TO_OPEN; ++index) {
    let tab = tabs[index];
    let impressionId = impressionIds[index];
    await do_test(tab, impressionId, true);
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_multiple_tabs_backward() {
  resetTelemetry();

  let tabs = [];
  let pid;

  for (let index = 0; index < TABS_TO_OPEN; ++index) {
    let url = getSERPUrl(
      "searchTelemetryAd_searchbox_with_content.html",
      `hello+world+${index}`
    );
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    await waitForPageWithAdImpressions();
    tabs.push(tab);
    let currentPid = E10SUtils.getBrowserPids(tab.linkedBrowser).at(0);
    if (pid == null) {
      pid = currentPid;
    } else {
      Assert.ok(pid == currentPid, "The process ID should be the same.");
    }
  }

  // Extract the impression IDs.
  await Services.fog.testFlushAllChildren();
  let recordedImpressions = Glean.serp.impression.testGetValue() ?? [];
  let impressionIds = recordedImpressions.map(
    impression => impression.extra.impression_id
  );

  // Reset telemetry because we're not concerned about inspecting every
  // impression event.
  resetTelemetry();

  for (let index = TABS_TO_OPEN - 1; index >= 0; --index) {
    let tab = tabs[index];
    let impressionId = impressionIds[index];
    await do_test(tab, impressionId, false);
    BrowserTestUtils.removeTab(tab);
  }
});
