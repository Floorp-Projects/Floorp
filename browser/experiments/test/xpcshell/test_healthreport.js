/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/experiments/Experiments.jsm");
Cu.import("resource://testing-common/services/healthreport/utils.jsm");
Cu.import("resource://testing-common/services-common/logging.js");

const kMeasurementVersion = 2;

function getStorageAndProvider(name) {
  return Task.spawn(function* get() {
    let storage = yield Metrics.Storage(name);
    let provider = new ExperimentsProvider();
    yield provider.init(storage);

    return [storage, provider];
  });
}

function run_test() {
  run_next_test();
}

add_test(function setup() {
  do_get_profile();
  initTestLogging();

  Services.prefs.setBoolPref(PREF_EXPERIMENTS_ENABLED, true);
  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  Services.prefs.setBoolPref(PREF_HEALTHREPORT_ENABLED, true);

  run_next_test();
});

add_task(function test_constructor() {
  Experiments.instance();
  yield Experiments._mainTask;
  let provider = new ExperimentsProvider();
});

add_task(function* test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new ExperimentsProvider();
  yield provider.init(storage);
  yield provider.shutdown();
  yield storage.close();
});

add_task(function* test_collect() {
  let [storage, provider] = yield getStorageAndProvider("no_active");

  // Initial state should not report anything.
  yield provider.collectDailyData();
  let m = provider.getMeasurement("info", kMeasurementVersion);
  let values = yield m.getValues();
  Assert.equal(values.days.size, 0, "Have no data if no experiments known.");

  // An old experiment that ended today should be reported.
  replaceExperiments(provider._experiments, FAKE_EXPERIMENTS_2);
  let now = new Date(FAKE_EXPERIMENTS_2[0].endDate);
  defineNow(provider._experiments._policy, now.getTime());

  yield provider.collectDailyData();
  values = yield m.getValues();
  Assert.equal(values.days.size, 1, "Have 1 day of data");
  Assert.ok(values.days.hasDay(now), "Has day the experiment ended.");
  let day = values.days.getDay(now);
  Assert.ok(day.has("lastActive"), "Has lastActive field.");
  Assert.equal(day.get("lastActive"), "id2", "Last active ID is sane.");
  Assert.strictEqual(day.get("lastActiveBranch"), undefined,
		     "no branch should be set yet");

  // Making an experiment active replaces the lastActive value.
  replaceExperiments(provider._experiments, FAKE_EXPERIMENTS_1);
  yield provider.collectDailyData();
  values = yield m.getValues();
  day = values.days.getDay(now);
  Assert.equal(day.get("lastActive"), "id1", "Last active ID is the active experiment.");
  Assert.equal(day.get("lastActiveBranch"), "foo",
	       "Experiment branch should be visible");

  // And make sure the observer works.
  replaceExperiments(provider._experiments, FAKE_EXPERIMENTS_2);
  Services.obs.notifyObservers(null, "experiments-changed", null);
  // This may not wait long enough. It relies on the SQL insert happening
  // in the same tick as the observer notification.
  yield storage.enqueueOperation(function () {
    return Promise.resolve();
  });

  values = yield m.getValues();
  day = values.days.getDay(now);
  Assert.equal(day.get("lastActive"), "id2", "Last active ID set by observer.");

  yield provider.shutdown();
  yield storage.close();
});

add_task(function* test_healthreporterJSON() {
  let reporter = yield getHealthReporter("healthreporterJSON");
  yield reporter.init();
  try {
    yield reporter._providerManager.registerProvider(new ExperimentsProvider());
    let experiments = Experiments.instance();
    defineNow(experiments._policy, Date.now());
    replaceExperiments(experiments, FAKE_EXPERIMENTS_1);
    yield reporter.collectMeasurements();

    let payload = yield reporter.getJSONPayload(true);
    let today = reporter._formatDate(reporter._policy.now());

    Assert.ok(today in payload.data.days, "Day in payload.");
    let day = payload.data.days[today];

    Assert.ok("org.mozilla.experiments.info" in day, "Measurement present.");
    let m = day["org.mozilla.experiments.info"];
    Assert.ok("lastActive" in m, "lastActive field present.");
    Assert.equal(m["lastActive"], "id1", "Last active ID proper.");
  } finally {
    reporter._shutdown();
  }
});
