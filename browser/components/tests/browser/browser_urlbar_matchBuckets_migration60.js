/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Makes sure the browser.urlbar.matchBuckets pref is set correctly starting in
// Firefox 60, nsBrowserGlue UI version 66.

const PREF_NAME = "browser.urlbar.matchBuckets";
const PREF_VALUE_SUGGESTIONS_FIRST = "suggestion:4,general:5";
const PREF_VALUE_GENERAL_FIRST = "general:5,suggestion:Infinity";
const STUDY_NAME = "pref-flip-search-composition-57-release-1413565";

ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);


// Migrates without doing anything else.  The pref should be set to show history
// first.
add_task(async function migrate() {
  await sanityCheckInitialState();

  // Trigger migration.  The pref is cleared initially, so after migration it
  // should be set on the user branch to show history first.
  await promiseMigration();
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""),
               PREF_VALUE_GENERAL_FIRST,
               "Pref should be set, general first");
  Assert.ok(Services.prefs.prefHasUserValue(PREF_NAME),
            "Pref should be set on user branch");

  Services.prefs.clearUserPref(PREF_NAME);
});


// Sets the pref to a value on the user branch and migrates.  The pref shouldn't
// change.
add_task(async function setUserPrefAndMigrate() {
  await sanityCheckInitialState();

  // Set a value for the pref on the user branch.
  let userValue = "userTest:10";
  Services.prefs.setCharPref(PREF_NAME, userValue);

  // Trigger migration.  The pref should be preserved.
  await promiseMigration();
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""),
               userValue,
               "Pref should remain same");
  Assert.ok(Services.prefs.prefHasUserValue(PREF_NAME),
            "Pref should remain on user branch");

  Services.prefs.clearUserPref(PREF_NAME);
});


// Sets the pref to a value on the default branch and migrates.  The pref
// shouldn't change.
add_task(async function setDefaultPrefAndMigrate() {
  await sanityCheckInitialState();

  // Set a value for the pref on the default branch.
  let defaultValue = "defaultTest:10";
  Services.prefs.getDefaultBranch(PREF_NAME).setCharPref("", defaultValue);

  // Trigger migration.  The pref should be preserved.
  await promiseMigration();
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""),
               defaultValue,
               "Pref should remain same");
  Assert.ok(!Services.prefs.prefHasUserValue(PREF_NAME),
            "Pref should remain on default branch");

  Services.prefs.deleteBranch(PREF_NAME);
});


// Installs the study using the pref that the overwhelming majority of users
// will see ("ratio": 97, "value": "suggestion:4,general:5") and migrates.  The
// study should be stopped and the pref should remain cleared.
add_task(async function installStudyAndMigrate() {
  await sanityCheckInitialState();

  // Normandy can't unset the pref if it didn't already have a value, so give it
  // a value that will be treated as empty by the migration.
  Services.prefs.getDefaultBranch(PREF_NAME).setCharPref("", "");

  // Install the study.  It should set the pref.
  await PreferenceExperiments.start(newExperimentOpts());
  Assert.ok(await PreferenceExperiments.has(STUDY_NAME),
            "Study installed");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""),
               PREF_VALUE_SUGGESTIONS_FIRST,
               "Pref should be set by study");

  // Trigger migration.  The study should be stopped, and the pref should be
  // cleared since it's the default value.
  await promiseMigration();
  Assert.ok(!(await getNonExpiredExperiment()), "Study stopped");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Pref should be cleared");

  await PreferenceExperiments.clearAllExperimentStorage();
});


// Installs the study using the pref that the overwhelming majority of users
// will see ("ratio": 97, "value": "suggestion:4,general:5"), except that the
// pref has unnecessary spaces in it, and then migrates.  The study should be
// stopped and the pref should remain cleared.  i.e., the migration code should
// parse the pref value and compare the resulting buckets instead of comparing
// strings directly.
add_task(async function installStudyPrefWithSpacesAndMigrate() {
  await sanityCheckInitialState();

  // Install the study.  It should set the pref.
  const preferenceValue = " suggestion : 4, general : 5 ";
  const experiment = newExperimentOpts({
    preferences: {
      [PREF_NAME]: {
        preferenceValue,
      },
    },
  });
  await PreferenceExperiments.start(experiment);
  Assert.ok(await PreferenceExperiments.has(STUDY_NAME),
            "Study installed");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), preferenceValue,
               "Pref should be set by study");

  // Trigger migration.  The study should be stopped, and the pref should be
  // cleared since it's the default value.
  await promiseMigration();
  Assert.ok(!(await getNonExpiredExperiment()), "Study stopped");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Pref should be cleared");

  await PreferenceExperiments.clearAllExperimentStorage();
});


