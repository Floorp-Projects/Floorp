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

  let tree = doc.getElementById("permissionsTree");
  Assert.equal(tree.view.rowCount, 0, "Row count should initially be 0");

  let inputBox = doc.getElementById("url");
  inputBox.focus();

  EventUtils.sendString("www.example.com", exceptionsDialog);

  let btnBlock = doc.getElementById("btnBlock");
  btnBlock.click();

  await TestUtils.waitForCondition(() => tree.view.rowCount == 1);

  Assert.equal(tree.view.getCellText(0, tree.treeBoxObject.columns.getColumnAt(0)),
               "http://www.example.com");
});

add_task(async function deleteALoginException() {
  let doc = exceptionsDialog.document;

  let tree = doc.getElementById("permissionsTree");
  Assert.equal(tree.view.rowCount, 1, "Row count should initially be 1");
  tree.focus();
  tree.view.selection.select(0);

  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
  } else {
    EventUtils.synthesizeKey("VK_DELETE", {});
  }

  await TestUtils.waitForCondition(() => tree.view.rowCount == 0);

  // eslint-disable-next-line mozilla/no-cpows-in-tests
  is_element_visible(content.gSubDialog._dialogs[0]._box,
    "Subdialog is visible after deleting an element");
});
