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
add_task(function* testWithoutNotifications() {
  yield SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  yield BrowserTestUtils.withNewTab("https://example.com", function*() {
    yield synthesizeKeyAndWaitForFocus(gURLBar, "l", {accelKey: true})
    is(document.activeElement, gURLBar.inputField, "urlbar should be focused");
    yield synthesizeKeyAndWaitForFocus(gIdentityHandler._identityBox, "VK_TAB", {shiftKey: true})
    is(document.activeElement, gIdentityHandler._identityBox,
       "identity block should be focused");
  });
});

// Checks that when there is a notification anchor, it will receive
// focus before the identity block.
add_task(function* testWithoutNotifications() {
  yield SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  yield BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function*(browser) {
    let popupshown = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    // Request a permission;
    BrowserTestUtils.synthesizeMouseAtCenter("#geo", {}, browser);
    yield popupshown;

    yield synthesizeKeyAndWaitForFocus(gURLBar, "l", {accelKey: true})
    is(document.activeElement, gURLBar.inputField, "urlbar should be focused");
    let geoIcon = document.getElementById("geo-notification-icon");
    yield synthesizeKeyAndWaitForFocus(geoIcon, "VK_TAB", {shiftKey: true})
    is(document.activeElement, geoIcon, "notification anchor should be focused");
    yield synthesizeKeyAndWaitForFocus(gIdentityHandler._identityBox, "VK_TAB", {shiftKey: true})
    is(document.activeElement, gIdentityHandler._identityBox,
       "identity block should be focused");
  });
});

// Checks that with invalid pageproxystate the identity block is ignored.
add_task(function* testInvalidPageProxyState() {
  yield SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
  yield BrowserTestUtils.withNewTab("about:blank", function*(browser) {
    // Loading about:blank will automatically focus the urlbar, which, however, can
    // race with the test code. So we only send the shortcut if the urlbar isn't focused yet.
    if (document.activeElement != gURLBar.inputField) {
      yield synthesizeKeyAndWaitForFocus(gURLBar, "l", {accelKey: true})
    }
    is(document.activeElement, gURLBar.inputField, "urlbar should be focused");
    yield synthesizeKeyAndWaitForFocus(gBrowser.getTabForBrowser(browser), "VK_TAB", {shiftKey: true})
    isnot(document.activeElement, gIdentityHandler._identityBox,
          "identity block should not be focused");
    // Restore focus to the url bar.
    gURLBar.focus();
  });
});
