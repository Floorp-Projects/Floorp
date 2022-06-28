/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BrowserGlue",
  "resource:///modules/BrowserGlue.jsm"
);

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PREF_DFPI_ENABLED_BY_DEFAULT =
  "privacy.restrict3rdpartystorage.rollout.enabledByDefault";
const PREF_DFPI_ROLLOUT_UI_ENABLED =
  "privacy.restrict3rdpartystorage.rollout.preferences.TCPToggleInStandard";
const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const CAT_PREF = "browser.contentblocking.category";

const LEARN_MORE_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  Services.prefs.getStringPref(
    "privacy.restrict3rdpartystorage.rollout.preferences.learnMoreURLSuffix"
  );

const {
  BEHAVIOR_REJECT_TRACKER,
  BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
} = Ci.nsICookieService;

function testTelemetryState(optIn) {
  let expectedValue;
  if (optIn == null) {
    expectedValue = 2;
  } else {
    expectedValue = optIn ? 1 : 0;
  }

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "privacy.dfpi_rollout_enabledByDefault",
    expectedValue,
    "Scalar should have correct value"
  );
}

add_setup(async function() {
  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const previousDefaultCB = defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF);

  registerCleanupFunction(function() {
    defaultPrefs.setIntPref(COOKIE_BEHAVIOR_PREF, previousDefaultCB);
    BrowserGlue._defaultCookieBehaviorAtStartup = previousDefaultCB;
  });

  // The BrowserGlue code which computes this flag runs before we can set the
  // default cookie behavior for this test. Thus we need to overwrite it in
  // order for the opt-out to work correctly.
  BrowserGlue._defaultCookieBehaviorAtStartup = BEHAVIOR_REJECT_TRACKER;
  defaultPrefs.setIntPref(
    COOKIE_BEHAVIOR_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
});

