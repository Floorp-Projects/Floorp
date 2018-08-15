/*
 * Test that a blocked request to autoplay media is shown to the user
 */

const AUTOPLAY_PAGE  = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "browser_autoplay_blocked.html";

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

add_task(async function testMainViewVisible() {

  Services.prefs.setIntPref("media.autoplay.default", Ci.nsIAutoplay.ALLOWED);

  await BrowserTestUtils.withNewTab(AUTOPLAY_PAGE, async function() {
    let permissionsList = document.getElementById("identity-popup-permission-list");
    let emptyLabel = permissionsList.nextSibling.nextSibling;

    ok(BrowserTestUtils.is_hidden(autoplayBlockedIcon()), "Blocked icon not shown");

    await openIdentityPopup();
    ok(!BrowserTestUtils.is_hidden(emptyLabel), "List of permissions is empty");
    await closeIdentityPopup();
  });

  Services.prefs.setIntPref("media.autoplay.default", Ci.nsIAutoplay.BLOCKED);

  await BrowserTestUtils.withNewTab(AUTOPLAY_PAGE, async function() {
    let permissionsList = document.getElementById("identity-popup-permission-list");
    let emptyLabel = permissionsList.nextSibling.nextSibling;

    ok(!BrowserTestUtils.is_hidden(autoplayBlockedIcon()), "Blocked icon is shown");

    await openIdentityPopup();
    ok(BrowserTestUtils.is_hidden(emptyLabel), "List of permissions is not empty");
    let labelText = SitePermissions.getPermissionLabel("autoplay-media");
    let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].textContent, labelText, "Correct value");
    await closeIdentityPopup();
  });

  Services.prefs.clearUserPref("media.autoplay.default");
});
