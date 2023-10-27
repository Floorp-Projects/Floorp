/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests ingest in the Rust backend.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

// These consts are copied from the update timer manager test. See
// `initUpdateTimerManager()`.
const PREF_APP_UPDATE_TIMERMINIMUMDELAY = "app.update.timerMinimumDelay";
const PREF_APP_UPDATE_TIMERFIRSTINTERVAL = "app.update.timerFirstInterval";
const MAIN_TIMER_INTERVAL = 1000; // milliseconds
const CATEGORY_UPDATE_TIMER = "update-timer";

const REMOTE_SETTINGS_SUGGESTION = {
  id: 1,
  url: "http://example.com/amp",
  title: "AMP Suggestion",
  keywords: ["amp"],
  click_url: "http://example.com/amp-click",
  impression_url: "http://example.com/amp-impression",
  advertiser: "Amp",
  iab_category: "22 - Shopping",
  icon: "1234",
};

add_setup(async function () {
  initUpdateTimerManager();

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: [REMOTE_SETTINGS_SUGGESTION],
      },
    ],
    prefs: [
      ["suggest.quicksuggest.sponsored", true],
      ["quicksuggest.rustEnabled", false],
    ],
  });
});

// IMPORTANT: This task must run first!
//
// This simulates the first time the Rust backend is enabled in a profile. The
// backend should perform ingestion immediately.
add_task(async function firstRun() {
  Assert.ok(
    !UrlbarPrefs.get("quicksuggest.rustEnabled"),
    "rustEnabled pref is initially false (this task must run first!)"
  );
  Assert.strictEqual(
    QuickSuggest.rustBackend.isEnabled,
    false,
    "Rust backend is initially disabled (this task must run first!)"
  );
  Assert.ok(
    !QuickSuggest.rustBackend.ingestPromise,
    "No ingest has been performed yet (this task must run first!)"
  );

  info("Enabling the Rust backend");
  UrlbarPrefs.set("quicksuggest.rustEnabled", true);
  Assert.ok(QuickSuggest.rustBackend.isEnabled, "Rust backend is now enabled");
  let { ingestPromise } = QuickSuggest.rustBackend;
  Assert.ok(ingestPromise, "Ingest started");

  info("Awaiting ingest promise");
  await ingestPromise;
  info("Done awaiting ingest promise");

  await checkSuggestions();

  // Disable and re-enable the backend. No new ingestion should start
  // immediately since this isn't the first time the backend has been enabled.
  UrlbarPrefs.set("quicksuggest.rustEnabled", false);
  UrlbarPrefs.set("quicksuggest.rustEnabled", true);
  Assert.equal(
    QuickSuggest.rustBackend.ingestPromise,
    ingestPromise,
    "No new ingest started"
  );

  await checkSuggestions();

  UrlbarPrefs.set("quicksuggest.rustEnabled", false);
});

