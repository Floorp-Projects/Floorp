/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

const PREF_NAME = "app.update.background.enabled";

async function test_background_update_pref(expectedEnabled, expectedLocked) {
  let actualEnabled = await UpdateUtils.readUpdateConfigSetting(PREF_NAME);
  is(
    actualEnabled,
    expectedEnabled,
    `Actual background update enabled setting should be ${expectedEnabled}`
  );

  let actualLocked = UpdateUtils.appUpdateSettingIsLocked(PREF_NAME);
  is(
    actualLocked,
    expectedLocked,
    `Background update enabled setting ${
      expectedLocked ? "should" : "should not"
    } be locked`
  );

  let setSuccess = true;
  try {
    await UpdateUtils.writeUpdateConfigSetting(PREF_NAME, actualEnabled);
  } catch (error) {
    setSuccess = false;
  }
  is(
    setSuccess,
    !expectedLocked,
    `Setting background update pref ${
      expectedLocked ? "should" : "should not"
    } fail`
  );

  if (AppConstants.MOZ_UPDATE_AGENT) {
    let shouldShowUI =
      !expectedLocked && UpdateUtils.PER_INSTALLATION_PREFS_SUPPORTED;
    await BrowserTestUtils.withNewTab("about:preferences", browser => {
      is(
        browser.contentDocument.getElementById("backgroundUpdate").hidden,
        !shouldShowUI,
        `When background update ${
          expectedLocked ? "is" : "isn't"
        } locked, and per-installation prefs ${
          UpdateUtils.PER_INSTALLATION_PREFS_SUPPORTED ? "are" : "aren't"
        } supported, the corresponding preferences entry ${
          shouldShowUI ? "shouldn't" : "should"
        } be hidden`
      );
    });
  } else {
    // The backgroundUpdate element is #ifdef'ed out if MOZ_UPDATER and
    // MOZ_UPDATE_AGENT are not both defined.
    info(
      "Warning: UI testing skipped because support for background update is " +
        "not present"
    );
  }
}

add_task(async function test_background_app_update_policy() {
  const origBackgroundUpdateVal = await UpdateUtils.readUpdateConfigSetting(
    PREF_NAME
  );
  registerCleanupFunction(async () => {
    await UpdateUtils.writeUpdateConfigSetting(
      PREF_NAME,
      origBackgroundUpdateVal
    );
  });

  await UpdateUtils.writeUpdateConfigSetting(PREF_NAME, true);
  await test_background_update_pref(true, false);

  await setupPolicyEngineWithJson({
    policies: {
      BackgroundAppUpdate: false,
    },
  });
  await test_background_update_pref(false, true);

  await setupPolicyEngineWithJson({});
  await UpdateUtils.writeUpdateConfigSetting(PREF_NAME, false);
  await test_background_update_pref(false, false);

  await setupPolicyEngineWithJson({
    policies: {
      BackgroundAppUpdate: true,
    },
  });
  await test_background_update_pref(true, true);
});
