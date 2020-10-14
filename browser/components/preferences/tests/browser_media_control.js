/**
 * Media control check box should change the media control pref, vice versa.
 */
const MEDIA_CONTROL_PREF = "media.hardwaremediakeys.enabled";

add_task(async function testMediaControlCheckBox() {
  const prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  const checkBox = gBrowser.contentDocument.getElementById(
    "mediaControlToggleEnabled"
  );
  ok(checkBox, "check box exists");

  await modifyPrefAndWaitUntilCheckBoxChanges(true);
  await modifyPrefAndWaitUntilCheckBoxChanges(false);
  await toggleCheckBoxAndWaitUntilPrefValueChanges(true);
  await toggleCheckBoxAndWaitUntilPrefValueChanges(false);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

async function modifyPrefAndWaitUntilCheckBoxChanges(isEnabled) {
  info((isEnabled ? "enable" : "disable") + " the pref");
  const checkBox = gBrowser.contentDocument.getElementById(
    "mediaControlToggleEnabled"
  );
  await SpecialPowers.pushPrefEnv({
    set: [[MEDIA_CONTROL_PREF, isEnabled]],
  });
  await TestUtils.waitForCondition(
    _ => checkBox.checked == isEnabled,
    "Waiting for the checkbox gets checked"
  );
  is(checkBox.checked, isEnabled, `check box status is correct`);
}

async function toggleCheckBoxAndWaitUntilPrefValueChanges(isChecked) {
  info((isChecked ? "check" : "uncheck") + " the check box");
  const checkBox = gBrowser.contentDocument.getElementById(
    "mediaControlToggleEnabled"
  );
  checkBox.click();
  is(
    Services.prefs.getBoolPref(MEDIA_CONTROL_PREF),
    isChecked,
    "the pref's value is correct"
  );
}