// Ingestion should be performed according to the defined interval.
add_task(async function interval() {
  let { ingestPromise } = QuickSuggest.rustBackend;
  Assert.ok(
    ingestPromise,
    "Sanity check: An ingest has already been performed"
  );
  Assert.ok(
    !UrlbarPrefs.get("quicksuggest.rustEnabled"),
    "Sanity check: Rust backend is initially disabled"
  );

  // Set a small interval and enable the backend. No new ingestion should start
  // immediately since this isn't the first time the backend has been enabled.
  let intervalSecs = 1;
  UrlbarPrefs.set("quicksuggest.rustIngestIntervalSeconds", intervalSecs);
  UrlbarPrefs.set("quicksuggest.rustEnabled", true);
  Assert.equal(
    QuickSuggest.rustBackend.ingestPromise,
    ingestPromise,
    "No new ingest has started"
  );

  // Wait for a few ingests to happen.
  for (let i = 0; i < 3; i++) {
    info("Preparing for ingest at index " + i);

    // Set a new suggestion so we can make sure ingest really happened.
    let suggestion = {
      ...REMOTE_SETTINGS_SUGGESTION,
      url: REMOTE_SETTINGS_SUGGESTION.url + "/" + i,
    };
    await QuickSuggestTestUtils.setRemoteSettingsRecords(
      [
        {
          type: "data",
          attachment: [suggestion],
        },
      ],
      // Don't force sync since the whole point here is to make sure the backend
      // ingests on its own!
      { forceSync: false }
    );

    // Wait for ingest to start and finish.
    info("Waiting for ingest to start at index " + i);
    ({ ingestPromise } = await waitForIngestStart(ingestPromise));
    info("Waiting for ingest to finish at index " + i);
    await ingestPromise;
    await checkSuggestions([suggestion]);
  }

  // In the loop above, there was one additional async call after awaiting the
  // ingest promise, to `checkSuggestions()`. It's possible, though unlikely,
  // that call took so long that another ingest has started. To be sure, wait
  // for one final ingest to start before continuing.
  ({ ingestPromise } = await waitForIngestStart(ingestPromise));

  // Now immediately disable the backend. New ingests should not start, but the
  // final one will still be ongoing.
  info("Disabling the backend");
  UrlbarPrefs.set("quicksuggest.rustEnabled", false);

  info("Awaiting final ingest promise");
  await ingestPromise;

  // Wait a few seconds.
  let waitSecs = 3 * intervalSecs;
  info(`Waiting ${waitSecs}s...`);
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 1000 * waitSecs));

  // No new ingests should have started.
  Assert.equal(
    QuickSuggest.rustBackend.ingestPromise,
    ingestPromise,
    "No new ingest started after disabling the backend"
  );

  UrlbarPrefs.clear("quicksuggest.rustIngestIntervalSeconds");
});

async function waitForIngestStart(oldIngestPromise) {
  let newIngestPromise;
  await TestUtils.waitForCondition(() => {
    let { ingestPromise } = QuickSuggest.rustBackend;
    if (ingestPromise != oldIngestPromise) {
      newIngestPromise = ingestPromise;
      return true;
    }
    return false;
  }, "Waiting for a new ingest to start");

  Assert.equal(
    QuickSuggest.rustBackend.ingestPromise,
    newIngestPromise,
    "Sanity check: ingestPromise hasn't changed since waitForCondition returned"
  );

  // A bare promise can't be returned because it will cause the awaiting caller
  // to await that promise! We're simply trying to return the promise, which the
  // caller can later await.
  return { ingestPromise: newIngestPromise };
}

async function checkSuggestions(expected = [REMOTE_SETTINGS_SUGGESTION]) {
  let actual = await QuickSuggest.rustBackend.query("amp");
  Assert.deepEqual(
    actual.map(s => s.url),
    expected.map(s => s.url),
    "Backend should be serving the expected suggestions"
  );
}

/**
 * Sets up the update timer manager for testing: makes it fire more often,
 * removes all existing timers, and initializes it for testing. The body of this
 * function is copied from:
 * https://searchfox.org/mozilla-central/source/toolkit/components/timermanager/tests/unit/consumerNotifications.js
 */
function initUpdateTimerManager() {
  // Set the timer to fire every second
  Services.prefs.setIntPref(
    PREF_APP_UPDATE_TIMERMINIMUMDELAY,
    MAIN_TIMER_INTERVAL / 1000
  );
  Services.prefs.setIntPref(
    PREF_APP_UPDATE_TIMERFIRSTINTERVAL,
    MAIN_TIMER_INTERVAL
  );

  // Remove existing update timers to prevent them from being notified
  for (let { data: entry } of Services.catMan.enumerateCategory(
    CATEGORY_UPDATE_TIMER
  )) {
    Services.catMan.deleteCategoryEntry(CATEGORY_UPDATE_TIMER, entry, false);
  }

  Cc["@mozilla.org/updates/timer-manager;1"]
    .getService(Ci.nsIUpdateTimerManager)
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "utm-test-init", "");
}