async function testRolloutUI({
  dFPIEnabledByDefault,
  rolloutUIEnabled,
  rolloutUIEnabledByExperiment,
}) {
  info(
    "Testing TCP preferences rollout UI " +
      JSON.stringify({ dFPIEnabledByDefault, rolloutUIEnabled })
  );

  // Initially the rollout pref is not set. Telemetry should record this unset
  // state.
  testTelemetryState(null);

  // Setting to standard category explicitly, since changing the default cookie
  // behavior still switches us to custom initially.
  let set = [[CAT_PREF, "standard"]];
  if (dFPIEnabledByDefault) {
    set.push([PREF_DFPI_ENABLED_BY_DEFAULT, true]);
  }
  if (rolloutUIEnabled) {
    set.push([PREF_DFPI_ROLLOUT_UI_ENABLED, true]);
  }
  await SpecialPowers.pushPrefEnv({ set });

  // At this point the pref can only be enabled or unset.
  testTelemetryState(dFPIEnabledByDefault || null);

  const uiEnabled =
    rolloutUIEnabled ||
    rolloutUIEnabledByExperiment ||
    (dFPIEnabledByDefault &&
      Services.prefs.prefHasUserValue(PREF_DFPI_ENABLED_BY_DEFAULT));

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let standardRadioOption = doc.getElementById("standardRadio");
  let strictRadioOption = doc.getElementById("strictRadio");
  let customRadioOption = doc.getElementById("customRadio");

  ok(standardRadioOption.selected, "Standard category is selected");

  is(
    Services.prefs.getIntPref(COOKIE_BEHAVIOR_PREF),
    dFPIEnabledByDefault
      ? BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
      : BEHAVIOR_REJECT_TRACKER,
    `dFPI is ${dFPIEnabledByDefault ? "enabled" : "disabled"}.`
  );

  let etpStandardTCPRolloutBox = doc.getElementById("etpStandardTCPRolloutBox");
  is(
    BrowserTestUtils.is_visible(etpStandardTCPRolloutBox),
    uiEnabled,
    `Rollout UI in standard is ${uiEnabled ? " " : "not "}visible.`
  );

  // Ensure that the regular TCP standard section is only visible if we don't show the rollout section.
  let etpStandardTCPBox = doc.getElementById("etpStandardTCPBox");
  let tcpSectionEnabled =
    !uiEnabled &&
    Services.prefs.getIntPref(COOKIE_BEHAVIOR_PREF) ==
      BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  is(
    BrowserTestUtils.is_visible(etpStandardTCPBox),
    tcpSectionEnabled,
    `Non-rollout TCP section in standard is ${
      tcpSectionEnabled ? " " : "not "
    }visible.`
  );

  if (uiEnabled) {
    // Test the TCP rollout section.
    let learnMoreLink = etpStandardTCPRolloutBox.querySelector(
      "#tcp-rollout-learn-more-link"
    );
    ok(learnMoreLink, "Should have a learn more link");
    BrowserTestUtils.is_visible(
      learnMoreLink,
      "Learn more link should be visible."
    );
    ok(
      learnMoreLink.href && !learnMoreLink.href.startsWith("about:blank"),
      "Learn more link should be valid"
    );
    is(
      learnMoreLink.href,
      LEARN_MORE_URL,
      "Learn more link should have the right target."
    );

    let description = etpStandardTCPRolloutBox.querySelector(
      ".tail-with-learn-more"
    );
    ok(description, "Should have a description element.");
    BrowserTestUtils.is_visible(description, "Description should be visible.");

    is(
      BrowserTestUtils.is_visible(etpStandardTCPRolloutBox),
      uiEnabled,
      `Rollout UI in standard is ${uiEnabled ? " " : "not "}visible.`
    );

    // Test the checkbox which toggles TCP in standard mode.
    let checkbox = doc.getElementById("etpStandardTCPRollout");

    ok(checkbox, "Should have a checkbox.");
    BrowserTestUtils.is_visible(checkbox, "TCP checkbox should be visible.");
    ok(checkbox.label, "Checkbox should have a label.");

    let [reloadWarning] = doc.getElementsByClassName(
      "content-blocking-reload-description"
    );
    ok(
      !BrowserTestUtils.is_visible(reloadWarning),
      "No reload warning initially."
    );

    is(
      checkbox.checked,
      dFPIEnabledByDefault,
      "Initial checkbox state reflects dFPI rollout pref state"
    );

    // If checkbox is not active initially, check it now and assert pref state.
    if (!dFPIEnabledByDefault) {
      let prefChangeDFPIByDefault = waitForAndAssertPrefState(
        PREF_DFPI_ENABLED_BY_DEFAULT,
        true,
        "Checkbox should set " + PREF_DFPI_ENABLED_BY_DEFAULT
      );
      let prefChangeCookieBehavior = waitForAndAssertPrefState(
        COOKIE_BEHAVIOR_PREF,
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
        "Checkbox should set " + COOKIE_BEHAVIOR_PREF
      );
      checkbox.click();
      await prefChangeDFPIByDefault;
      await prefChangeCookieBehavior;
      is(
        Services.prefs.getStringPref(CAT_PREF),
        "standard",
        "Should still be in standard category"
      );
      ok(
        BrowserTestUtils.is_visible(reloadWarning),
        "Reload warning should be visible."
      );
      testTelemetryState(true);
    }

    // Un-check checkbox and assert pref state.
    let prefChangeDFPIByDefault = waitForAndAssertPrefState(
      PREF_DFPI_ENABLED_BY_DEFAULT,
      false,
      "Checkbox should set " + PREF_DFPI_ENABLED_BY_DEFAULT
    );
    let prefChangeCookieBehavior = waitForAndAssertPrefState(
      COOKIE_BEHAVIOR_PREF,
      BEHAVIOR_REJECT_TRACKER,
      "Checkbox should set " + COOKIE_BEHAVIOR_PREF
    );
    checkbox.click();
    await prefChangeDFPIByDefault;
    await prefChangeCookieBehavior;
    is(
      Services.prefs.getStringPref(CAT_PREF),
      "standard",
      "Should still be in standard category"
    );
    ok(
      BrowserTestUtils.is_visible(reloadWarning),
      "Reload warning should be visible."
    );
    testTelemetryState(false);
  }

  let categoryPrefChange = waitForAndAssertPrefState(CAT_PREF, "strict");
  strictRadioOption.click();
  await categoryPrefChange;
  ok(
    !BrowserTestUtils.is_visible(etpStandardTCPRolloutBox),
    "When strict is selected rollout UI is not visible."
  );

  categoryPrefChange = waitForAndAssertPrefState(CAT_PREF, "custom");
  customRadioOption.click();
  await categoryPrefChange;
  ok(
    !BrowserTestUtils.is_visible(etpStandardTCPRolloutBox),
    "When custom is selected rollout UI is not visible."
  );

  // Switch back to standard and test if we show the rollout UI again.
  categoryPrefChange = waitForAndAssertPrefState(CAT_PREF, "standard");
  standardRadioOption.click();
  await categoryPrefChange;
  is(
    BrowserTestUtils.is_visible(etpStandardTCPRolloutBox),
    uiEnabled,
    `Rollout UI in standard is ${uiEnabled ? " " : "not "}visible.`
  );

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
  Services.prefs.setStringPref(CAT_PREF, "standard");
  Services.prefs.clearUserPref(PREF_DFPI_ENABLED_BY_DEFAULT);
  Services.prefs.clearUserPref(PREF_DFPI_ROLLOUT_UI_ENABLED);

  testTelemetryState(null);
}

// Clients which are not part of the rollout. They should not see the
// preferences UI.
add_task(async function test_rollout_ui_disabled() {
  await testRolloutUI({ dFPIEnabledByDefault: false, rolloutUIEnabled: false });
});

// Clients which are part of the rollout, user opted-in to dFPI in standard.
// They should see the preferences UI with the checkbox checked.
add_task(async function test_rollout_ui_enabled() {
  await testRolloutUI({ dFPIEnabledByDefault: true, rolloutUIEnabled: true });
});

// Clients which are part of the rollout, user didn't opt-in to dFPI in
// standard. e.g. they dismissed the onboarding dialog.
// They should see the preferences UI with the checkbox unchecked.
add_task(async function test_rollout_ui_enabled_dfpi_disabled() {
  await testRolloutUI({ dFPIEnabledByDefault: false, rolloutUIEnabled: true });
});

add_task(async function test_rollout_ui_enabled_by_nimbus() {
  await testRolloutUI({
    dFPIEnabledByDefault: false,
    rolloutUIEnabled: false,
    rolloutUIEnabledByExperiment: false,
  });

  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "tcpPreferences",
    value: { enabled: true },
  });

  await testRolloutUI({
    dFPIEnabledByDefault: false,
    rolloutUIEnabled: false,
    rolloutUIEnabledByExperiment: true,
  });

  await doCleanup();

  await testRolloutUI({
    dFPIEnabledByDefault: false,
    rolloutUIEnabled: false,
    rolloutUIEnabledByExperiment: false,
  });
});
