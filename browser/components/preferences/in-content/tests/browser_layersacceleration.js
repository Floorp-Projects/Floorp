add_task(async function() {
  SpecialPowers.pushPrefEnv({set: [
    ["gfx.direct2d.disabled", false],
    ["layers.acceleration.disabled", false]
  ]});

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
  let checkbox = doc.querySelector("#allowHWAccel");
  is(!checkbox.checked,
     Services.prefs.getBoolPref("layers.acceleration.disabled"),
     "checkbox should represent inverted pref value before clicking on checkbox");

  checkbox.click();

  is(!checkbox.checked,
     Services.prefs.getBoolPref("layers.acceleration.disabled"),
     "checkbox should represent inverted pref value after clicking on checkbox");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
