/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserGlue",
  "resource:///modules/BrowserGlue.jsm"
);

const ROLLOUT_PREF_PHASE1 =
  "privacy.restrict3rdpartystorage.rollout.enabledByDefault";
const ROLLOUT_PREF_PHASE1_PREFERENCES =
  "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard";
const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const CB_CATEGORY_PREF = "browser.contentblocking.category";

const defaultPrefs = Services.prefs.getDefaultBranch("");
const previousDefaultCB = defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF);

// Avoid timeouts on test-macosx1015-64-qr/opt-test-verify-fis-e10s
requestLongerTimeout(2);

function cleanup() {
  BrowserGlue._defaultCookieBehaviorAtStartup = previousDefaultCB;
  defaultPrefs.setIntPref(COOKIE_BEHAVIOR_PREF, previousDefaultCB);

  Services.prefs.clearUserPref(ROLLOUT_PREF_PHASE1);
  Services.prefs.clearUserPref(ROLLOUT_PREF_PHASE1_PREFERENCES);
  Services.prefs.clearUserPref(CB_CATEGORY_PREF);

  // Same for the tcpByDefault feature probe.
  Services.telemetry.scalarSet(
    "privacy.dfpi_rollout_tcpByDefault_feature",
    false
  );
}

/**
 * Waits for preference to be set and asserts the value.
 * @param {string} pref - Preference key.
 * @param {*} expectedValue - Expected value of the preference.
 * @param {string} message - Assertion message.
 */
async function waitForAndAssertPrefState(pref, expectedValue, message) {
  await TestUtils.waitForPrefChange(pref, value => {
    if (value != expectedValue) {
      return false;
    }
    is(value, expectedValue, message);
    return true;
  });
}

function testTelemetryState(
  expectedValueOptIn,
  expectedValueTCPByDefault,
  message = "Scalars should have correct value"
) {
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "privacy.dfpi_rollout_tcpByDefault_feature",
    expectedValueTCPByDefault,
    message
  );
}

// Copied from browser/components/preferences/tests/head.js
async function openPreferencesViaOpenPreferencesAPI(aPane, aOptions) {
  let finalPaneEvent = Services.prefs.getBoolPref("identity.fxaccounts.enabled")
    ? "sync-pane-loaded"
    : "privacy-pane-loaded";
  let finalPrefPaneLoaded = TestUtils.topicObserved(finalPaneEvent, () => true);
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  openPreferences(aPane, aOptions);
  let newTabBrowser = gBrowser.selectedBrowser;

  if (!newTabBrowser.contentWindow) {
    await BrowserTestUtils.waitForEvent(newTabBrowser, "Initialized", true);
    await BrowserTestUtils.waitForEvent(newTabBrowser.contentWindow, "load");
    await finalPrefPaneLoaded;
  }

  let win = gBrowser.contentWindow;
  let selectedPane = win.history.state;
  if (!aOptions || !aOptions.leaveOpen) {
    gBrowser.removeCurrentTab();
  }
  return { selectedPane };
}

async function testPreferencesSectionVisibility(isVisible, message) {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let etpStandardTCPRolloutBox = gBrowser.contentDocument.getElementById(
    "etpStandardTCPRolloutBox"
  );
  if (message) {
    info(message);
  }
  is(
    BrowserTestUtils.is_visible(etpStandardTCPRolloutBox),
    isVisible,
    `Preferences TCP rollout UI in standard is ${
      isVisible ? " " : "not "
    }visible.`
  );

  gBrowser.removeCurrentTab();
}

function setDefaultCookieBehavior(cookieBehavior) {
  BrowserGlue._defaultCookieBehaviorAtStartup = cookieBehavior;
  defaultPrefs.setIntPref(COOKIE_BEHAVIOR_PREF, cookieBehavior);
}

function waitForNimbusFeatureUpdate(feature) {
  return new Promise(resolve => {
    let callback = () => {
      NimbusFeatures[feature].off(callback);
      resolve();
    };
    NimbusFeatures[feature].onUpdate(callback);
  });
}

/**
 * Tests that enabling the tcpByDefault Nimbus feature changes the default
 * cookie behavior to enable TCP.
 */