// Installs the study using a pref that a tiny number of users will see
// ("ratio": 1, "value": "general:3,suggestion:6") and migrates.  The study
// should be stopped and the pref should be preserved since it's not the new
// default.
add_task(async function installStudyMinorityPrefAndMigrate() {
  await sanityCheckInitialState();

  // Install the study.  It should set the pref.
  let preferenceValue = "general:3,suggestion:6";
  const experiment = newExperimentOpts({
    preferences: {
      [PREF_NAME]: {
        preferenceValue,
      },
    },
  });
  await PreferenceExperiments.start(experiment);
  Assert.ok(await PreferenceExperiments.has(STUDY_NAME),
            "Study installed");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), preferenceValue,
               "Pref should be set by study");

  // Trigger migration.  The study should be stopped, and the pref should remain
  // the same since it's a non-default value.  It should be set on the user
  // branch.
  await promiseMigration();
  Assert.ok(!(await getNonExpiredExperiment()), "Study stopped");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""),
               preferenceValue,
               "Pref should remain the same");
  Assert.ok(Services.prefs.prefHasUserValue(PREF_NAME),
            "Pref should be set on user branch");

  await PreferenceExperiments.clearAllExperimentStorage();
  Services.prefs.clearUserPref(PREF_NAME);
});


// Sets the pref to some value on the default branch, installs the study, and
// migrates.  The study should be stopped and the original pref value should be
// restored.
add_task(async function setDefaultPrefInstallStudyAndMigrate() {
  await sanityCheckInitialState();

  // First, set the pref to some value on the default branch.  (If the pref is
  // set on the user branch, starting the study actually throws because the
  // study's branch is the default branch.)
  let defaultValue = "test:10";
  Services.prefs.getDefaultBranch(PREF_NAME).setCharPref("", defaultValue);

  // Install the study.  It should set the pref.
  await PreferenceExperiments.start(newExperimentOpts());
  Assert.ok(await PreferenceExperiments.has(STUDY_NAME),
            "Study installed");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""),
               PREF_VALUE_SUGGESTIONS_FIRST,
               "Pref should be set by study");

  // Trigger migration.  The study should be stopped, and the pref should be
  // restored to the value set above.
  await promiseMigration();
  Assert.ok(!(await getNonExpiredExperiment()), "Study stopped");
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), defaultValue,
               "Pref should be restored to user value");

  await PreferenceExperiments.clearAllExperimentStorage();
  Services.prefs.deleteBranch(PREF_NAME);
});


async function sanityCheckInitialState() {
  Assert.equal(Services.prefs.getCharPref(PREF_NAME, ""), "",
               "Pref should be cleared initially");
  Assert.ok(!(await PreferenceExperiments.has(STUDY_NAME)),
            "Study should not be installed initially");
}

function promiseMigration() {
  let topic = "browser-glue-test";
  let donePromise = TestUtils.topicObserved(topic, (subj, data) => {
    return "migrateMatchBucketsPrefForUI66-done" == data;
  });
  Cc["@mozilla.org/browser/browserglue;1"]
    .getService(Ci.nsIObserver)
    .observe(null, topic, "migrateMatchBucketsPrefForUI66");
  return donePromise;
}

function newExperimentOpts(opts = {}) {
  const defaultPref = {
    [PREF_NAME]: {},
  };
  const defaultPrefInfo = {
    preferenceValue: PREF_VALUE_SUGGESTIONS_FIRST,
    preferenceBranchType: "default",
    preferenceType: "string",
  };
  const preferences = {};
  for (const [prefName, prefInfo] of Object.entries(opts.preferences || defaultPref)) {
    preferences[prefName] = { ...defaultPrefInfo, ...prefInfo };
  }

  return Object.assign({
    name: STUDY_NAME,
    actionName: "SomeAction",
    branch: "branch",
  }, opts, {
    preferences,
  });
}

async function getNonExpiredExperiment() {
  try {
    let exp = await PreferenceExperiments.get(STUDY_NAME);
    if (exp.expired) {
      return null;
    }
  } catch (ex) {}
  return null;
}
