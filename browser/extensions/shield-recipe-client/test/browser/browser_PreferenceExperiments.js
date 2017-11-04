"use strict";

Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
Cu.import("resource://shield-recipe-client/lib/PreferenceExperiments.jsm", this);

// Save ourselves some typing
const {withMockExperiments} = PreferenceExperiments;
const DefaultPreferences = new Preferences({defaultBranch: true});
const startupPrefs = "extensions.shield-recipe-client.startupExperimentPrefs";

function experimentFactory(attrs) {
  return Object.assign({
    name: "fakename",
    branch: "fakebranch",
    expired: false,
    lastSeen: new Date().toJSON(),
    preferenceName: "fake.preference",
    preferenceValue: "falkevalue",
    preferenceType: "string",
    previousPreferenceValue: "oldfakevalue",
    preferenceBranchType: "default",
  }, attrs);
}

// clearAllExperimentStorage
add_task(withMockExperiments(async function(experiments) {
  experiments.test = experimentFactory({name: "test"});
  ok(await PreferenceExperiments.has("test"), "Mock experiment is detected.");
  await PreferenceExperiments.clearAllExperimentStorage();
  ok(
    !(await PreferenceExperiments.has("test")),
    "clearAllExperimentStorage removed all stored experiments",
  );
}));

// start should throw if an experiment with the given name already exists
add_task(withMockExperiments(async function(experiments) {
  experiments.test = experimentFactory({name: "test"});
  await Assert.rejects(
    PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "value",
      preferenceType: "string",
      preferenceBranchType: "default",
    }),
    "start threw an error due to a conflicting experiment name",
  );
}));

// start should throw if an experiment for the given preference is active
add_task(withMockExperiments(async function(experiments) {
  experiments.test = experimentFactory({name: "test", preferenceName: "fake.preference"});
  await Assert.rejects(
    PreferenceExperiments.start({
      name: "different",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "value",
      preferenceType: "string",
      preferenceBranchType: "default",
    }),
    "start threw an error due to an active experiment for the given preference",
  );
}));

// start should throw if an invalid preferenceBranchType is given
add_task(withMockExperiments(async function() {
  await Assert.rejects(
    PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "value",
      preferenceType: "string",
      preferenceBranchType: "invalid",
    }),
    "start threw an error due to an invalid preference branch type",
  );
}));

// start should save experiment data, modify the preference, and register a
// watcher.
decorate_task(
  withMockExperiments,
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  async function testStart(experiments, mockPreferences, startObserverStub) {
    mockPreferences.set("fake.preference", "oldvalue", "default");
    mockPreferences.set("fake.preference", "uservalue", "user");

    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "newvalue",
      preferenceBranchType: "default",
      preferenceType: "string",
    });
    ok("test" in experiments, "start saved the experiment");
    ok(
      startObserverStub.calledWith("test", "fake.preference", "string", "newvalue"),
      "start registered an observer",
    );

    const expectedExperiment = {
      name: "test",
      branch: "branch",
      expired: false,
      preferenceName: "fake.preference",
      preferenceValue: "newvalue",
      preferenceType: "string",
      previousPreferenceValue: "oldvalue",
      preferenceBranchType: "default",
    };
    const experiment = {};
    Object.keys(expectedExperiment).forEach(key => experiment[key] = experiments.test[key]);
    Assert.deepEqual(experiment, expectedExperiment, "start saved the experiment");

    is(
      DefaultPreferences.get("fake.preference"),
      "newvalue",
      "start modified the default preference",
    );
    is(
      Preferences.get("fake.preference"),
      "uservalue",
      "start did not modify the user preference",
    );
    is(
      Preferences.get(`${startupPrefs}.fake.preference`),
      "newvalue",
      "start saved the experiment value to the startup prefs tree",
    );
  },
);