add_task(async function test_phase2() {
  // Disable TCP by default.
  setDefaultCookieBehavior(Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    "TCP is disabled by default."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  ok(
    !NimbusFeatures.tcpByDefault.getVariable("enabled"),
    "tcpByDefault Nimbus feature is disabled initially."
  );
  ok(
    !NimbusFeatures.tcpPreferences.getVariable("enabled"),
    "tcpPreferences Nimbus feature is disabled initially."
  );

  await testPreferencesSectionVisibility(
    false,
    "TCP preferences section should not be visible initially."
  );

  testTelemetryState(2, false, "Telemetry should indicate not enrolled.");

  let cookieBehaviorChange = waitForAndAssertPrefState(
    COOKIE_BEHAVIOR_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "Cookie behavior updates to TCP enabled."
  );

  // Enable Nimbus feature for phase 2.
  let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "tcpByDefault",
    value: { enabled: true },
  });

  await cookieBehaviorChange;
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "TCP is enabled by default."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  ok(
    NimbusFeatures.tcpByDefault.getVariable("enabled"),
    "tcpByDefault Nimbus feature is enabled."
  );
  ok(
    !NimbusFeatures.tcpPreferences.getVariable("enabled"),
    "tcpPreferences Nimbus feature is still disabled."
  );

  await testPreferencesSectionVisibility(
    false,
    "Preferences section should still not be visible."
  );

  testTelemetryState(2, true, "Telemetry should indicate phase 2");

  await doEnrollmentCleanup();
  cleanup();
});

/**
 * Tests that phase 2 overrides TCP opt-out choice from phase 1 and that we no
 * longer show the preferences section.
 */
add_task(async function test_phase1_opt_out_to_phase2() {
  // Disable TCP by default.
  setDefaultCookieBehavior(Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    "TCP is disabled by default."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  ok(
    !NimbusFeatures.tcpByDefault.getVariable("enabled"),
    "tcpByDefault Nimbus feature is disabled initially."
  );
  ok(
    !NimbusFeatures.tcpPreferences.getVariable("enabled"),
    "tcpPreferences Nimbus feature is disabled initially."
  );

  await testPreferencesSectionVisibility(
    false,
    "TCP preferences section should not be visible initially."
  );

  testTelemetryState(2, false, "Telemetry should indicate not enrolled.");

  info("Set the phase 1 rollout pref indicating user opt-out state.");
  // This simulates the prefs set when the user opts out via the messaging
  // system modal.
  Services.prefs.setBoolPref(ROLLOUT_PREF_PHASE1, false);
  Services.prefs.setBoolPref(ROLLOUT_PREF_PHASE1_PREFERENCES, true);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    `After opt-out TCP is still disabled by default.`
  );

  testTelemetryState(0, false, "Telemetry indicates opt-out.");

  ok(
    !NimbusFeatures.tcpByDefault.getVariable("enabled"),
    "tcpByDefault Nimbus feature is still disabled."
  );
  ok(
    NimbusFeatures.tcpPreferences.getVariable("enabled"),
    "tcpPreferences Nimbus feature is enabled after-opt-out."
  );

  await testPreferencesSectionVisibility(
    true,
    "Preferences section should be visible after opt-out."
  );

  let cookieBehaviorChange = waitForAndAssertPrefState(
    COOKIE_BEHAVIOR_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "Cookie behavior updates to TCP enabled after tcpByDefault enrollment."
  );

  // Enable Nimbus feature for phase 2.
  let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "tcpByDefault",
    value: { enabled: true },
  });

  await cookieBehaviorChange;
  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "TCP is enabled by default."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  await testPreferencesSectionVisibility(
    false,
    "Preferences section should no longer be visible."
  );

  testTelemetryState(0, true, "Telemetry should indicate phase 2.");

  await doEnrollmentCleanup();
  cleanup();
});

/**
 * Tests that in phase 2 that we no longer show the preferences section if a
 * client opted-in in phase 1.
 */
