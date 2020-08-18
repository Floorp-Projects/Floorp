"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);

function assertHistograms(histograms, expected, message) {
  for (let i = 0; i < histograms.length; i++) {
    const [name, histogram] = histograms[i];
    const actual = histogram.snapshot().values;
    const expectedValues = i < expected.length ? expected[i] : {};

    Assert.deepEqual(expectedValues, actual, `${name} - ${message}`);
  }
}

async function loadAndWaitForStop(tab, uri) {
  BrowserUsageTelemetry._lastRecordSiteOriginsPerLoadedTabs = 0;

  await Promise.all([
    BrowserTestUtils.loadURI(tab.linkedBrowser, uri),
    BrowserTestUtils.browserStopped(tab.linkedBrowser, uri),
  ]);

  // Yield the event loop after the document loads to wait for the telemetry
  // publish in the Fission case.
  await new Promise(resolve => executeSoon(resolve));
  await new Promise(resolve => executeSoon(resolve));
}

async function openNewForegroundTab(uri) {
  BrowserUsageTelemetry._lastRecordSiteOriginsPerLoadedTabs = 0;

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: uri,
    waitForStateStop: true,
  });

  // Yield the event loop after the document loads to wait for the telemetry
  // publish in the Fission case.
  await new Promise(resolve => executeSoon(resolve));
  return tab;
}

add_task(async function test_siteOriginsPerLoadedTabsHistogram() {
  const histograms = BrowserUsageTelemetry._siteOriginHistogramIds.map(
    ([, , name]) => [name, TelemetryTestUtils.getAndClearHistogram(name)]
  );
  assertHistograms(histograms, [], "initial");

  dump("*** *** load example.com (1)\n");
  await loadAndWaitForStop(gBrowser.selectedTab, "http://example.com/");

  // We have one origin open in one tab. (100 * 1 / 1)
  assertHistograms(
    histograms,
    [
      { 75: 0, 88: 1, 103: 0 }, // 1 loaded tab.
    ],
    "navigate to https://example.com"
  );

  let tabs = [await openNewForegroundTab("http://example.com/")];

  // We have one origin open in two tabs. (100 * 1 / 2)
  assertHistograms(
    histograms,
    [
      { 75: 0, 88: 1, 103: 0 }, // 1 loaded tab.
      { 40: 0, 47: 1, 55: 0 }, // 2 <= N < 5 loaded tabs.
    ],
    "opening a new tab containing to https://example.com"
  );

  // Open three new tabs
  tabs.push(
    await openNewForegroundTab("http://example.com/"),
    await openNewForegroundTab("http://example.com/"),
    await openNewForegroundTab("http://example.com/")
  );

  // We have one origin open in five tabs. (100 * 1 / 5)
  assertHistograms(
    histograms,
    [
      { 75: 0, 88: 1, 103: 0 }, // 1 loaded tab.
      { 21: 0, 25: 1, 29: 1, 47: 1, 55: 0 }, // 2 <= N < 5 loaded tabs.
      { 15: 0, 18: 1, 21: 0 }, // 5 <= N < 10 loaded tabs.
    ],
    "open a three new tabs containing https://example.com"
  );

  for (const tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
  tabs = [];

  await loadAndWaitForStop(
    gBrowser.selectedTab,
    `${TEST_ROOT}/multiple_iframes.html`
  );

  // We have 10 origins open in 1 tab (100 * 10 / 1)
  assertHistograms(
    histograms,
    [
      { 75: 0, 88: 1, 907: 1, 1059: 0 }, // 1 loaded tab.
      { 21: 0, 25: 1, 29: 1, 47: 1, 55: 0 }, // 2 <= N < 5 loaded tabs.
      { 15: 0, 18: 1, 21: 0 }, // 5 <= N < 10 loaded tabs.
    ],
    "navigate to a new tab containing multiple origins"
  );

  tabs.push(await openNewForegroundTab(`${TEST_ROOT}/multiple_iframes.html`));

  // We have 10 origins open in 2 tabs (100 * 10 / 2)
  assertHistograms(
    histograms,
    [
      { 75: 0, 88: 1, 907: 1, 1059: 0 }, // 1 loaded tab.
      { 21: 0, 25: 1, 29: 1, 47: 1, 487: 1, 569: 0 }, // 2 <= N < 5 loaded tabs.
      { 15: 0, 18: 1, 21: 0 }, // 5 <= N < 10 loaded tabs.
    ],
    "navigate to a new tab containing multiple origins"
  );

  tabs.push(await openNewForegroundTab(`${TEST_ROOT}/multiple_iframes.html`));

  // We have 10 origins open in 3 tabs (100 * 10 / 3)
  assertHistograms(
    histograms,
    [
      { 75: 0, 88: 1, 907: 1, 1059: 0 }, // 1 loaded tab.
      { 21: 0, 25: 1, 29: 1, 47: 1, 306: 1, 487: 1, 569: 0 }, // 2 <= N < 5 loaded tabs.
      { 15: 0, 18: 1, 21: 0 }, // 5 <= N < 10 loaded tabs.
    ],
    "navigate to a new tab containing multiple origins"
  );

  for (const tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});
