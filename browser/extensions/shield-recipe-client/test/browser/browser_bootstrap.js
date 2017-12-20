"use strict";

Cu.import("resource://shield-recipe-client/lib/ShieldRecipeClient.jsm", this);
Cu.import("resource://shield-recipe-client/lib/PreferenceExperiments.jsm", this);

// We can't import bootstrap.js directly since it isn't in the jar manifest, but
// we can use Addon.getResourceURI to get a path to the file and import using
// that instead.
Cu.import("resource://gre/modules/AddonManager.jsm", this);
const bootstrapPromise = AddonManager.getAddonByID("shield-recipe-client@mozilla.org").then(addon => {
  const bootstrapUri = addon.getResourceURI("bootstrap.js");
  const {Bootstrap} = Cu.import(bootstrapUri.spec, {});
  return Bootstrap;
});

// Use a decorator to get around getAddonByID being async.
function withBootstrap(testFunction) {
  return async function wrappedTestFunction(...args) {
    const Bootstrap = await bootstrapPromise;
    return testFunction(...args, Bootstrap);
  };
}

const initPref1 = "test.initShieldPrefs1";
const initPref2 = "test.initShieldPrefs2";
const initPref3 = "test.initShieldPrefs3";

const experimentPref1 = "test.initExperimentPrefs1";
const experimentPref2 = "test.initExperimentPrefs2";
const experimentPref3 = "test.initExperimentPrefs3";
const experimentPref4 = "test.initExperimentPrefs4";

decorate_task(
  withBootstrap,
  async function testInitShieldPrefs(Bootstrap) {
    const defaultBranch = Services.prefs.getDefaultBranch("");

    const prefDefaults = {
      [initPref1]: true,
      [initPref2]: 2,
      [initPref3]: "string",
    };

    for (const pref of Object.keys(prefDefaults)) {
      is(
        defaultBranch.getPrefType(pref),
        defaultBranch.PREF_INVALID,
        `Pref ${pref} don't exist before being initialized.`,
      );
    }

    Bootstrap.initShieldPrefs(prefDefaults);

    ok(
      defaultBranch.getBoolPref(initPref1),
      `Pref ${initPref1} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getIntPref(initPref2),
      2,
      `Pref ${initPref2} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getCharPref(initPref3),
      "string",
      `Pref ${initPref3} has a default value after being initialized.`,
    );

    for (const pref of Object.keys(prefDefaults)) {
      ok(
        !defaultBranch.prefHasUserValue(pref),
        `Pref ${pref} doesn't have a user value after being initialized.`,
      );
    }

    defaultBranch.deleteBranch("test.");
  },
);

decorate_task(
  withBootstrap,
  async function testInitShieldPrefsError(Bootstrap) {
    Assert.throws(
      () => Bootstrap.initShieldPrefs({"test.prefTypeError": new Date()}),
      "initShieldPrefs throws when given an invalid type for the pref value.",
    );
  },
);

decorate_task(
  withPrefEnv({
    set: [
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref1}`, true],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref2}`, 2],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref3}`, "string"],
    ],
    clear: [[experimentPref1], [experimentPref2], [experimentPref3]],
  }),
  withBootstrap,
  async function testInitExperimentPrefs(Bootstrap) {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    for (const pref of [experimentPref1, experimentPref2, experimentPref3]) {
      is(
        defaultBranch.getPrefType(pref),
        defaultBranch.PREF_INVALID,
        `Pref ${pref} don't exist before being initialized.`,
      );
    }

    Bootstrap.initExperimentPrefs();

    ok(
      defaultBranch.getBoolPref(experimentPref1),
      `Pref ${experimentPref1} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getIntPref(experimentPref2),
      2,
      `Pref ${experimentPref2} has a default value after being initialized.`,
    );
    is(
      defaultBranch.getCharPref(experimentPref3),
      "string",
      `Pref ${experimentPref3} has a default value after being initialized.`,
    );

    for (const pref of [experimentPref1, experimentPref2, experimentPref3]) {
      ok(
        !defaultBranch.prefHasUserValue(pref),
        `Pref ${pref} doesn't have a user value after being initialized.`,
      );
    }
  },
);

decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.startupExperimentPrefs.test.existingPref", "experiment"],
    ],
  }),
  withBootstrap,
  async function testInitExperimentPrefsExisting(Bootstrap) {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    defaultBranch.setCharPref("test.existingPref", "default");
    Bootstrap.initExperimentPrefs();
    is(
      defaultBranch.getCharPref("test.existingPref"),
      "experiment",
      "initExperimentPrefs overwrites the default values of existing preferences.",
    );
  },
);

decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.startupExperimentPrefs.test.mismatchPref", "experiment"],
    ],
  }),
  withBootstrap,
  async function testInitExperimentPrefsMismatch(Bootstrap) {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    defaultBranch.setIntPref("test.mismatchPref", 2);
    Bootstrap.initExperimentPrefs();
    is(
      defaultBranch.getPrefType("test.mismatchPref"),
      Services.prefs.PREF_INT,
      "initExperimentPrefs skips prefs that don't match the existing default value's type.",
    );
  },
);

decorate_task(
  withBootstrap,
  async function testStartupDelayed(Bootstrap) {
    const finishStartupStub = sinon.stub(Bootstrap, "finishStartup");
    try {
      Bootstrap.startup(undefined, 1); // 1 == APP_STARTUP
      ok(
        !finishStartupStub.called,
        "When started at app startup, do not call ShieldRecipeClient.startup immediately.",
      );

      Bootstrap.observe(null, "sessionstore-windows-restored");
      ok(
        finishStartupStub.called,
        "Once the sessionstore-windows-restored event is observed, call ShieldRecipeClient.startup.",
      );
    } finally {
      finishStartupStub.restore();
    }
  },
);

decorate_task(
  withBootstrap,
  async function testStartupDelayed(Bootstrap) {
    const finishStartupStub = sinon.stub(Bootstrap, "finishStartup");
    try {
      Bootstrap.startup(undefined, 3); // 3 == ADDON_ENABLED
      ok(
        finishStartupStub.called,
        "When the add-on is enabled outside app startup, call ShieldRecipeClient.startup immediately.",
      );
    } finally {
      finishStartupStub.restore();
    }
  },
);

// During startup, preferences that are changed for experiments should
// be record by calling PreferenceExperiments.recordOriginalValues.
decorate_task(
  withPrefEnv({
    set: [
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref1}`, true],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref2}`, 2],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref3}`, "string"],
      [`extensions.shield-recipe-client.startupExperimentPrefs.${experimentPref4}`, "another string"],
    ],
    clear: [
      [experimentPref1],
      [experimentPref2],
      [experimentPref3],
      [experimentPref4],
      ["extensions.shield-recipe-client.startupExperimentPrefs.existingPref"],
    ],
  }),
  withBootstrap,
  withStub(PreferenceExperiments, "recordOriginalValues"),
  async function testInitExperimentPrefs(Bootstrap, recordOriginalValuesStub) {
    const defaultBranch = Services.prefs.getDefaultBranch("");

    defaultBranch.setBoolPref(experimentPref1, false);
    defaultBranch.setIntPref(experimentPref2, 1);
    defaultBranch.setCharPref(experimentPref3, "original string");
    // experimentPref4 is left unset

    Bootstrap.initExperimentPrefs();
    await Bootstrap.finishStartup();

    Assert.deepEqual(
      recordOriginalValuesStub.getCall(0).args,
      [{
        [experimentPref1]: false,
        [experimentPref2]: 1,
        [experimentPref3]: "original string",
        [experimentPref4]: null,  // null because it was not initially set.
      }],
      "finishStartup should record original values of the prefs initExperimentPrefs changed",
    );
  },
);

// Test that startup prefs are handled correctly when there is a value on the user branch but not the default branch.
decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.startupExperimentPrefs.testing.does-not-exist", "foo"],
      ["testing.does-not-exist", "foo"],
    ],
  }),
  withBootstrap,
  withStub(PreferenceExperiments, "recordOriginalValues"),
  async function testInitExperimentPrefsNoDefaultValue(Bootstrap) {
    Bootstrap.initExperimentPrefs();
    ok(true, "initExperimentPrefs should not throw for non-existant prefs");
  },
);
