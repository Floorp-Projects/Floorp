SpecialPowers.pushPrefEnv({set: [
  ["browser.preferences.defaultPerformanceSettings.enabled", true],
  ["dom.ipc.processCount", 4],
  ["layers.acceleration.disabled", false],
]});

add_task(function*() {
  let prefs = yield openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
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

  let allowHWAccel = doc.querySelector("#allowHWAccel");
  is(Services.prefs.getBoolPref("layers.acceleration.disabled"), false,
    "pref value should be false before clicking on checkbox");
  ok(allowHWAccel.checked, "checkbox should be checked");

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(Services.prefs.getIntPref("dom.ipc.processCount"), 4, "default pref value should be default value");
  is(contentProcessCount.selectedItem.value, 4, "selected item should be the default one");

  allowHWAccel.click();
  is(Services.prefs.getBoolPref("layers.acceleration.disabled"), true,
    "pref value should be true after clicking on checkbox");
  ok(!allowHWAccel.checked, "checkbox should not be checked");

  contentProcessCount.value = 7;
  contentProcessCount.doCommand();
  is(Services.prefs.getIntPref("dom.ipc.processCount"), 7, "pref value should be 7");
  is(contentProcessCount.selectedItem.value, 7, "selected item should be 7");

  allowHWAccel.click();
  is(Services.prefs.getBoolPref("layers.acceleration.disabled"), false,
    "pref value should be false after clicking on checkbox");
  ok(allowHWAccel.checked, "checkbox should not be checked");

  contentProcessCount.value = 4;
  contentProcessCount.doCommand();
  is(Services.prefs.getIntPref("dom.ipc.processCount"), 4, "pref value should be default value");
  is(contentProcessCount.selectedItem.value, 4, "selected item should be default one");

  is(performanceSettings.hidden, false, "performance settings section should be still shown");

  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(function*() {
  let prefs = yield openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
  let performanceSettings = doc.querySelector("#performanceSettings");

  is(performanceSettings.hidden, true, "performance settings section should not be shown");

  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", false);

  is(performanceSettings.hidden, false, "performance settings section should be shown");

  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(function*() {
  Services.prefs.setIntPref("dom.ipc.processCount", 7);

  let prefs = yield openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;

  let performanceSettings = doc.querySelector("#performanceSettings");
  is(performanceSettings.hidden, false, "performance settings section should be shown");

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(Services.prefs.getIntPref("dom.ipc.processCount"), 7, "pref value should be 7");
  is(contentProcessCount.selectedItem.value, 7, "selected item should be 7");

  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(function*() {
  Services.prefs.setBoolPref("layers.acceleration.disabled", true);

  let prefs = yield openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;

  let performanceSettings = doc.querySelector("#performanceSettings");
  is(performanceSettings.hidden, false, "performance settings section should be shown");

  let allowHWAccel = doc.querySelector("#allowHWAccel");
  is(Services.prefs.getBoolPref("layers.acceleration.disabled"), true,
    "pref value is false");
  ok(!allowHWAccel.checked, "checkbox should not be checked");

  Services.prefs.setBoolPref("browser.preferences.defaultPerformanceSettings.enabled", true);
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
