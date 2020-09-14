/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Assert } = ChromeUtils.import("resource://testing-common/Assert.jsm");
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PRELOAD_PREF = "network.preload";

const PRELOAD_HISTOGRAMS = [
  "TIME_TO_LOAD_EVENT_START_PRELOAD_MS",
  "TIME_TO_LOAD_EVENT_END_PRELOAD_MS",
];

const NO_PRELOAD_HISTOGRAMS = [
  "TIME_TO_LOAD_EVENT_START_NO_PRELOAD_MS",
  "TIME_TO_LOAD_EVENT_END_NO_PRELOAD_MS",
];

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);

function setupTest(prefValue) {
  Services.prefs.setBoolPref(PRELOAD_PREF, prefValue);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PRELOAD_PREF);
  });

  const preloadHistograms = PRELOAD_HISTOGRAMS.map(name =>
    TelemetryTestUtils.getAndClearHistogram(name)
  );
  const noPreloadHistograms = NO_PRELOAD_HISTOGRAMS.map(name =>
    TelemetryTestUtils.getAndClearHistogram(name)
  );

  return [preloadHistograms, noPreloadHistograms];
}

function telemetrySubmitted(preloadHistograms, noPreloadHistograms) {
  function reducer(acc, histogram) {
    if (Object.values(histogram.snapshot().values).length) {
      acc += 1;
    }
    return acc;
  }

  return TestUtils.waitForCondition(() => {
    return (
      preloadHistograms.reduce(reducer, 0) == 2 ||
      noPreloadHistograms.reduce(reducer, 0) == 2
    );
  }, "Waiting for telemetry to be collected");
}

async function runTest({
  uri,
  preloadHistograms,
  noPreloadHistograms,
  shouldPreload,
}) {
  await Promise.all([
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, uri),
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, undefined, uri),
  ]);

  await telemetrySubmitted(preloadHistograms, noPreloadHistograms);

  const [expected, unexpected] = shouldPreload
    ? [preloadHistograms, noPreloadHistograms]
    : [noPreloadHistograms, preloadHistograms];

  // We don't know what the actual load times will be. We just know there
  // should be exactly one key in each of these histograms that has the value
  // 1.
  for (const histogram of expected) {
    const values = Object.values(histogram.snapshot().values).filter(
      value => value > 0
    );
    ok(values.length == 1, `${histogram.name()} should contain an entry`);
    histogram.clear();
  }
  for (const histogram of unexpected) {
    Assert.deepEqual(
      histogram.snapshot().values,
      {},
      `${histogram.name()} should not contain an entry`
    );
    histogram.clear();
  }
}

const TESTS = [
  {
    uri: `${TEST_ROOT}contain_ifame.html`,
    shouldPreload: false,
  },
  {
    uri: `${TEST_ROOT}preload_iframe.html`,
    shouldPreload: true,
  },
  {
    uri: `${TEST_ROOT}preload_iframe_nested.html`,
    shouldPreload: true,
  },
  {
    uri: `${TEST_ROOT}preload_link_header.sjs`,
    shouldPreload: true,
  },
  {
    uri: `${TEST_ROOT}preload_link_header.sjs?nested`,
    shouldPreload: true,
  },
];

add_task(async function test_loadNoPreload() {
  const [preloadHistograms, noPreloadHistograms] = setupTest(
    /* network.preload = */ false
  );

  for (const [i, test] of TESTS.entries()) {
    info(`Running test ${i} with network.preload=false: ${test.uri}`);
    await runTest({
      uri: test.uri,
      shouldPreload: test.shouldPreload,
      preloadHistograms,
      noPreloadHistograms,
    });
  }
});

add_task(async function test_loadPreload() {
  const [preloadHistograms, noPreloadHistograms] = setupTest(
    /* network.preload = */ true
  );

  for (const [i, test] of TESTS.entries()) {
    info(`Running test ${i} with network.preload=true: uri=${test.uri}`);
    await runTest({
      uri: test.uri,
      shouldPreload: test.shouldPreload,
      preloadHistograms,
      noPreloadHistograms,
    });
  }
});