// start should modify the user preference for the user branch type
add_task(withMockExperiments(withMockPreferences(async function(experiments, mockPreferences) {
  const startObserver = sinon.stub(PreferenceExperiments, "startObserver");
  mockPreferences.set("fake.preference", "oldvalue", "user");
  mockPreferences.set("fake.preference", "olddefaultvalue", "default");

  await PreferenceExperiments.start({
    name: "test",
    branch: "branch",
    preferenceName: "fake.preference",
    preferenceValue: "newvalue",
    preferenceType: "string",
    preferenceBranchType: "user",
  });
  ok(
    startObserver.calledWith("test", "fake.preference", "string", "newvalue"),
    "start registered an observer",
  );

  const expectedExperiment = {
    name: "test",
    branch: "branch",
    expired: false,
    preferenceName: "fake.preference",
    preferenceValue: "newvalue",
    preferenceType: "string",
    previousPreferenceValue: "oldvalue",
    preferenceBranchType: "user",
  };

  const experiment = {};
  Object.keys(expectedExperiment).forEach(key => experiment[key] = experiments.test[key]);
  Assert.deepEqual(experiment, expectedExperiment, "start saved the experiment");

  Assert.notEqual(
    DefaultPreferences.get("fake.preference"),
    "newvalue",
    "start did not modify the default preference",
  );
  is(Preferences.get("fake.preference"), "newvalue", "start modified the user preference");

  startObserver.restore();
})));

// start should detect if a new preference value type matches the previous value type
add_task(withMockPreferences(async function(mockPreferences) {
  mockPreferences.set("fake.type_preference", "oldvalue");

  await Assert.rejects(
    PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.type_preference",
      preferenceBranchType: "user",
      preferenceValue: 12345,
      preferenceType: "integer",
    }),
    "start threw error for incompatible preference type"
  );
}));


// startObserver should throw if an observer for the experiment is already
// active.
add_task(withMockExperiments(async function() {
  PreferenceExperiments.startObserver("test", "fake.preference", "string", "newvalue");
  Assert.throws(
    () => PreferenceExperiments.startObserver("test", "another.fake", "string", "othervalue"),
    "startObserver threw due to a conflicting active observer",
  );
  PreferenceExperiments.stopAllObservers();
}));

// startObserver should register an observer that calls stop when a preference
// changes from its experimental value.
add_task(withMockExperiments(withMockPreferences(async function(mockExperiments, mockPreferences) {
  const tests = [
    ["string", "startvalue", "experimentvalue", "newvalue"],
    ["boolean", false, true, false],
    ["integer", 1, 2, 42],
  ];

  for (const [type, startvalue, experimentvalue, newvalue] of tests) {
    const stop = sinon.stub(PreferenceExperiments, "stop");
    mockPreferences.set("fake.preference" + type, startvalue);

    // NOTE: startObserver does not modify the pref
    PreferenceExperiments.startObserver("test" + type, "fake.preference" + type, type, experimentvalue);

    // Setting it to the experimental value should not trigger the call.
    Preferences.set("fake.preference" + type, experimentvalue);
    ok(!stop.called, "Changing to the experimental pref value did not trigger the observer");

    // Setting it to something different should trigger the call.
    Preferences.set("fake.preference" + type, newvalue);
    ok(stop.called, "Changing to a different value triggered the observer");

    PreferenceExperiments.stopAllObservers();
    stop.restore();
  }
})));

add_task(withMockExperiments(async function testHasObserver() {
  PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentValue");

  ok(await PreferenceExperiments.hasObserver("test"), "hasObserver detects active observers");
  ok(
    !(await PreferenceExperiments.hasObserver("missing")),
    "hasObserver doesn't detect inactive observers",
  );

  PreferenceExperiments.stopAllObservers();
}));

// stopObserver should throw if there is no observer active for it to stop.
add_task(withMockExperiments(async function() {
  Assert.throws(
    () => PreferenceExperiments.stopObserver("neveractive", "another.fake", "othervalue"),
    "stopObserver threw because there was not matching active observer",
  );
}));

// stopObserver should cancel an active observer.
add_task(withMockExperiments(withMockPreferences(async function(mockExperiments, mockPreferences) {
  const stop = sinon.stub(PreferenceExperiments, "stop");
  mockPreferences.set("fake.preference", "startvalue");

  PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
  PreferenceExperiments.stopObserver("test");

  // Setting the preference now that the observer is stopped should not call
  // stop.
  Preferences.set("fake.preference", "newvalue");
  ok(!stop.called, "stopObserver successfully removed the observer");

  // Now that the observer is stopped, start should be able to start a new one
  // without throwing.
  try {
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
  } catch (err) {
    ok(false, "startObserver did not throw an error for an observer that was already stopped");
  }

  PreferenceExperiments.stopAllObservers();
  stop.restore();
})));

