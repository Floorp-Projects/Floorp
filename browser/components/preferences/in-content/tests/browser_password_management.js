"use strict";

ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm", this);

const PM_URL = "chrome://passwordmgr/content/passwordManager.xul";
const PREF_MANAGEMENT_URI = "signon.management.overrideURI";

var passwordsDialog;

add_task(async function test_setup() {
  Services.logins.removeAllLogins();

  // add login data
  let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                                 Ci.nsILoginInfo, "init");
  let login = new nsLoginInfo("http://example.com/", "http://example.com/", null,
                              "user", "password", "u1", "p1");
  Services.logins.addLogin(login);

  registerCleanupFunction(async function() {
    Services.logins.removeAllLogins();
    Services.prefs.clearUserPref(PREF_MANAGEMENT_URI);
  });
});

add_task(async function test_openPasswordSubDialog() {
  Services.telemetry.clearEvents();
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});

  let dialogOpened = promiseLoadSubDialog(PM_URL);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let doc = content.document;
    let savePasswordCheckBox = doc.getElementById("savePasswords");
    Assert.ok(!savePasswordCheckBox.checked,
              "Save Password CheckBox should be unchecked by default");
    savePasswordCheckBox.click();

    let showPasswordsButton = doc.getElementById("showPasswords");
    showPasswordsButton.click();
  });

  passwordsDialog = await dialogOpened;

  // check telemetry events while we are in here
  TelemetryTestUtils.assertEvents(
    [["pwmgr", "open_management", "preferences"]],
    {category: "pwmgr", method: "open_management"});
});

add_task(async function test_deletePasswordWithKey() {
  let doc = passwordsDialog.document;

  let tree = doc.getElementById("signonsTree");
  Assert.equal(tree.view.rowCount, 1, "Row count should initially be 1");
  tree.focus();
  tree.view.selection.select(0);

  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("KEY_Backspace");
  } else {
    EventUtils.synthesizeKey("KEY_Delete");
  }

  await TestUtils.waitForCondition(() => tree.view.rowCount == 0);

  is_element_visible(content.gSubDialog._dialogs[0]._box,
    "Subdialog is visible after deleting an element");
});

add_task(async function subdialog_cleanup() {
  Services.telemetry.clearEvents();
  // Undo the save password change.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let doc = content.document;
    let savePasswordCheckBox = doc.getElementById("savePasswords");
    if (savePasswordCheckBox.checked) {
      savePasswordCheckBox.click();
    }
  });
  gBrowser.removeCurrentTab();
});

add_task(async function test_openPasswordManagement_overrideURI() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["signon.management.page.enabled", true],
  ]});

  Services.prefs.setStringPref(PREF_MANAGEMENT_URI, "about:logins?filter=%DOMAIN%");
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});

  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:logins?filter=");

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let doc = content.document;
    let showPasswordsButton = doc.getElementById("showPasswords");
    showPasswordsButton.click();
  });

  let tab = await tabOpenPromise;
  ok(tab, "Tab opened");
  BrowserTestUtils.removeTab(tab);
  Services.prefs.clearUserPref(PREF_MANAGEMENT_URI);
  gBrowser.removeCurrentTab();
});
