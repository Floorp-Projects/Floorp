add_task(async function() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane was selected");

  let doc = gBrowser.contentDocument;
  // Fake the subdialog and LoginHelper
  let win = doc.defaultView;
  let dialogURL = "";
  win.gSubDialog = {
    open(aDialogURL, unused, unused2, aCallback) {
      dialogURL = aDialogURL;
      masterPasswordSet = masterPasswordNextState;
      aCallback();
    }
  };
  let masterPasswordSet = false;
  win.LoginHelper = {
    isMasterPasswordSet() {
      return masterPasswordSet;
    }
  };

  let checkbox = doc.querySelector("#useMasterPassword");
  ok(!checkbox.checked, "master password checkbox should be unchecked by default");
  let button = doc.getElementById("changeMasterPassword");
  ok(button.disabled, "master password button should be disabled by default");

  let masterPasswordNextState = true;
  checkbox.click();
  is(dialogURL,
     "chrome://mozapps/content/preferences/changemp.xul",
     "clicking on the checkbox should open the masterpassword dialog");
  ok(!button.disabled, "master password button should now be enabled");
  ok(checkbox.checked, "master password checkbox should be checked now");

  dialogURL = "";
  button.doCommand();
  is(dialogURL,
     "chrome://mozapps/content/preferences/changemp.xul",
     "clicking on the button should open the masterpassword dialog");
  ok(!button.disabled, "master password button should still be enabled");
  ok(checkbox.checked, "master password checkbox should be checked still");

  masterPasswordNextState = false;
  dialogURL = "";
  checkbox.click();
  is(dialogURL,
     "chrome://mozapps/content/preferences/removemp.xul",
     "clicking on the checkbox to uncheck master password should show the removal dialog");
  ok(button.disabled, "master password button should now be disabled");
  ok(!checkbox.checked, "master password checkbox should now be unchecked");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
