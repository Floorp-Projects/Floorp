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

add_task(async function testPrefsAreDefault() {
  Services.prefs.setIntPref("dom.ipc.processCount", DEFAULT_PROCESS_COUNT);
  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    true
  );

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

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(
    contentProcessCount.disabled,
    false,
    "process count control should be enabled"
  );
  is(
    Services.prefs.getIntPref("dom.ipc.processCount"),
    DEFAULT_PROCESS_COUNT,
    "default pref should be default value"
  );
  is(
    contentProcessCount.selectedItem.value,
    DEFAULT_PROCESS_COUNT,
    "selected item should be the default one"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref("dom.ipc.processCount");
  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    true
  );
});

add_task(async function testPrefsSetByUser() {
  const kNewCount = DEFAULT_PROCESS_COUNT - 2;

  Services.prefs.setIntPref("dom.ipc.processCount", kNewCount);
  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    false
  );

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
  let performanceSettings = doc.querySelector("#performanceSettings");
  is(
    performanceSettings.hidden,
    false,
    "performance settings section is shown"
  );

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(
    contentProcessCount.disabled,
    false,
    "process count control should be enabled"
  );
  is(
    Services.prefs.getIntPref("dom.ipc.processCount"),
    kNewCount,
    "process count should be the set value"
  );
  is(
    contentProcessCount.selectedItem.value,
    kNewCount,
    "selected item should be the set one"
  );

  let useRecommendedPerformanceSettings = doc.querySelector(
    "#useRecommendedPerformanceSettings"
  );
  useRecommendedPerformanceSettings.click();

  is(
    Services.prefs.getBoolPref(
      "browser.preferences.defaultPerformanceSettings.enabled"
    ),
    true,
    "pref value should be true after clicking on checkbox"
  );
  is(
    Services.prefs.getIntPref("dom.ipc.processCount"),
    DEFAULT_PROCESS_COUNT,
    "process count should be default value"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref("dom.ipc.processCount");
  Services.prefs.setBoolPref(
    "browser.preferences.defaultPerformanceSettings.enabled",
    true
  );
});
