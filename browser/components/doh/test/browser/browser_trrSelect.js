/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(setup);

add_task(async function testTRRSelect() {
  // Set up the resolver lists in the default and user pref branches.
  // dummyTRR3 which only exists in the user-branch value should be ignored.
  let oldResolverList = Services.prefs.getCharPref("network.trr.resolvers");
  Services.prefs
    .getDefaultBranch("")
    .setCharPref(
      "network.trr.resolvers",
      `[{"url": "https://dummytrr.com/query"}, {"url": "https://dummytrr2.com/query"}]`
    );
  Services.prefs.setCharPref(
    "network.trr.resolvers",
    `[{"url": "https://dummytrr.com/query"}, {"url": "https://dummytrr2.com/query"}, {"url": "https://dummytrr3.com/query"}]`
  );

  // Clean start: doh-rollout.uri should be set after init.
  setPassingHeuristics();
  let prefPromise = TestUtils.waitForPrefChange(prefs.BREADCRUMB_PREF);
  Preferences.set(prefs.ENABLED_PREF, true);
  await prefPromise;
  is(Preferences.get(prefs.BREADCRUMB_PREF), true, "Breadcrumb saved.");
  is(
    Preferences.get(prefs.TRR_SELECT_URI_PREF),
    "https://dummytrr.com/query",
    "TRR selection complete."
  );

  // Wait for heuristics to complete.
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

  // Reset and restart the controller for good measure.
  Preferences.reset(prefs.TRR_SELECT_DRY_RUN_RESULT_PREF);
  Preferences.reset(prefs.TRR_SELECT_URI_PREF);

  prefPromise = TestUtils.waitForPrefChange(prefs.TRR_SELECT_URI_PREF);
  await restartDoHController();
  await prefPromise;
  is(
    Preferences.get(prefs.TRR_SELECT_URI_PREF),
    "https://dummytrr.com/query",
    "TRR selection complete."
  );

  // Wait for heuristics to complete.
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

  // Disable committing and reset. The committed URI should be cleared but the
  // dry-run-result should persist.
  Preferences.set(prefs.TRR_SELECT_COMMIT_PREF, false);
  prefPromise = TestUtils.waitForPrefChange(prefs.TRR_SELECT_URI_PREF);
  await restartDoHController();
  await prefPromise;
  ok(!Preferences.isSet(prefs.TRR_SELECT_URI_PREF), "TRR selection cleared.");
  try {
    await BrowserTestUtils.waitForCondition(() => {
      return !Preferences.isSet(prefs.TRR_SELECT_DRY_RUN_RESULT_PREF);
    });
    ok(false, "Dry run result was cleared, fail!");
  } catch (e) {
    ok(true, "Dry run result was not cleared.");
  }
  is(
    Preferences.get(prefs.TRR_SELECT_DRY_RUN_RESULT_PREF),
    "https://dummytrr.com/query",
    "dry-run result has the correct value."
  );

  // Wait for heuristics to complete.
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

  // Reset and restart again, dry-run-result should be recorded but not
  // be committed. Committing is still disabled from above.
  Preferences.reset(prefs.TRR_SELECT_DRY_RUN_RESULT_PREF);
  Preferences.reset(prefs.TRR_SELECT_URI_PREF);
  await restartDoHController();
  try {
    await BrowserTestUtils.waitForCondition(() => {
      return Preferences.get(prefs.TRR_SELECT_URI_PREF);
    });
    ok(false, "Dry run result got committed, fail!");
  } catch (e) {
    ok(true, "Dry run result did not get committed");
  }
  is(
    Preferences.get(prefs.TRR_SELECT_DRY_RUN_RESULT_PREF),
    "https://dummytrr.com/query",
    "TRR selection complete, dry-run result recorded."
  );
  Preferences.set(prefs.TRR_SELECT_COMMIT_PREF, true);

  // Wait for heuristics to complete.
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

  // Reset doh-rollout.uri, and change the dry-run-result to another one on the
  // default list. After init, the existing dry-run-result should be committed.
  Preferences.reset(prefs.TRR_SELECT_URI_PREF);
  Preferences.set(
    prefs.TRR_SELECT_DRY_RUN_RESULT_PREF,
    "https://dummytrr2.com/query"
  );
  prefPromise = TestUtils.waitForPrefChange(prefs.TRR_SELECT_URI_PREF);
  await restartDoHController();
  await prefPromise;
  is(
    Preferences.get(prefs.TRR_SELECT_URI_PREF),
    "https://dummytrr2.com/query",
    "TRR selection complete, existing dry-run-result committed."
  );

  // Wait for heuristics to complete.
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

  // Reset doh-rollout.uri, and change the dry-run-result to another one NOT on
  // default list. After init, a new TRR should be selected and committed.
  Preferences.reset(prefs.TRR_SELECT_URI_PREF);
  Preferences.set(
    prefs.TRR_SELECT_DRY_RUN_RESULT_PREF,
    "https://dummytrr3.com/query"
  );
  prefPromise = TestUtils.waitForPrefChange(prefs.TRR_SELECT_URI_PREF);
  await restartDoHController();
  await prefPromise;
  is(
    Preferences.get(prefs.TRR_SELECT_URI_PREF),
    "https://dummytrr.com/query",
    "TRR selection complete, existing dry-run-result discarded and refreshed."
  );

  // Wait for heuristics to complete.
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

  Services.prefs
    .getDefaultBranch("")
    .setCharPref("network.trr.resolvers", oldResolverList);
  Services.prefs.clearUserPref("network.trr.resolvers");
});
