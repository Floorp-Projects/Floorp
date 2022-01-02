/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

SimpleTest.requestFlakyTimeout("Need to test expiration timeout");

function delay(msec) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, msec));
}

function promiseIdle() {
  return new Promise(resolve => {
    Services.tm.idleDispatchToMainThread(resolve);
  });
}

const ORIGIN_CAP = 5;
const SLIDING_WINDOW_MS = 5000;

const PREF_ORIGIN_CAP = "fission.experiment.max-origins.origin-cap";
const PREF_SLIDING_WINDOW_MS =
  "fission.experiment.max-origins.sliding-window-ms";
const PREF_QUALIFIED = "fission.experiment.max-origins.qualified";
const PREF_LAST_QUALIFIED = "fission.experiment.max-origins.last-qualified";
const PREF_LAST_DISQUALIFIED =
  "fission.experiment.max-origins.last-disqualified";

const SITE_ORIGINS = [
  "http://example.com/",
  "http://example.org/",
  "http://example.net/",
  "http://example.tw/",
  "http://example.cn/",
  "http://example.fi/",
  "http://example.in/",
  "http://example.lk/",
  "http://w3c-test.org/",
  "https://www.mozilla.org/",
];

function openTab(url) {
  return BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url,
    waitForStateStop: true,
  });
}

async function assertQualified() {
  // The unique origin calculation runs from an idle task, so make sure
  // the queued idle task has had a chance to run.
  await promiseIdle();

  // Make sure the clock has advanced since the qualification timestamp
  // was recorded.
  await delay(1);

  let qualified = Services.prefs.getBoolPref(PREF_QUALIFIED);
  let lastQualified = Services.prefs.getIntPref(PREF_LAST_QUALIFIED);
  let lastDisqualified = Services.prefs.getIntPref(PREF_LAST_DISQUALIFIED);
  let currentTime = Date.now() / 1000;

  ok(qualified, "Should be qualified");
  ok(
    lastQualified > 0,
    `Last qualified timestamp (${lastQualified}) should be greater than 0`
  );
  ok(
    lastQualified < currentTime,
    `Last qualified timestamp (${lastQualified}) should be less than the current time (${currentTime})`
  );
  ok(
    lastQualified > lastDisqualified,
    `Last qualified timestamp (${lastQualified}) should be after the last disqualified time (${lastDisqualified})`
  );

  ok(
    lastDisqualified < currentTime,
    `Last disqualified timestamp (${lastDisqualified}) should be less than the current time (${currentTime})`
  );
}

async function assertDisqualified(opts) {
  // The unique origin calculation runs from an idle task, so make sure
  // the queued idle task has had a chance to run.
  await promiseIdle();

  let qualified = Services.prefs.getBoolPref(PREF_QUALIFIED);
  let lastQualified = Services.prefs.getIntPref(PREF_LAST_QUALIFIED, 0);
  let lastDisqualified = Services.prefs.getIntPref(PREF_LAST_DISQUALIFIED);
  let currentTime = Date.now() / 1000;

  ok(!qualified, "Should not be qualified");
  if (!opts.initialValues) {
    ok(
      lastQualified > 0,
      `Last qualified timestamp (${lastQualified}) should be greater than 0`
    );
  }
  ok(
    lastQualified < currentTime,
    `Last qualified timestamp (${lastQualified}) should be less than the current time (${currentTime})`
  );

  ok(
    lastDisqualified < currentTime,
    `Last disqualified timestamp (${lastDisqualified}) should be less than the current time (${currentTime})`
  );

  ok(
    lastDisqualified > 0,
    `Last disqualified timestamp (${lastDisqualified}) should be greater than 0`
  );

  if (opts.qualificationPending) {
    ok(
      lastQualified > lastDisqualified,
      `Last qualified timestamp (${lastQualified}) should be after the last disqualified time (${lastDisqualified})`
    );
  } else {
    ok(
      lastDisqualified > lastQualified,
      `Last disqualified timestamp (${lastDisqualified}) should be after the last qualified time (${lastQualified})`
    );
  }
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_ORIGIN_CAP, ORIGIN_CAP],
      [PREF_SLIDING_WINDOW_MS, SLIDING_WINDOW_MS],
    ],
  });

  const { BrowserTelemetryUtils } = ChromeUtils.import(
    "resource://gre/modules/BrowserTelemetryUtils.jsm"
  );

  // Make sure we actually record telemetry for our disqualifying origin
  // count.
  BrowserTelemetryUtils.min_interval = 1;

  let tabs = [];

  // Open one initial tab to make sure the origin counting code has had
  // a chance to run before checking the initial state.
  tabs.push(await openTab("http://mochi.test:8888/"));

  await assertQualified();

  let lastDisqualified = Services.prefs.getIntPref(PREF_LAST_DISQUALIFIED);
  is(lastDisqualified, 0, "Last disqualification timestamp should be 0");

  info(
    `Opening ${SITE_ORIGINS.length} tabs with distinct origins to exceed the cap (${ORIGIN_CAP})`
  );
  ok(
    SITE_ORIGINS.length > ORIGIN_CAP,
    "Should have enough site origins to exceed the origin cap"
  );
  tabs.push(...(await Promise.all(SITE_ORIGINS.map(openTab))));

  await assertDisqualified({ qualificationPending: false });

  info("Close unique-origin tabs");
  await Promise.all(tabs.map(tab => BrowserTestUtils.removeTab(tab)));

  info("Open a new tab to trigger the origin count code once more");
  tabs = [await openTab(SITE_ORIGINS[0])];

  await assertDisqualified({ qualificationPending: true });

  info(
    "Wait long enough to clear the sliding window since last disqualified state"
  );
  await delay(SLIDING_WINDOW_MS + 1000);

  info("Open a new tab to trigger the origin count code again");
  tabs.push(await openTab(SITE_ORIGINS[0]));

  await assertQualified();

  info(
    "Clear preference values and re-populate the initial value from telemetry"
  );
  Services.prefs.clearUserPref(PREF_QUALIFIED);
  Services.prefs.clearUserPref(PREF_LAST_QUALIFIED);
  Services.prefs.clearUserPref(PREF_LAST_DISQUALIFIED);
  BrowserTelemetryUtils._checkedInitialExperimentQualification = false;

  info("Open a new tab to trigger the origin count code again");
  tabs.push(await openTab(SITE_ORIGINS[0]));

  await assertDisqualified({ initialValues: true });

  info(
    "Wait long enough to clear the sliding window since last disqualified state"
  );
  await delay(SLIDING_WINDOW_MS + 1000);

  info("Open a new tab to trigger the origin count code again");
  tabs.push(await openTab(SITE_ORIGINS[0]));

  await assertQualified();

  await Promise.all(tabs.map(tab => BrowserTestUtils.removeTab(tab)));

  // Clear the cached recording interval so it resets to the default
  // value on the next call.
  BrowserTelemetryUtils.min_interval = null;
});
