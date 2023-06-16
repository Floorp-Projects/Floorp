/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MASTER_PASSWORD = "omgsecret!";
const mpToken = Cc["@mozilla.org/security/pk11tokendb;1"]
  .getService(Ci.nsIPK11TokenDB)
  .getInternalKeyToken();

async function checkDeviceManager({ buttonIsDisabled }) {
  let deviceManagerWindow = window.openDialog(
    "chrome://pippki/content/device_manager.xhtml",
    "",
    ""
  );
  await BrowserTestUtils.waitForEvent(deviceManagerWindow, "load");

  let tree = deviceManagerWindow.document.getElementById("device_tree");
  ok(tree, "The device tree exists");

  // Find and select the item related to the internal key token
  for (let i = 0; i < tree.view.rowCount; i++) {
    tree.view.selection.select(i);

    try {
      let selected_token = deviceManagerWindow.selected_slot.getToken();
      if (selected_token.isInternalKeyToken) {
        break;
      }
    } catch (e) {}
  }

  // Check to see if the button was updated correctly
  let changePwButton =
    deviceManagerWindow.document.getElementById("change_pw_button");
  is(
    changePwButton.getAttribute("disabled") == "true",
    buttonIsDisabled,
    "Change Password button is in the correct state: " + buttonIsDisabled
  );

  await BrowserTestUtils.closeWindow(deviceManagerWindow);
}

async function checkAboutPreferences({ checkboxIsDisabled }) {
  await BrowserTestUtils.withNewTab(
    "about:preferences#privacy",
    async browser => {
      is(
        browser.contentDocument.getElementById("useMasterPassword").disabled,
        checkboxIsDisabled,
        "Master Password checkbox is in the correct state: " +
          checkboxIsDisabled
      );
    }
  );
}

add_task(async function test_policy_disable_masterpassword() {
  ok(!mpToken.hasPassword, "Starting the test with no password");

  // No password and no policy: access to setting a primary password
  // should be enabled.
  await checkDeviceManager({ buttonIsDisabled: false });
  await checkAboutPreferences({ checkboxIsDisabled: false });

  await setupPolicyEngineWithJson({
    policies: {
      DisableMasterPasswordCreation: true,
    },
  });

  // With the `DisableMasterPasswordCreation: true` policy active, the
  // UI entry points for creating a Primary Password should be disabled.
  await checkDeviceManager({ buttonIsDisabled: true });
  await checkAboutPreferences({ checkboxIsDisabled: true });

  mpToken.changePassword("", MASTER_PASSWORD);
  ok(mpToken.hasPassword, "Master password was set");

  // If a Primary Password is already set, there's no point in disabling
  // the
  await checkDeviceManager({ buttonIsDisabled: false });
  await checkAboutPreferences({ checkboxIsDisabled: false });

  // Clean up
  mpToken.changePassword(MASTER_PASSWORD, "");
  ok(!mpToken.hasPassword, "Master password was cleaned up");
});
