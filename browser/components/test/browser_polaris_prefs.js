const POLARIS_ENABLED = "browser.polaris.enabled";
const PREF_DNT = "privacy.donottrackheader.enabled";
const PREF_TP = "privacy.trackingprotection.enabled";
const PREF_TPUI = "privacy.trackingprotection.ui.enabled";

let prefs = [PREF_DNT, PREF_TP, PREF_TPUI];

function spinEventLoop() {
  return new Promise((resolve) => executeSoon(resolve));
};

// Spin event loop before checking so that polaris pref observer can set
// dependent prefs.
function* assertPref(pref, enabled) {
  yield spinEventLoop();
  let prefEnabled = Services.prefs.getBoolPref(pref);
  Assert.equal(prefEnabled, enabled, "Checking state of pref " + pref + ".");
};

function* testPrefs(test) {
  for (let pref of prefs) {
    yield test(pref);
  }
}

function isNightly() {
  return Services.appinfo.version.includes("a1");
}

add_task(function* test_default_values() {
  if (!isNightly()) {
    ok(true, "Skipping test, not Nightly")
    return;
  }
  Assert.ok(!Services.prefs.getBoolPref(POLARIS_ENABLED), POLARIS_ENABLED + " is disabled by default.");
  Assert.ok(!Services.prefs.getBoolPref(PREF_TPUI), PREF_TPUI + "is disabled by default.");
});

add_task(function* test_changing_pref_changes_tracking() {
  if (!isNightly()) {
    ok(true, "Skipping test, not Nightly")
    return;
  }

  // Register a cleanup function for all the prefs affected by this entire test file.
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(POLARIS_ENABLED);
    for (let pref of prefs) {
      Services.prefs.clearUserPref(pref);
    }
  });

  function* testPref(pref) {
    Services.prefs.setBoolPref(POLARIS_ENABLED, true);
    yield assertPref(pref, true);
    Services.prefs.setBoolPref(POLARIS_ENABLED, false);
    // We don't clear the DNT pref if Polaris is disabled.
    if (pref != PREF_DNT) {
      yield assertPref(pref, false);
    } else {
      yield assertPref(pref, true);
    }
    Services.prefs.setBoolPref(POLARIS_ENABLED, true);
    yield assertPref(pref, true);
  }
  yield testPrefs(testPref);
});

add_task(function* test_prefs_can_be_changed_individually() {
  if (!isNightly()) {
    ok(true, "Skipping test, not Nightly")
    return;
  }
  function* testPref(pref) {
    Services.prefs.setBoolPref(POLARIS_ENABLED, true);
    yield assertPref(pref, true);
    Services.prefs.setBoolPref(pref, false);
    yield assertPref(pref, false);
    yield assertPref(POLARIS_ENABLED, true);

    Services.prefs.setBoolPref(POLARIS_ENABLED, false);
    yield assertPref(pref, false);

    Services.prefs.setBoolPref(pref, true);
    yield assertPref(pref, true);
    yield assertPref(POLARIS_ENABLED, false);
  }
  yield testPrefs(testPref);
});
