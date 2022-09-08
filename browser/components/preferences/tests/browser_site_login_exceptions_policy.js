/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

const PERMISSIONS_URL =
  "chrome://browser/content/preferences/dialogs/permissions.xhtml";

var exceptionsDialog;

add_task(async function openLoginExceptionsSubDialog() {
  // ensure rememberSignons is off for this test;
  ok(
    !Services.prefs.getBoolPref("signon.rememberSignons"),
    "Check initial value of signon.rememberSignons pref"
  );

  // Undo the save password change.
  registerCleanupFunction(async function() {
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
      let doc = content.document;
      let savePasswordCheckBox = doc.getElementById("savePasswords");
      if (savePasswordCheckBox.checked) {
        savePasswordCheckBox.click();
      }
    });

    gBrowser.removeCurrentTab();
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  });

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      PasswordManagerExceptions: ["https://pwexception.example.com"],
    },
  });

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let dialogOpened = promiseLoadSubDialog(PERMISSIONS_URL);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    let doc = content.document;
    let savePasswordCheckBox = doc.getElementById("savePasswords");
    savePasswordCheckBox.click();

    let loginExceptionsButton = doc.getElementById("passwordExceptions");
    loginExceptionsButton.click();
  });

  exceptionsDialog = await dialogOpened;

  let doc = exceptionsDialog.document;

  let richlistbox = doc.getElementById("permissionsBox");
  Assert.equal(richlistbox.itemCount, 1, `Row count should initially be 1`);

  richlistbox.focus();
  richlistbox.selectedIndex = 0;
  Assert.ok(doc.getElementById("removePermission").disabled);
});