// stopAllObservers
add_task(withMockExperiments(withMockPreferences(async function(mockExperiments, mockPreferences) {
  const stop = sinon.stub(PreferenceExperiments, "stop");
  mockPreferences.set("fake.preference", "startvalue");
  mockPreferences.set("other.fake.preference", "startvalue");

  PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
  PreferenceExperiments.startObserver("test2", "other.fake.preference", "string", "experimentvalue");
  PreferenceExperiments.stopAllObservers();

  // Setting the preference now that the observers are stopped should not call
  // stop.
  Preferences.set("fake.preference", "newvalue");
  Preferences.set("other.fake.preference", "newvalue");
  ok(!stop.called, "stopAllObservers successfully removed all observers");

  // Now that the observers are stopped, start should be able to start new
  // observers without throwing.
  try {
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");
    PreferenceExperiments.startObserver("test2", "other.fake.preference", "string", "experimentvalue");
  } catch (err) {
    ok(false, "startObserver did not throw an error for an observer that was already stopped");
  }

  PreferenceExperiments.stopAllObservers();
  stop.restore();
})));

// markLastSeen should throw if it can't find a matching experiment
add_task(withMockExperiments(async function() {
  await Assert.rejects(
    PreferenceExperiments.markLastSeen("neveractive"),
    "markLastSeen threw because there was not a matching experiment",
  );
}));

// markLastSeen should update the lastSeen date
add_task(withMockExperiments(async function(experiments) {
  const oldDate = new Date(1988, 10, 1).toJSON();
  experiments.test = experimentFactory({name: "test", lastSeen: oldDate});
  await PreferenceExperiments.markLastSeen("test");
  Assert.notEqual(
    experiments.test.lastSeen,
    oldDate,
    "markLastSeen updated the experiment lastSeen date",
  );
}));

// stop should throw if an experiment with the given name doesn't exist
add_task(withMockExperiments(async function() {
  await Assert.rejects(
    PreferenceExperiments.stop("test"),
    "stop threw an error because there are no experiments with the given name",
  );
}));

// stop should throw if the experiment is already expired
add_task(withMockExperiments(async function(experiments) {
  experiments.test = experimentFactory({name: "test", expired: true});
  await Assert.rejects(
    PreferenceExperiments.stop("test"),
    "stop threw an error because the experiment was already expired",
  );
}));

// stop should mark the experiment as expired, stop its observer, and revert the
// preference value.
decorate_task(
  withMockExperiments,
  withMockPreferences,
  withSpy(PreferenceExperiments, "stopObserver"),
  async function testStop(experiments, mockPreferences, stopObserverSpy) {
    mockPreferences.set(`${startupPrefs}.fake.preference`, "experimentvalue", "user");
    mockPreferences.set("fake.preference", "experimentvalue", "default");
    experiments.test = experimentFactory({
      name: "test",
      expired: false,
      preferenceName: "fake.preference",
      preferenceValue: "experimentvalue",
      preferenceType: "string",
      previousPreferenceValue: "oldvalue",
      preferenceBranchType: "default",
    });
    PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");

    await PreferenceExperiments.stop("test");
    ok(stopObserverSpy.calledWith("test"), "stop removed an observer");
    is(experiments.test.expired, true, "stop marked the experiment as expired");
    is(
      DefaultPreferences.get("fake.preference"),
      "oldvalue",
      "stop reverted the preference to its previous value",
    );
    ok(
      !Services.prefs.prefHasUserValue(`${startupPrefs}.fake.preference`),
      "stop cleared the startup preference for fake.preference.",
    );

    PreferenceExperiments.stopAllObservers();
  },
);