add_task(async function test_phase1_opt_in_to_phase2() {
  // Disable TCP by default.
  setDefaultCookieBehavior(Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    "TCP is disabled by default."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  ok(
    !NimbusFeatures.tcpByDefault.getVariable("enabled"),
    "tcpByDefault Nimbus feature is disabled initially."
  );
  ok(
    !NimbusFeatures.tcpPreferences.getVariable("enabled"),
    "tcpPreferences Nimbus feature is disabled initially."
  );

  await testPreferencesSectionVisibility(
    false,
    "TCP preferences section should not be visible initially."
  );

  testTelemetryState(2, false, "Telemetry should indicate not enrolled.");

  info("Set the phase 1 rollout pref indicator user opt-in state.");
  // This simulates the prefs set when the user opts-in via the messaging
  // system modal.
  Services.prefs.setBoolPref(ROLLOUT_PREF_PHASE1, true);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    `TCP is enabled default.`
  );

  testTelemetryState(1, false, "Telemetry indicates opt-in.");

  ok(
    !NimbusFeatures.tcpByDefault.getVariable("enabled"),
    "tcpByDefault Nimbus feature is still disabled."
  );
  // ROLLOUT_PREF_PHASE1_PREFERENCES controls the tcpPreferences feature state,
  // it is the fallback pref defined for the feature..
  // ROLLOUT_PREF_PHASE1_PREFERENCES is not set, but the preferences section is
  // shown because the rollout pref is set to true.
  ok(
    !NimbusFeatures.tcpPreferences.getVariable("enabled"),
    "tcpPreferences Nimbus feature is disabled after opt-in."
  );

  await testPreferencesSectionVisibility(
    true,
    "Preferences section should be visible after opt-in."
  );

  let featureUpdatePromise = waitForNimbusFeatureUpdate("tcpByDefault");

  // Enable Nimbus feature for phase 2.
  let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "tcpByDefault",
    value: { enabled: true },
  });

  // We can't await a pref change here since the rollout-pref opt-in has already
  // enabled TCP. Enabling the feature will not change the pref state.
  // Instead, wait for the Nimbus feature update callback.
  await featureUpdatePromise;
  ok(
    NimbusFeatures.tcpByDefault.getVariable("enabled"),
    "tcpByDefault Nimbus feature is now enabled."
  );

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "TCP is still enabled by default."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  await testPreferencesSectionVisibility(
    false,
    "Preferences section should still not be visible."
  );

  testTelemetryState(1, true, "Telemetry should indicate phase 2.");

  info(
    "Changing opt-in choice after phase 2 enrollment should not disable TCP."
  );
  Services.prefs.setBoolPref(ROLLOUT_PREF_PHASE1, false);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "TCP is still enabled by default after opt-out."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  Services.prefs.clearUserPref(ROLLOUT_PREF_PHASE1);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    "TCP is still enabled by default after pref clear."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  await doEnrollmentCleanup();
  cleanup();
});

/**
 * Tests that in phase 2+ we don't target clients with enterprise policy setting
 * a cookie behavior.
 */
add_task(async function test_phase2_enterprise_policy_with_cookie_behavior() {
  // Disable TCP by default.
  setDefaultCookieBehavior(Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    "TCP is disabled by default."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        Locked: false,
        Behavior: "accept",
      },
    },
  });

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    "Cookie behavior is set to ACCEPT by enterprise policy."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  // Enable Nimbus feature for phase 2.
  let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "tcpByDefault",
    value: { enabled: true },
  });

  is(
    defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    "Cookie behavior is *still* set to ACCEPT_ALL by enterprise policy."
  );
  ok(
    !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
    "No user value for cookie behavior."
  );

  await doEnrollmentCleanup();
  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  cleanup();
});

/**
 * Tests that in phase 2+ still target clients with enterprise policy *not*
 * setting a cookie behavior.
 */
add_task(
  async function test_phase2_enterprise_policy_without_cookie_behavior() {
    // Disable TCP by default.
    setDefaultCookieBehavior(Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

    is(
      defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      "TCP is disabled by default."
    );
    ok(
      !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
      "No user value for cookie behavior."
    );

    await EnterprisePolicyTesting.setupPolicyEngineWithJson({
      policies: {
        PopupBlocking: {
          Locked: true,
        },
      },
    });

    is(
      defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      "Cookie behavior is still set to BEHAVIOR_REJECT_TRACKER."
    );
    ok(
      !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
      "No user value for cookie behavior."
    );

    let cookieBehaviorChange = waitForAndAssertPrefState(
      COOKIE_BEHAVIOR_PREF,
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      "Cookie behavior updates to TCP enabled after tcpByDefault enrollment."
    );

    // Enable Nimbus feature for phase 2.
    let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
      featureId: "tcpByDefault",
      value: { enabled: true },
    });

    await cookieBehaviorChange;
    is(
      defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF),
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      "TCP is enabled by default."
    );
    ok(
      !Services.prefs.prefHasUserValue(COOKIE_BEHAVIOR_PREF),
      "No user value for cookie behavior."
    );

    await doEnrollmentCleanup();
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
    cleanup();
  }
);
