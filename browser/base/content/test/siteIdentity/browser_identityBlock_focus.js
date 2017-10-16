/* Tests that the identity block can be reached via keyboard
 * shortcuts and that it has the correct tab order.
 */

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com");
const PERMISSIONS_PAGE = TEST_PATH + "permissions.html";

function synthesizeKeyAndWaitForFocus(element, keyCode, options) {
  let focused = BrowserTestUtils.waitForEvent(element, "focus");
  EventUtils.synthesizeKey(keyCode, options);
  return focused;
}

// Checks that the identity block is the next element after the urlbar
// to be focused if there are no active notification anchors.
add_task(async function testWithoutNotifications() {
  await SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    await synthesizeKeyAndWaitForFocus(gURLBar, "l", {accelKey: true});
    is(document.activeElement, gURLBar.inputField, "urlbar should be focused");
    await synthesizeKeyAndWaitForFocus(gIdentityHandler._identityBox, "VK_TAB", {shiftKey: true});
    is(document.activeElement, gIdentityHandler._identityBox,
       "identity block should be focused");
  });
});

// Checks that when there is a notification anchor, it will receive
// focus before the identity block.
add_task(async function testWithoutNotifications() {
  await SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    // Request a permission;
    BrowserTestUtils.synthesizeMouseAtCenter("#geo", {}, browser);
    await popupshown;

    await synthesizeKeyAndWaitForFocus(gURLBar, "l", {accelKey: true});
    is(document.activeElement, gURLBar.inputField, "urlbar should be focused");
    let geoIcon = document.getElementById("geo-notification-icon");
    await synthesizeKeyAndWaitForFocus(geoIcon, "VK_TAB", {shiftKey: true});
    is(document.activeElement, geoIcon, "notification anchor should be focused");
    await synthesizeKeyAndWaitForFocus(gIdentityHandler._identityBox, "VK_TAB", {shiftKey: true});
    is(document.activeElement, gIdentityHandler._identityBox,
       "identity block should be focused");
  });
});

// Checks that with invalid pageproxystate the identity block is ignored.
add_task(async function testInvalidPageProxyState() {
  await SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    // Loading about:blank will automatically focus the urlbar, which, however, can
    // race with the test code. So we only send the shortcut if the urlbar isn't focused yet.
    if (document.activeElement != gURLBar.inputField) {
      await synthesizeKeyAndWaitForFocus(gURLBar, "l", {accelKey: true});
    }
    is(document.activeElement, gURLBar.inputField, "urlbar should be focused");
    await synthesizeKeyAndWaitForFocus(gBrowser.getTabForBrowser(browser), "VK_TAB", {shiftKey: true});
    isnot(document.activeElement, gIdentityHandler._identityBox,
          "identity block should not be focused");
    // Restore focus to the url bar.
    gURLBar.focus();
  });
});
