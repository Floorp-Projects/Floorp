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

  // The pref is true by default.
  await modifyPrefAndWaitUntilCheckBoxChanges(false);
  await modifyPrefAndWaitUntilCheckBoxChanges(true);
  await toggleCheckBoxAndWaitUntilPrefValueChanges(false);
  await toggleCheckBoxAndWaitUntilPrefValueChanges(true);
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
  checkAndClearTelemetryProbe(isEnabled);
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
  checkAndClearTelemetryProbe(isChecked, true /* check UI */);
}

/**
 * These telemetry related variable and method should be removed after the
 * telemetry probe `MEDIA_CONTROL_SETTING_CHANGE` gets expired.
 */
const HISTOGRAM_ID = "MEDIA_CONTROL_SETTING_CHANGE";
const HISTOGRAM_KEYS = {
  EnableFromUI: 0,
  EnableTotal: 1,
  DisableFromUI: 2,
  DisableTotal: 3,
};

function checkAndClearTelemetryProbe(isEnable, checkUI = false) {
  const histogram = Services.telemetry.getHistogramById(HISTOGRAM_ID);
  let keyTotal = isEnable ? "EnableTotal" : "DisableTotal";
  let keyUI = null;
  if (checkUI) {
    keyUI = isEnable ? "EnableFromUI" : "DisableFromUI";
  }
  for (let [key, val] of Object.entries(histogram.snapshot().values)) {
    if (key == HISTOGRAM_KEYS[keyTotal]) {
      ok(val, "Increase the amount for the probe 'changeing total setting'");
    }
    if (keyUI && key == HISTOGRAM_KEYS[keyUI]) {
      ok(val, "Increase the amount for the probe 'changeing setting from UI'");
    }
  }
  histogram.clear();
}