// stop should also support user pref experiments
add_task(withMockExperiments(withMockPreferences(async function(experiments, mockPreferences) {
  const stopObserver = sinon.stub(PreferenceExperiments, "stopObserver");
  const hasObserver = sinon.stub(PreferenceExperiments, "hasObserver");
  hasObserver.returns(true);

  mockPreferences.set("fake.preference", "experimentvalue", "user");
  experiments.test = experimentFactory({
    name: "test",
    expired: false,
    preferenceName: "fake.preference",
    preferenceValue: "experimentvalue",
    preferenceType: "string",
    previousPreferenceValue: "oldvalue",
    preferenceBranchType: "user",
  });
  PreferenceExperiments.startObserver("test", "fake.preference", "string", "experimentvalue");

  await PreferenceExperiments.stop("test");
  ok(stopObserver.calledWith("test"), "stop removed an observer");
  is(experiments.test.expired, true, "stop marked the experiment as expired");
  is(
    Preferences.get("fake.preference"),
    "oldvalue",
    "stop reverted the preference to its previous value",
  );
  stopObserver.restore();
  PreferenceExperiments.stopAllObservers();
})));

// stop should remove a preference that had no value prior to an experiment for user prefs
add_task(withMockExperiments(withMockPreferences(async function(experiments, mockPreferences) {
  const stopObserver = sinon.stub(PreferenceExperiments, "stopObserver");
  mockPreferences.set("fake.preference", "experimentvalue", "user");
  experiments.test = experimentFactory({
    name: "test",
    expired: false,
    preferenceName: "fake.preference",
    preferenceValue: "experimentvalue",
    preferenceType: "string",
    previousPreferenceValue: null,
    preferenceBranchType: "user",
  });

  await PreferenceExperiments.stop("test");
  ok(
    !Preferences.isSet("fake.preference"),
    "stop removed the preference that had no value prior to the experiment",
  );

  stopObserver.restore();
})));

// stop should not modify a preference if resetValue is false
add_task(withMockExperiments(withMockPreferences(async function(experiments, mockPreferences) {
  const stopObserver = sinon.stub(PreferenceExperiments, "stopObserver");
  mockPreferences.set("fake.preference", "customvalue", "default");
  experiments.test = experimentFactory({
    name: "test",
    expired: false,
    preferenceName: "fake.preference",
    preferenceValue: "experimentvalue",
    preferenceType: "string",
    previousPreferenceValue: "oldvalue",
    peferenceBranchType: "default",
  });

  await PreferenceExperiments.stop("test", false);
  is(
    DefaultPreferences.get("fake.preference"),
    "customvalue",
    "stop did not modify the preference",
  );

  stopObserver.restore();
})));

// get should throw if no experiment exists with the given name
add_task(withMockExperiments(async function() {
  await Assert.rejects(
    PreferenceExperiments.get("neverexisted"),
    "get rejects if no experiment with the given name is found",
  );
}));

// get
add_task(withMockExperiments(async function(experiments) {
  const experiment = experimentFactory({name: "test"});
  experiments.test = experiment;

  const fetchedExperiment = await PreferenceExperiments.get("test");
  Assert.deepEqual(fetchedExperiment, experiment, "get fetches the correct experiment");

  // Modifying the fetched experiment must not edit the data source.
  fetchedExperiment.name = "othername";
  is(experiments.test.name, "test", "get returns a copy of the experiment");
}));

add_task(withMockExperiments(async function testGetAll(experiments) {
  const experiment1 = experimentFactory({name: "experiment1"});
  const experiment2 = experimentFactory({name: "experiment2", disabled: true});
  experiments.experiment1 = experiment1;
  experiments.experiment2 = experiment2;

  const fetchedExperiments = await PreferenceExperiments.getAll();
  is(fetchedExperiments.length, 2, "getAll returns a list of all stored experiments");
  Assert.deepEqual(
    fetchedExperiments.find(e => e.name === "experiment1"),
    experiment1,
    "getAll returns a list with the correct experiments",
  );
  const fetchedExperiment2 = fetchedExperiments.find(e => e.name === "experiment2");
  Assert.deepEqual(
    fetchedExperiment2,
    experiment2,
    "getAll returns a list with the correct experiments, including disabled ones",
  );

  fetchedExperiment2.name = "othername";
  is(experiment2.name, "experiment2", "getAll returns copies of the experiments");
}));

