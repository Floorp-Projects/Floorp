/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

function clearAllPermissionsByPrefix(aPrefix) {
  for (let perm of Services.perms.all) {
    if (perm.type.startsWith(aPrefix)) {
      Services.perms.removePermission(perm);
    }
  }
}

add_setup(async function () {
  // Enable the popup blocker.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.disable_open_during_load", true]],
  });
});

// Tests that we show a special message when popup blocking exceeds
// a certain maximum of popups per page.
add_task(async function test_maximum_reported_blocks() {
  Services.prefs.setIntPref("privacy.popups.maxReported", 5);

  // Open the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    baseURL + "popup_blocker_10_popups.html"
  );

  // Wait for the popup-blocked notification.
  let notification = await TestUtils.waitForCondition(() =>
    gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked")
  );

  // Slightly hacky way to ensure we show the correct message in this case.
  ok(
    notification.messageText.textContent.includes("more than"),
    "Notification label has 'more than'"
  );
  ok(
    notification.messageText.textContent.includes("5"),
    "Notification label shows the maximum number of popups"
  );

  gBrowser.removeTab(tab);

  Services.prefs.clearUserPref("privacy.popups.maxReported");
});

add_task(async function test_opening_blocked_popups() {
  // Open the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    baseURL + "popup_blocker.html"
  );

  await testPopupBlockingToolbar(tab);
});

add_task(async function test_opening_blocked_popups_privateWindow() {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  // Open the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    baseURL + "popup_blocker.html"
  );
  await testPopupBlockingToolbar(tab);
  await BrowserTestUtils.closeWindow(win);
});

async function testPopupBlockingToolbar(tab) {
  let win = tab.ownerGlobal;
  // Wait for the popup-blocked notification.
  let notification;
  await TestUtils.waitForCondition(
    () =>
      (notification = win.gBrowser
        .getNotificationBox()
        .getNotificationWithValue("popup-blocked"))
  );

  // Show the menu.
  let popupShown = BrowserTestUtils.waitForEvent(win, "popupshown");
  let popupFilled = waitForBlockedPopups(2, {
    doc: win.document,
  });
  EventUtils.synthesizeMouseAtCenter(
    notification.buttonContainer.querySelector("button"),
    {},
    win
  );
  let popup_event = await popupShown;
  let menu = popup_event.target;
  is(menu.id, "blockedPopupOptions", "Blocked popup menu shown");

  await popupFilled;

  // Pressing "allow" should open all blocked popups.
  let popupTabs = [];
  function onTabOpen(event) {
    popupTabs.push(event.target);
  }
  win.gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen);

  // Press the button.
  let allow = win.document.getElementById("blockedPopupAllowSite");
  allow.doCommand();
  await TestUtils.waitForCondition(
    () =>
      popupTabs.length == 2 &&
      popupTabs.every(
        aTab => aTab.linkedBrowser.currentURI.spec != "about:blank"
      )
  );

  win.gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);

  ok(
    popupTabs[0].linkedBrowser.currentURI.spec.endsWith("popup_blocker_a.html"),
    "Popup a"
  );
  ok(
    popupTabs[1].linkedBrowser.currentURI.spec.endsWith("popup_blocker_b.html"),
    "Popup b"
  );

  let popupPerms = Services.perms.getAllByTypeSince("popup", 0);
  is(popupPerms.length, 1, "One popup permission added");
  let popupPerm = popupPerms[0];
  let expectedExpireType = PrivateBrowsingUtils.isWindowPrivate(win)
    ? Services.perms.EXPIRE_SESSION
    : Services.perms.EXPIRE_NEVER;
  is(
    popupPerm.expireType,
    expectedExpireType,
    "Check expireType is appropriate for the window"
  );

  // Clean up.
  win.gBrowser.removeTab(tab);
  for (let popup of popupTabs) {
    win.gBrowser.removeTab(popup);
  }
  clearAllPermissionsByPrefix("popup");
  // Ensure the menu closes.
  menu.hidePopup();
}
