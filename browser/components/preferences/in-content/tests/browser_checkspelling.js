add_task(async function() {
  SpecialPowers.pushPrefEnv({ set: [["layout.spellcheckDefault", 2]] });

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;
  let checkbox = doc.querySelector("#checkSpelling");
  is(
    checkbox.checked,
    Services.prefs.getIntPref("layout.spellcheckDefault") == 2,
    "checkbox should represent pref value before clicking on checkbox"
  );
  ok(
    checkbox.checked,
    "checkbox should be checked before clicking on checkbox"
  );

  checkbox.click();

  is(
    checkbox.checked,
    Services.prefs.getIntPref("layout.spellcheckDefault") == 2,
    "checkbox should represent pref value after clicking on checkbox"
  );
  ok(
    !checkbox.checked,
    "checkbox should not be checked after clicking on checkbox"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
