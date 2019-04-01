/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PERMISSIONS_URL = "chrome://browser/content/preferences/permissions.xul";

async function openCookiesDialog(doc) {
  let cookieExceptionsButton = doc.getElementById("cookieExceptions");
  ok(cookieExceptionsButton, "cookieExceptionsButton found");
  let dialogPromise = promiseLoadSubDialog(PERMISSIONS_URL);
  cookieExceptionsButton.click();
  let dialog = await dialogPromise;
  return dialog;
}

function checkCookiesDialog(dialog) {
  ok(dialog, "dialog loaded");
  let buttonIds = ["removePermission", "removeAllPermissions", "btnApplyChanges"];

  for (let buttonId of buttonIds) {
    let button = dialog.document.getElementById(buttonId);
    ok(button, `${buttonId} found`);
  }

  let cancelButton =
    dialog.document.getElementsByClassName("actionButtons")[1].children[0];

  is(cancelButton.getAttribute("label"), "Cancel", "cancelButton found");
}

function addNewPermission(websiteAddress, dialog) {
  let url = dialog.document.getElementById("url");
  let buttonDialog = dialog.document.getElementById("btnBlock");
  let permissionsBox = dialog.document.getElementById("permissionsBox");
  let currentPermissions = permissionsBox.itemCount;

  url.value = websiteAddress;
  url.dispatchEvent(new Event("input", {bubbles: true}));
  is(buttonDialog.hasAttribute("disabled"), false,
     "When the user add an url the button should be clickable");
  buttonDialog.click();

  is(permissionsBox.itemCount, currentPermissions + 1,
     "Website added in url should be in the list");
}

async function cleanList(dialog) {
  let removeAllButton = dialog.document.getElementById("removeAllPermissions");
  if (!removeAllButton.hasAttribute("disabled")) removeAllButton.click();
}

function addData(websites, dialog) {
  for (let website of websites) {
    addNewPermission(website, dialog);
  }
}

function deletePermission(permission, dialog) {
  let permissionsBox = dialog.document.getElementById("permissionsBox");
  let elements = permissionsBox.getElementsByAttribute("origin", permission);
  is(elements.length, 1, "It should find only one entry");
  permissionsBox.selectItem(elements[0]);
  let removePermissionButton = dialog.document.getElementById("removePermission");
  is(removePermissionButton.hasAttribute("disabled"), false,
     "The button should be clickable to remove selected item");
  removePermissionButton.click();
}

function save(dialog) {
  let saveButton = dialog.document.getElementById("btnApplyChanges");
  saveButton.click();
}

function cancel(dialog) {
  let cancelButton =
    dialog.document.getElementsByClassName("actionButtons")[1].children[0];
  is(cancelButton.getAttribute("label"), "Cancel", "cancelButton found");
  cancelButton.click();
}

async function checkExpected(expected, doc) {
  let dialog = await openCookiesDialog(doc);
  let permissionsBox = dialog.document.getElementById("permissionsBox");

  is(permissionsBox.itemCount, expected.length,
     `There should be ${expected.length} elements in the list`);

  for (let website of expected) {
    let elements = permissionsBox.getElementsByAttribute("origin", website);
    is(elements.length, 1, "It should find only one entry");
  }
  return dialog;
}

async function runTest(test, websites, doc) {
  let dialog = await openCookiesDialog(doc);
  checkCookiesDialog(dialog);

  if (test.needPreviousData) {
    addData(websites, dialog);
    save(dialog);
    dialog = await openCookiesDialog(doc);
  }

  for (let step of test.steps) {
    switch (step) {
      case "addNewPermission":
        addNewPermission(test.newData, dialog);
        break;
      case "deletePermission":
        deletePermission(test.newData, dialog);
        break;
      case "deleteAllPermission":
        await cleanList(dialog);
        break;
      case "save":
        save(dialog);
        break;
      case "cancel":
        cancel(dialog);
        break;
      case "openPane":
        dialog = await openCookiesDialog(doc);
        break;
      default:
        // code block
    }
  }
  dialog = await checkExpected(test.expected, doc);
  await cleanList(dialog);
  save(dialog);
}

add_task(async function checkPermissions() {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true});
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = win.document;
  let websites = ["http://test1.com", "http://test2.com"];

  let tests = [{
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "save"],
    "expected": ["https://mytest.com"], // when open the pane again it should find this in the list
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "cancel"],
    "expected": [],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "deletePermission", "save"],
    "expected": [],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "deletePermission", "cancel"],
    "expected": [],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "save", "openPane", "deletePermission", "save"],
    "expected": [],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "save", "openPane", "deletePermission", "cancel"],
    "expected": ["https://mytest.com"],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "deleteAllPermission", "save"],
    "expected": [],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "deleteAllPermission", "cancel"],
    "expected": [],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "save", "openPane", "deleteAllPermission", "save"],
    "expected": [],
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "save", "openPane", "deleteAllPermission", "cancel"],
    "expected": ["https://mytest.com"],
  },
  {
    "needPreviousData": true,
    "newData": "https://mytest.com",
    "steps": ["deleteAllPermission", "save"],
    "expected": [],
  },
  {
    "needPreviousData": true,
    "newData": "https://mytest.com",
    "steps": ["deleteAllPermission", "cancel"],
    "expected": websites,
  },
  {
    "needPreviousData": true,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "save"],
    "expected": (function() {
      let result = websites.slice();
      result.push("https://mytest.com");
      return result;
    }()),
  },
  {
    "needPreviousData": true,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "cancel"],
    "expected": websites,
  },
  {
    "needPreviousData": false,
    "newData": "https://mytest.com",
    "steps": ["addNewPermission", "save", "openPane", "deleteAllPermission", "addNewPermission",  "save"],
    "expected": ["https://mytest.com"],
  }];

  for (let test of tests) {
    await runTest(test, websites, doc);
  }

  gBrowser.removeCurrentTab();
});
