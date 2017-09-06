"use strict";

Cu.import("resource://shield-recipe-client/lib/ShieldRecipeClient.jsm", this);

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
decorate_task(
  withPrefEnv({
    clear: [[initPref1], [initPref2], [initPref3]],
  }),
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

const experimentPref1 = "test.initExperimentPrefs1";
const experimentPref2 = "test.initExperimentPrefs2";
const experimentPref3 = "test.initExperimentPrefs3";
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
  withStub(ShieldRecipeClient, "startup"),
  async function testStartupDelayed(Bootstrap, startupStub) {
    Bootstrap.startup(undefined, 1); // 1 == APP_STARTUP
    ok(
      !startupStub.called,
      "When started at app startup, do not call ShieldRecipeClient.startup immediately.",
    );

    Bootstrap.observe(null, "sessionstore-windows-restored");
    ok(
      startupStub.called,
      "Once the sessionstore-windows-restored event is observed, call ShieldRecipeClient.startup.",
    );
  },
);

decorate_task(
  withBootstrap,
  withStub(ShieldRecipeClient, "startup"),
  async function testStartupDelayed(Bootstrap, startupStub) {
    Bootstrap.startup(undefined, 3); // 1 == ADDON_ENABLED
    ok(
      startupStub.called,
      "When the add-on is enabled outside app startup, call ShieldRecipeClient.startup immediately.",
    );
  },
);
