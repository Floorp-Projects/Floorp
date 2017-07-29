const DEFAULT_HW_ACCEL_PREF = Services.prefs.getDefaultBranch(null).getBoolPref("layers.acceleration.disabled");
const DEFAULT_PROCESS_COUNT = Services.prefs.getDefaultBranch(null).getIntPref("dom.ipc.processCount");

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [
    ["layers.acceleration.disabled", DEFAULT_HW_ACCEL_PREF],
    ["dom.ipc.processCount", DEFAULT_PROCESS_COUNT],
    ["browser.preferences.defaultPerformanceSettings.enabled", true],
  ]});
});

add_task(async function() {
  Services.prefs.clearUserPref("dom.ipc.processCount");
  Services.prefs.setIntPref("dom.ipc.processCount.web", DEFAULT_PROCESS_COUNT + 1);
  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", null, {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.selectedBrowser.contentDocument;
  let useRecommendedPerformanceSettings = doc.querySelector("#useRecommendedPerformanceSettings");

  is(Services.prefs.getBoolPref("browser.preferences.defaultPerformanceSettings.enabled"), true,
    "pref value should be true before clicking on checkbox");
  ok(useRecommendedPerformanceSettings.checked, "checkbox should be checked before clicking on checkbox");

  useRecommendedPerformanceSettings.click();

  let performanceSettings = doc.querySelector("#performanceSettings");
  is(performanceSettings.hidden, false, "performance settings section is shown");

  is(Services.prefs.getBoolPref("browser.preferences.defaultPerformanceSettings.enabled"), false,
     "pref value should be false after clicking on checkbox");
  ok(!useRecommendedPerformanceSettings.checked, "checkbox should not be checked after clicking on checkbox");

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(contentProcessCount.disabled, false, "process count control should be enabled");
  is(Services.prefs.getIntPref("dom.ipc.processCount"), DEFAULT_PROCESS_COUNT + 1, "e10s rollout value should be default value");
  is(contentProcessCount.selectedItem.value, DEFAULT_PROCESS_COUNT + 1, "selected item should be the default one");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref("dom.ipc.processCount");
  Services.prefs.clearUserPref("dom.ipc.processCount.web");
  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);
});

add_task(async function() {
  Services.prefs.clearUserPref("dom.ipc.processCount");
  Services.prefs.setIntPref("dom.ipc.processCount.web", DEFAULT_PROCESS_COUNT + 1);
  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", false);

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", null, {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.selectedBrowser.contentDocument;
  let performanceSettings = doc.querySelector("#performanceSettings");
  is(performanceSettings.hidden, false, "performance settings section is shown");

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(contentProcessCount.disabled, false, "process count control should be enabled");
  is(Services.prefs.getIntPref("dom.ipc.processCount"), DEFAULT_PROCESS_COUNT, "default value should be the current value");
  is(contentProcessCount.selectedItem.value, DEFAULT_PROCESS_COUNT, "selected item should be the default one");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref("dom.ipc.processCount");
  Services.prefs.clearUserPref("dom.ipc.processCount.web");
  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);
});

add_task(async function() {
  Services.prefs.setIntPref("dom.ipc.processCount", DEFAULT_PROCESS_COUNT + 2);
  Services.prefs.setIntPref("dom.ipc.processCount.web", DEFAULT_PROCESS_COUNT + 1);
  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", false);

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", null, {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.selectedBrowser.contentDocument;
  let performanceSettings = doc.querySelector("#performanceSettings");
  is(performanceSettings.hidden, false, "performance settings section is shown");

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(contentProcessCount.disabled, false, "process count control should be enabled");
  is(Services.prefs.getIntPref("dom.ipc.processCount"), DEFAULT_PROCESS_COUNT + 2, "process count should be the set value");
  is(contentProcessCount.selectedItem.value, DEFAULT_PROCESS_COUNT + 2, "selected item should be the set one");

  let useRecommendedPerformanceSettings = doc.querySelector("#useRecommendedPerformanceSettings");
  useRecommendedPerformanceSettings.click();

  is(Services.prefs.getBoolPref("browser.preferences.defaultPerformanceSettings.enabled"), true,
    "pref value should be true after clicking on checkbox");
  is(Services.prefs.getIntPref("dom.ipc.processCount"), DEFAULT_PROCESS_COUNT,
    "process count should be default value");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  Services.prefs.clearUserPref("dom.ipc.processCount");
  Services.prefs.clearUserPref("dom.ipc.processCount.web");
  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);
});
