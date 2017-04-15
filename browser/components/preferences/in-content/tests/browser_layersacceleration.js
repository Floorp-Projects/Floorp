add_task(function*() {
  SpecialPowers.pushPrefEnv({set: [
    ["gfx.direct2d.disabled", false],
    ["layers.acceleration.disabled", false]
  ]});

  let prefs = yield openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
  let checkbox = doc.querySelector("#allowHWAccel");
  is(!checkbox.checked,
     Services.prefs.getBoolPref("layers.acceleration.disabled"),
     "checkbox should represent inverted pref value before clicking on checkbox");

  if (AppConstants.platform == "win") {
    is(Services.prefs.getBoolPref("gfx.direct2d.disabled"), false, "direct2d pref should be set to false");
  }

  checkbox.click();

  is(!checkbox.checked,
     Services.prefs.getBoolPref("layers.acceleration.disabled"),
     "checkbox should represent inverted pref value after clicking on checkbox");
  if (AppConstants.platform == "win") {
    is(Services.prefs.getBoolPref("gfx.direct2d.disabled"), true, "direct2d pref should be set to true");
  }

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
