const DEFAULT_HW_ACCEL_PREF = Services.prefs
  .getDefaultBranch(null)
  .getBoolPref("layers.acceleration.disabled");
const DEFAULT_PROCESS_COUNT = Services.prefs
  .getDefaultBranch(null)
  .getIntPref("dom.ipc.processCount");

add_task(async function() {
  // We must temporarily disable `Once` StaticPrefs check for the duration of
  // this test (see bug 1556131). We must do so in a separate operation as
  // pushPrefEnv doesn't set the preferences in the order one could expect.
  await SpecialPowers.pushPrefEnv({
    set: [["preferences.force-disable.check.once.policy", true]],
  });
  await SpecialPowers.pushPrefEnv({
    set: [
      ["layers.acceleration.disabled", DEFAULT_HW_ACCEL_PREF],
      ["dom.ipc.processCount", DEFAULT_PROCESS_COUNT],
      ["browser.preferences.defaultPerformanceSettings.enabled", true],
    ],
  });
});

add_task(async function() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
  let useRecommendedPerformanceSettings = doc.querySelector(
    "#useRecommendedPerformanceSettings"
  );

  is(
    Services.prefs.getBoolPref(
      "browser.preferences.defaultPerformanceSettings.enabled"
    ),
    true,
    "pref value should be true before clicking on checkbox"
  );
  ok(
    useRecommendedPerformanceSettings.checked,
    "checkbox should be checked before clicking on checkbox"
  );

  useRecommendedPerformanceSettings.click();

  let performanceSettings = doc.querySelector("#performanceSettings");
  is(
    performanceSettings.hidden,
    false,
    "performance settings section is shown"
  );

  is(
    Services.prefs.getBoolPref(
      "browser.preferences.defaultPerformanceSettings.enabled"
    ),
    false,
    "pref value should be false after clicking on checkbox"
  );
  ok(
    !useRecommendedPerformanceSettings.checked,
    "checkbox should not be checked after clicking on checkbox"
  );

  let allowHWAccel = doc.querySelector("#allowHWAccel");
  let allowHWAccelPref = Services.prefs.getBoolPref(
    "layers.acceleration.disabled"
  );
  is(
    allowHWAccelPref,
    DEFAULT_HW_ACCEL_PREF,
    "pref value should be the default value before clicking on checkbox"
  );
  is(
    allowHWAccel.checked,
    !DEFAULT_HW_ACCEL_PREF,
    "checkbox should show the invert of the default value"
  );

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(
    contentProcessCount.disabled,
    true,
    "process count control should be disabled"
  );

  let contentProcessCountEnabledDescription = doc.querySelector(
    "#contentProcessCountEnabledDescription"
  );
  is(
    contentProcessCountEnabledDescription.hidden,
    true,
    "process count enabled description should be hidden"
  );

  let contentProcessCountDisabledDescription = doc.querySelector(
    "#contentProcessCountDisabledDescription"
  );
  is(
    contentProcessCountDisabledDescription.hidden,
    false,
    "process count enabled description should be shown"
  );

  allowHWAccel.click();
  allowHWAccelPref = Services.prefs.getBoolPref("layers.acceleration.disabled");
  is(
    allowHWAccelPref,
    !DEFAULT_HW_ACCEL_PREF,
    "pref value should be opposite of the default value after clicking on checkbox"
  );
  is(
    allowHWAccel.checked,
    !allowHWAccelPref,
    "checkbox should show the invert of the current value"
  );

  allowHWAccel.click();
  allowHWAccelPref = Services.prefs.getBoolPref("layers.acceleration.disabled");
  is(
    allowHWAccelPref,
    DEFAULT_HW_ACCEL_PREF,
    "pref value should be the default value after clicking on checkbox"
  );
  is(
    allowHWAccel.checked,
    !allowHWAccelPref,
    "checkbox should show the invert of the current value"
  );

  is(
    performanceSettings.hidden,
    false,
    "performance settings section should be still shown"
  );

  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    true
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
  let performanceSettings = doc.querySelector("#performanceSettings");

  is(
    performanceSettings.hidden,
    true,
    "performance settings section should not be shown"
  );

  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    false
  );

  is(
    performanceSettings.hidden,
    false,
    "performance settings section should be shown"
  );

  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    true
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function() {
  Services.prefs.setBoolPref("layers.acceleration.disabled", true);

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;

  let performanceSettings = doc.querySelector("#performanceSettings");
  is(
    performanceSettings.hidden,
    false,
    "performance settings section should be shown"
  );

  let allowHWAccel = doc.querySelector("#allowHWAccel");
  is(
    Services.prefs.getBoolPref("layers.acceleration.disabled"),
    true,
    "pref value is false"
  );
  ok(!allowHWAccel.checked, "checkbox should not be checked");

  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    true
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
