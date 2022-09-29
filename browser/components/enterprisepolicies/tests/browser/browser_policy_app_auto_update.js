/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
ChromeUtils.defineESModuleGetters(this, {
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
});

async function test_app_update_auto(expectedEnabled, expectedLocked) {
  let actualEnabled = await UpdateUtils.getAppUpdateAutoEnabled();
  is(
    actualEnabled,
    expectedEnabled,
    `Actual auto update enabled setting should match the expected value of ${expectedEnabled}`
  );

  let actualLocked = UpdateUtils.appUpdateAutoSettingIsLocked();
  is(
    actualLocked,
    expectedLocked,
    `Auto update enabled setting ${
      expectedLocked ? "should" : "should not"
    } be locked`
  );

  let setSuccess = true;
  try {
    await UpdateUtils.setAppUpdateAutoEnabled(actualEnabled);
  } catch (error) {
    setSuccess = false;
  }
  is(
    setSuccess,
    !expectedLocked,
    `Setting auto update ${expectedLocked ? "should" : "should not"} fail`
  );

  await BrowserTestUtils.withNewTab("about:preferences", browser => {
    is(
      browser.contentDocument.getElementById("updateSettingsContainer").hidden,
      expectedLocked,
      `When auto update ${
        expectedLocked ? "is" : "isn't"
      } locked, the corresponding preferences entry ${
        expectedLocked ? "should" : "shouldn't"
      } be hidden`
    );
  });
}

add_task(async function test_app_auto_update_policy() {
  let originalUpdateAutoValue = await UpdateUtils.getAppUpdateAutoEnabled();
  registerCleanupFunction(async () => {
    await UpdateUtils.setAppUpdateAutoEnabled(originalUpdateAutoValue);
  });

  await UpdateUtils.setAppUpdateAutoEnabled(true);
  await test_app_update_auto(true, false);

  await setupPolicyEngineWithJson({
    policies: {
      AppAutoUpdate: false,
    },
  });
  await test_app_update_auto(false, true);

  await setupPolicyEngineWithJson({});
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  await test_app_update_auto(false, false);

  await setupPolicyEngineWithJson({
    policies: {
      AppAutoUpdate: true,
    },
  });
  await test_app_update_auto(true, true);
});
