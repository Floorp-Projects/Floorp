"use strict";

ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm", this);

var passwordsDialog;

add_task(async function test_openPasswordManagement() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:logins");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    let doc = content.document;

    let savePasswordCheckBox = doc.getElementById("savePasswords");
    Assert.ok(
      !savePasswordCheckBox.checked,
      "Save Password CheckBox should be unchecked by default"
    );

    let showPasswordsButton = doc.getElementById("showPasswords");
    showPasswordsButton.click();
  });

  let tab = await tabOpenPromise;
  ok(tab, "Tab opened");

  // check telemetry events while we are in here
  await LoginTestUtils.telemetry.waitForEventCount(1);
  TelemetryTestUtils.assertEvents(
    [["pwmgr", "open_management", "preferences"]],
    { category: "pwmgr", method: "open_management" },
    { clear: true, process: "content" }
  );

  BrowserTestUtils.removeTab(tab);
  gBrowser.removeCurrentTab();
});
