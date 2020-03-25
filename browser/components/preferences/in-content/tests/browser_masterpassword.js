ChromeUtils.import("resource://testing-common/OSKeyStoreTestUtils.jsm", this);

add_task(async function() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "panePrivacy", "Privacy pane was selected");

  let doc = gBrowser.contentDocument;
  // Fake the subdialog and LoginHelper
  let win = doc.defaultView;
  let dialogURL = "";
  let dialogOpened = false;
  win.gSubDialog = {
    open(aDialogURL, unused, unused2, aCallback) {
      dialogOpened = true;
      dialogURL = aDialogURL;
      masterPasswordSet = masterPasswordNextState;
      aCallback();
    },
  };
  let masterPasswordSet = false;
  win.LoginHelper = {
    isMasterPasswordSet() {
      return masterPasswordSet;
    },
  };

  let checkbox = doc.querySelector("#useMasterPassword");
  checkbox.scrollIntoView();
  ok(
    !checkbox.checked,
    "master password checkbox should be unchecked by default"
  );
  let button = doc.getElementById("changeMasterPassword");
  ok(button.disabled, "master password button should be disabled by default");

  let masterPasswordNextState = false;
  if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    let osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
    checkbox.click();
    info("waiting for os auth dialog to appear and get canceled");
    await osAuthDialogShown;
    await TestUtils.waitForCondition(
      () => !checkbox.checked,
      "wait for checkbox to get unchecked"
    );
    ok(!dialogOpened, "the dialog should not have opened");
    ok(
      !dialogURL,
      "the changemp dialog should not have been opened when the os auth dialog is canceled"
    );
    ok(
      !checkbox.checked,
      "master password checkbox should be unchecked after canceling os auth dialog"
    );
    ok(button.disabled, "button should be disabled after canceling os auth");
  }

  masterPasswordNextState = true;
  if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    let osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
    checkbox.click();
    info("waiting for os auth dialog to appear");
    await osAuthDialogShown;
    info("waiting for dialogURL to get set");
    await TestUtils.waitForCondition(
      () => dialogURL,
      "wait for open to get called asynchronously"
    );
    is(
      dialogURL,
      "chrome://mozapps/content/preferences/changemp.xhtml",
      "clicking on the checkbox should open the masterpassword dialog"
    );
  } else {
    masterPasswordSet = true;
    doc.defaultView.gPrivacyPane._initMasterPasswordUI();
    await TestUtils.waitForCondition(
      () => !button.disabled,
      "waiting for master password button to get enabled"
    );
  }
  ok(!button.disabled, "master password button should now be enabled");
  ok(checkbox.checked, "master password checkbox should be checked now");

  dialogURL = "";
  button.doCommand();
  await TestUtils.waitForCondition(
    () => dialogURL,
    "wait for open to get called asynchronously"
  );
  is(
    dialogURL,
    "chrome://mozapps/content/preferences/changemp.xhtml",
    "clicking on the button should open the masterpassword dialog"
  );
  ok(!button.disabled, "master password button should still be enabled");
  ok(checkbox.checked, "master password checkbox should be checked still");

  // Confirm that we won't automatically respond to the dialog,
  // since we don't expect a dialog here, we want the test to fail if one appears.
  is(
    Services.prefs.getStringPref(
      "toolkit.osKeyStore.unofficialBuildOnlyLogin",
      ""
    ),
    "",
    "Pref should be set to an empty string"
  );

  masterPasswordNextState = false;
  dialogURL = "";
  checkbox.click();
  is(
    dialogURL,
    "chrome://mozapps/content/preferences/removemp.xhtml",
    "clicking on the checkbox to uncheck master password should show the removal dialog"
  );
  ok(button.disabled, "master password button should now be disabled");
  ok(!checkbox.checked, "master password checkbox should now be unchecked");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
