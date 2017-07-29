"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

const COOKIES_URL = "chrome://browser/content/preferences/cookies.xul";

const URI = Services.io.newURI("http://www.example.com");
var cookiesDialog;

add_task(async function openCookiesSubDialog() {
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});

  let dialogOpened = promiseLoadSubDialog(COOKIES_URL);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let doc = content.document;
    let cookiesButton = doc.getElementById("historyRememberCookies");
    cookiesButton.click();
  });

  cookiesDialog = await dialogOpened;
});

add_task(async function testDeleteCookie() {
  let doc = cookiesDialog.document;

  // Add a cookie.
  Services.cookies.add(URI.host, URI.path, "", "", false, false, true, Date.now());

  let tree = doc.getElementById("cookiesList");
  Assert.equal(tree.view.rowCount, 1, "Row count should initially be 1");
  tree.focus();
  tree.view.selection.select(0);

  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
  } else {
    EventUtils.synthesizeKey("VK_DELETE", {});
  }

  await waitForCondition(() => tree.view.rowCount == 0);

  is_element_visible(content.gSubDialog._dialogs[0]._box,
    "Subdialog is visible after deleting an element");

});

add_task(async function removeTab() {
  gBrowser.removeCurrentTab();
});
