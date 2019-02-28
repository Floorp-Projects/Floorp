/*
 * Test that a blocked request to autoplay media is shown to the user
 */

const AUTOPLAY_PAGE  = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "browser_autoplay_blocked.html";

const AUTOPLAY_PREF = "media.autoplay.default";
const AUTOPLAY_PERM = "autoplay-media";

function openIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
  gIdentityHandler._identityBox.click();
  return promise;
}

function closeIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
  gIdentityHandler._identityPopup.hidePopup();
  return promise;
}

function autoplayBlockedIcon() {
  return document.querySelector("#blocked-permissions-container " +
                                ".blocked-permission-icon.autoplay-media-icon");
}

async function blockedIconShown(browser) {
  // May need to wait for `GloballyAutoplayBlocked` event before showing icon.
  if (BrowserTestUtils.is_hidden(autoplayBlockedIcon())) {
    await BrowserTestUtils.waitForEvent(browser, "GloballyAutoplayBlocked");
  }
  ok(!BrowserTestUtils.is_hidden(autoplayBlockedIcon()), "Blocked icon is shown");
}

add_task(async function setup() {
  registerCleanupFunction(() => {
    Services.perms.removeAll();
    Services.prefs.clearUserPref(AUTOPLAY_PREF);
  });
});

add_task(async function testMainViewVisible() {

  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.ALLOWED);

  await BrowserTestUtils.withNewTab(AUTOPLAY_PAGE, async function() {
    let permissionsList = document.getElementById("identity-popup-permission-list");
    let emptyLabel = permissionsList.nextElementSibling.nextElementSibling;

    ok(BrowserTestUtils.is_hidden(autoplayBlockedIcon()), "Blocked icon not shown");

    await openIdentityPopup();
    ok(!BrowserTestUtils.is_hidden(emptyLabel), "List of permissions is empty");
    await closeIdentityPopup();
  });

  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  await BrowserTestUtils.withNewTab(AUTOPLAY_PAGE, async function(browser) {
    let permissionsList = document.getElementById("identity-popup-permission-list");
    let emptyLabel = permissionsList.nextElementSibling.nextElementSibling;

    await blockedIconShown(browser);

    await openIdentityPopup();
    ok(BrowserTestUtils.is_hidden(emptyLabel), "List of permissions is not empty");
    let labelText = SitePermissions.getPermissionLabel(AUTOPLAY_PERM);
    let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].textContent, labelText, "Correct value");

    let menulist = document.getElementById("identity-popup-popup-menulist");
    Assert.equal(menulist.label, "Block");

    await EventUtils.synthesizeMouseAtCenter(menulist, { type: "mousedown" });
    await BrowserTestUtils.waitForCondition(() => {
      return menulist.getElementsByTagName("menuitem")[0].label === "Allow";
    });

    let menuitem = menulist.getElementsByTagName("menuitem")[0];
    Assert.equal(menuitem.getAttribute("label"), "Allow");

    menuitem.click();
    menulist.menupopup.hidePopup();
    await closeIdentityPopup();

    let uri = Services.io.newURI(AUTOPLAY_PAGE);
    let state = SitePermissions.get(uri, AUTOPLAY_PERM).state;
    Assert.equal(state, SitePermissions.ALLOW);
  });

  Services.perms.removeAll();
});

add_task(async function testGloballyBlockedOnNewWindow() {
  Services.prefs.setIntPref(AUTOPLAY_PREF, Ci.nsIAutoplay.BLOCKED);

  let uri = Services.io.newURI(AUTOPLAY_PAGE);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri.spec);
  await blockedIconShown(tab.linkedBrowser);

  Assert.deepEqual(SitePermissions.get(uri, AUTOPLAY_PERM, tab.linkedBrowser), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  let promiseWin = BrowserTestUtils.waitForNewWindow();
  gBrowser.replaceTabWithWindow(tab);
  let win = await promiseWin;
  tab = win.gBrowser.selectedTab;

  Assert.deepEqual(SitePermissions.get(uri, AUTOPLAY_PERM, tab.linkedBrowser), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  SitePermissions.remove(uri, AUTOPLAY_PERM, tab.linkedBrowser);
  await BrowserTestUtils.closeWindow(win);
});