add_task(withMockExperiments(withMockPreferences(async function testGetAllActive(experiments) {
  experiments.active = experimentFactory({
    name: "active",
    expired: false,
  });
  experiments.inactive = experimentFactory({
    name: "inactive",
    expired: true,
  });

  const activeExperiments = await PreferenceExperiments.getAllActive();
  Assert.deepEqual(
    activeExperiments,
    [experiments.active],
    "getAllActive only returns active experiments",
  );

  activeExperiments[0].name = "newfakename";
  Assert.notEqual(
    experiments.active.name,
    "newfakename",
    "getAllActive returns copies of stored experiments",
  );
})));

// has
add_task(withMockExperiments(async function(experiments) {
  experiments.test = experimentFactory({name: "test"});
  ok(await PreferenceExperiments.has("test"), "has returned true for a stored experiment");
  ok(!(await PreferenceExperiments.has("missing")), "has returned false for a missing experiment");
}));

// init should register telemetry experiments
decorate_task(
  withMockExperiments,
  withMockPreferences,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(PreferenceExperiments, "startObserver"),
  async function testInit(experiments, mockPreferences, setActiveStub, startObserverStub) {
    mockPreferences.set("fake.pref", "experiment value");

    experiments.test = experimentFactory({
      name: "test",
      branch: "branch",
      preferenceName: "fake.pref",
      preferenceValue: "experiment value",
      expired: false,
      preferenceBranchType: "default",
    });

    await PreferenceExperiments.init();
    ok(
      setActiveStub.calledWith("test", "branch", {type: "normandy-preference-experiment"}),
      "Experiment is registered by init",
    );
  },
);

// starting and stopping experiments should register in telemetry
decorate_task(
  withMockExperiments,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  withStub(TelemetryEnvironment, "setExperimentInactive"),
  async function testInitTelemetry(experiments, setActiveStub, setInactiveStub) {
    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: "fake.preference",
      preferenceValue: "value",
      preferenceType: "string",
      preferenceBranchType: "default",
    });

    ok(
      setActiveStub.calledWith("test", "branch", {type: "normandy-preference-experiment"}),
      "Experiment is registerd by start()",
    );
    await PreferenceExperiments.stop("test");
    ok(setInactiveStub.calledWith("test", "branch"), "Experiment is unregisterd by stop()");
  },
);

// Experiments shouldn't be recorded by init() in telemetry if they are expired
decorate_task(
  withMockExperiments,
  withStub(TelemetryEnvironment, "setExperimentActive"),
  async function testInitTelemetryExpired(experiments, setActiveStub) {
    experiments.experiment1 = experimentFactory({name: "expired", branch: "branch", expired: true});
    await PreferenceExperiments.init();
    ok(!setActiveStub.called, "Expired experiment is not registered by init");
  },
);

// Experiments should end if the preference has been changed when init() is called
add_task(withMockExperiments(withMockPreferences(async function testInitChanges(experiments, mockPreferences) {
  const stopStub = sinon.stub(PreferenceExperiments, "stop");
  mockPreferences.set("fake.preference", "experiment value", "default");
  experiments.test = experimentFactory({
    name: "test",
    preferenceName: "fake.preference",
    preferenceValue: "experiment value",
  });
  mockPreferences.set("fake.preference", "changed value");
  await PreferenceExperiments.init();
  ok(stopStub.calledWith("test"), "Experiment is stopped because value changed");
  ok(Preferences.get("fake.preference"), "changed value", "Preference value was not changed");
  stopStub.restore();
})));


// init should register an observer for experiments
add_task(withMockExperiments(withMockPreferences(async function testInitRegistersObserver(experiments, mockPreferences) {
  const startObserver = sinon.stub(PreferenceExperiments, "startObserver");
  mockPreferences.set("fake.preference", "experiment value", "default");
  experiments.test = experimentFactory({
    name: "test",
    preferenceName: "fake.preference",
    preferenceValue: "experiment value",
  });
  await PreferenceExperiments.init();

  ok(
    startObserver.calledWith("test", "fake.preference", "string", "experiment value"),
    "init registered an observer",
  );

  startObserver.restore();
})));

