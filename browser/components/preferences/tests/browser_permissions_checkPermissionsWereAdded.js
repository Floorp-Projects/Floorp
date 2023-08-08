/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PERMISSIONS_URL =
  "chrome://browser/content/preferences/dialogs/permissions.xhtml";

const _checkAndOpenCookiesDialog = async doc => {
  let cookieExceptionsButton = doc.getElementById("cookieExceptions");
  ok(cookieExceptionsButton, "cookieExceptionsButton found");
  let dialogPromise = promiseLoadSubDialog(PERMISSIONS_URL);
  cookieExceptionsButton.click();
  let dialog = await dialogPromise;
  return dialog;
};

const _checkCookiesDialog = (dialog, buttonIds) => {
  ok(dialog, "dialog loaded");
  let urlLabel = dialog.document.getElementById("urlLabel");
  ok(!urlLabel.hidden, "urlLabel should be visible");
  let url = dialog.document.getElementById("url");
  ok(!url.hidden, "url should be visible");
  for (let buttonId of buttonIds) {
    let buttonDialog = dialog.document.getElementById(buttonId);
    ok(buttonDialog, "blockButtonDialog found");
    is(
      buttonDialog.hasAttribute("disabled"),
      true,
      "If the user hasn't added an url the button shouldn't be clickable"
    );
  }
  return dialog;
};

const _addWebsiteAddressToPermissionBox = (
  websiteAddress,
  dialog,
  buttonId,
  expectError
) => {
  let url = dialog.document.getElementById("url");
  let buttonDialog = dialog.document.getElementById(buttonId);
  url.value = websiteAddress;
  url.dispatchEvent(new Event("input", { bubbles: true }));
  is(
    buttonDialog.hasAttribute("disabled"),
    false,
    "When the user add an url the button should be clickable"
  );
  buttonDialog.click();
  let permissionsBox = dialog.document.getElementById("permissionsBox");
  let children = permissionsBox.getElementsByAttribute("origin", "*");
  is(
    !!children.length,
    !expectError,
    "Website added in url should be in the list"
  );
};

const _checkIfPermissionsWereAdded = (dialog, expectedResult) => {
  let permissionsBox = dialog.document.getElementById("permissionsBox");
  for (let website of expectedResult) {
    let elements = permissionsBox.getElementsByAttribute("origin", website);
    is(elements.length, 1, "It should find only one coincidence");
  }
};

const _removesAllSitesInPermissionBox = (dialog, shouldBePossible) => {
  let removeAllWebsitesButton = dialog.document.getElementById(
    "removeAllPermissions"
  );
  ok(removeAllWebsitesButton, "removeAllWebsitesButton found");
  const isDisabled = removeAllWebsitesButton.hasAttribute("disabled");
  is(
    !removeAllWebsitesButton.hasAttribute("disabled"),
    shouldBePossible,
    "There should be websites in the list"
  );
  if (!isDisabled) {
    removeAllWebsitesButton.click();
  }
};

add_task(async function checkCookiePermissions() {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = win.document;
  let buttonIds = ["btnBlock", "btnCookieSession", "btnAllow"];

  let dialog = await _checkAndOpenCookiesDialog(doc);
  _checkCookiesDialog(dialog, buttonIds);

  let tests = [
    {
      inputWebsite: "google.com",
      expectedResult: ["http://google.com", "https://google.com"],
    },
    {
      inputWebsite: "https://google.com",
      expectedResult: ["https://google.com"],
    },
    {
      inputWebsite: "s3.eu-central-1.amazonaws.com",
      expectedResult: [
        "http://s3.eu-central-1.amazonaws.com",
        "https://s3.eu-central-1.amazonaws.com",
      ],
    },
    {
      inputWebsite: "file://",
      expectedResult: ["file:///"],
    },
    {
      inputWebsite: "about:config",
      expectedResult: ["about:config"],
    },
    // Invalid inputs, expected to not be added
    {
      inputWebsite: "http://",
      expectedResult: [],
    },
    {
      inputWebsite: "*.example.com",
      expectedResult: [],
    },
    {
      inputWebsite: "http://*.example.com",
      expectedResult: [],
    },
  ];

  for (let buttonId of buttonIds) {
    for (let test of tests) {
      _addWebsiteAddressToPermissionBox(
        test.inputWebsite,
        dialog,
        buttonId,
        test.expectedResult.length === 0
      );
      _checkIfPermissionsWereAdded(dialog, test.expectedResult);
      _removesAllSitesInPermissionBox(dialog, !!test.expectedResult.length);
    }
  }

  gBrowser.removeCurrentTab();
});
