"use strict";
const PERMISSIONS_URL = "chrome://browser/content/preferences/permissions.xul";

var exceptionsDialog;

add_task(async function openLoginExceptionsSubDialog() {
  // Undo the save password change.
  registerCleanupFunction(async function() {
    await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
      let doc = content.document;
      let savePasswordCheckBox = doc.getElementById("savePasswords");
      if (savePasswordCheckBox.checked) {
        savePasswordCheckBox.click();
      }
    });

    gBrowser.removeCurrentTab();
  });

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});

  let dialogOpened = promiseLoadSubDialog(PERMISSIONS_URL);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let doc = content.document;
    let savePasswordCheckBox = doc.getElementById("savePasswords");
    Assert.ok(!savePasswordCheckBox.checked,
              "Save Password CheckBox should be unchecked by default");
    savePasswordCheckBox.click();

    let loginExceptionsButton = doc.getElementById("passwordExceptions");
    loginExceptionsButton.click();
  });

  exceptionsDialog = await dialogOpened;
});

add_task(async function addALoginException() {
  let doc = exceptionsDialog.document;

  let richlistbox = doc.getElementById("permissionsBox");
  Assert.equal(richlistbox.itemCount, 0, "Row count should initially be 0");

  let inputBox = doc.getElementById("url");
  inputBox.focus();

  EventUtils.sendString("www.example.com", exceptionsDialog);

  let btnBlock = doc.getElementById("btnBlock");
  btnBlock.click();

  await TestUtils.waitForCondition(() => richlistbox.itemCount == 1);

  Assert.equal(richlistbox.getItemAtIndex(0).getAttribute("origin"),
               "http://www.example.com");
});

add_task(async function deleteALoginException() {
  let doc = exceptionsDialog.document;

  let richlistbox = doc.getElementById("permissionsBox");
  Assert.equal(richlistbox.itemCount, 1, "Row count should initially be 1");
  richlistbox.focus();
  richlistbox.selectedIndex = 0;

  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("KEY_Backspace");
  } else {
    EventUtils.synthesizeKey("KEY_Delete");
  }

  await TestUtils.waitForCondition(() => richlistbox.itemCount == 0);

  is_element_visible(content.gSubDialog._dialogs[0]._box,
    "Subdialog is visible after deleting an element");
});
