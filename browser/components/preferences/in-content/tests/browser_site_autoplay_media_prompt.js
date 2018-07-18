"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm");

const URL = "http://www.example.com";
const PRINCIPAL = Services.scriptSecurityManager
  .createCodebasePrincipal(Services.io.newURI(URL), {});

const PERMISSIONS_URL = "chrome://browser/content/preferences/permissions.xul";
const AUTOPLAY_ENABLED_KEY = "media.autoplay.default";
const GESTURES_NEEDED_KEY = "media.autoplay.enabled.user-gestures-needed";
const ASK_PERMISSIONS_KEY = "media.autoplay.enabled.ask-permissions";

var exceptionsDialog;

Services.prefs.setIntPref(AUTOPLAY_ENABLED_KEY, Ci.nsIAutoplay.ALLOWED);
Services.prefs.setBoolPref(GESTURES_NEEDED_KEY, false);
Services.prefs.setBoolPref(ASK_PERMISSIONS_KEY, true);

async function openExceptionsDialog() {
  let dialogOpened = promiseLoadSubDialog(PERMISSIONS_URL);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let exceptionsButton = content.document.getElementById("autoplayMediaPolicyButton");
    exceptionsButton.click();
  });
  exceptionsDialog = await dialogOpened;
}

add_task(async function ensureMenuHidden() {

  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref(AUTOPLAY_ENABLED_KEY);
    Services.prefs.clearUserPref(GESTURES_NEEDED_KEY);
    Services.prefs.clearUserPref(ASK_PERMISSIONS_KEY);
    gBrowser.removeCurrentTab();
  });

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let win = gBrowser.selectedBrowser.contentWindow;
  is_element_hidden(win.document.getElementById("autoplayMediaPolicy"),
                    "Ensure checkbox is hidden when preffed off");
});

add_task(async function enableBlockingAutoplay() {

  Services.prefs.setBoolPref(GESTURES_NEEDED_KEY, true);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let doc = content.document;
    let autoplayMenu = doc.getElementById("autoplayMediaPolicyMenu");
    autoplayMenu.click();
    let askMenuItem = autoplayMenu.childNodes[0].childNodes[1];
    askMenuItem.click();
  });

  Assert.equal(Services.prefs.getIntPref(AUTOPLAY_ENABLED_KEY),
               Ci.nsIAutoplay.PROMPT,
               "Ensure we have set autoplay to false");
});

add_task(async function addException() {
  await openExceptionsDialog();
  let doc = exceptionsDialog.document;

  let richlistbox = doc.getElementById("permissionsBox");
  Assert.equal(richlistbox.itemCount, 0, "Row count should initially be 0");

  let inputBox = doc.getElementById("url");
  inputBox.focus();

  EventUtils.sendString(URL, exceptionsDialog);

  let btnAllow = doc.getElementById("btnAllow");
  btnAllow.click();

  await TestUtils.waitForCondition(() => richlistbox.itemCount == 1);
  Assert.equal(richlistbox.getItemAtIndex(0).getAttribute("origin"), URL);

  let permChanged = TestUtils.topicObserved("perm-changed");
  let btnApplyChanges = doc.getElementById("btnApplyChanges");
  btnApplyChanges.click();
  await permChanged;

  is(Services.perms.testPermissionFromPrincipal(PRINCIPAL, "autoplay-media"),
     Ci.nsIPermissionManager.ALLOW_ACTION, "Correctly added the exception");
});

add_task(async function deleteException() {
  await openExceptionsDialog();
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

  let permChanged = TestUtils.topicObserved("perm-changed");
  let btnApplyChanges = doc.getElementById("btnApplyChanges");
  btnApplyChanges.click();
  await permChanged;

  is(Services.perms.testPermissionFromPrincipal(PRINCIPAL, "autoplay-media"),
     Ci.nsIPermissionManager.UNKNOWN_ACTION, "Correctly removed the exception");
});