decorate_task(
  withMockExperiments,
  async function testSaveStartupPrefs(experiments) {
    const experimentPrefs = {
      char: "string",
      int: 2,
      bool: true,
    };

    for (const [key, value] of Object.entries(experimentPrefs)) {
      experiments[key] = experimentFactory({
        preferenceName: `fake.${key}`,
        preferenceValue: value,
      });
    }

    Services.prefs.deleteBranch(startupPrefs);
    Services.prefs.setBoolPref(`${startupPrefs}.fake.old`, true);
    await PreferenceExperiments.saveStartupPrefs();

    ok(
      Services.prefs.getBoolPref(`${startupPrefs}.fake.bool`),
      "The startup value for fake.bool was saved.",
    );
    is(
      Services.prefs.getCharPref(`${startupPrefs}.fake.char`),
      "string",
      "The startup value for fake.char was saved.",
    );
    is(
      Services.prefs.getIntPref(`${startupPrefs}.fake.int`),
      2,
      "The startup value for fake.int was saved.",
    );
    ok(
      !Services.prefs.prefHasUserValue(`${startupPrefs}.fake.old`),
      "saveStartupPrefs deleted old startup pref values.",
    );
  },
);

decorate_task(
  withMockExperiments,
  async function testSaveStartupPrefsError(experiments) {
    experiments.test = experimentFactory({
      preferenceName: "fake.invalidValue",
      preferenceValue: new Date(),
    });

    await Assert.rejects(
      PreferenceExperiments.saveStartupPrefs(),
      "saveStartupPrefs throws if an experiment has an invalid preference value type",
    );
  },
);

// test that default branch prefs restore to the right value if the default pref changes
decorate_task(
  withMockExperiments,
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stopObserver"),
  async function testDefaultBranchStop(mockExperiments, mockPreferences, stopObserverStub) {
    const prefName = "fake.preference";
    mockPreferences.set(prefName, "old version's value", "default");

    // start an experiment
    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: prefName,
      preferenceValue: "experiment value",
      preferenceBranchType: "default",
      preferenceType: "string",
    });

    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Starting an experiment should change the pref",
    );

    // Now pretend that firefox has updated and restarted to a version
    // where the built-default value of fake.preference is something
    // else. Bootstrap has run and changed the pref to the
    // experimental value, and produced the call to
    // recordOriginalValues below.
    PreferenceExperiments.recordOriginalValues({ [prefName]: "new version's value" });
    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Recording original values shouldn't affect the preference."
    );

    // Now stop the experiment. It should revert to the new version's default, not the old.
    await PreferenceExperiments.stop("test");
    is(
      Services.prefs.getCharPref(prefName),
      "new version's value",
      "Preference should revert to new default",
    );
  },
);

// test that default branch prefs restore to the right value if the preference is removed
decorate_task(
  withMockExperiments,
  withMockPreferences,
  withStub(PreferenceExperiments, "startObserver"),
  withStub(PreferenceExperiments, "stopObserver"),
  async function testDefaultBranchStop(mockExperiments, mockPreferences, stopObserverStub) {
    const prefName = "fake.preference";
    mockPreferences.set(prefName, "old version's value", "default");

    // start an experiment
    await PreferenceExperiments.start({
      name: "test",
      branch: "branch",
      preferenceName: prefName,
      preferenceValue: "experiment value",
      preferenceBranchType: "default",
      preferenceType: "string",
    });

    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Starting an experiment should change the pref",
    );

    // Now pretend that firefox has updated and restarted to a version
    // where fake.preference has been removed in the default pref set.
    // Bootstrap has run and changed the pref to the experimental
    // value, and produced the call to recordOriginalValues below.
    PreferenceExperiments.recordOriginalValues({ [prefName]: null });
    is(
      Services.prefs.getCharPref(prefName),
      "experiment value",
      "Recording original values shouldn't affect the preference."
    );

    // Now stop the experiment. It should remove the preference
    await PreferenceExperiments.stop("test");
    is(
      Services.prefs.getCharPref(prefName, "DEFAULT"),
      "DEFAULT",
      "Preference should be absent",
    );
  },
);
