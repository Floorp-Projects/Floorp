/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const baseURL = "http://example.com/browser/browser/base/content/test/general/";

function test() {
  waitForExplicitFinish();
  Task.spawn(function* () {
    // Enable the popup blocker.
    yield pushPrefs(["dom.disable_open_during_load", true]);

    // Open the test page.
    let tab = gBrowser.loadOneTab(baseURL + "popup_blocker.html", { inBackground: false });
    yield promiseTabLoaded(tab);

    // Wait for the popup-blocked notification.
    let notification;
    yield promiseWaitForCondition(() =>
      notification = gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"));

    // Show the menu.
    let popupShown = promiseWaitForEvent(window, "popupshown");
    let popupFilled = BrowserTestUtils.waitForMessage(gBrowser.selectedBrowser.messageManager,
                                                      "PopupBlocking:ReplyGetBlockedPopupList");
    notification.querySelector("button").doCommand();
    let popup_event = yield popupShown;
    let menu = popup_event.target;
    is(menu.id, "blockedPopupOptions", "Blocked popup menu shown");

    yield popupFilled;
    // The menu is filled on the same message that we waited for, so let's ensure that it
    // had a chance of running before this test code.
    yield new Promise(resolve => executeSoon(resolve));

    // Check the menu contents.
    let sep = menu.querySelector("menuseparator");
    let popupCount = 0;
    for (let i = sep.nextElementSibling; i; i = i.nextElementSibling) {
      popupCount++;
    }
    is(popupCount, 2, "Two popups were blocked");

    // Pressing "allow" should open all blocked popups.
    let popupTabs = [];
    function onTabOpen(event) {
      popupTabs.push(event.target);
    }
    gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen);

    // Press the button.
    let allow = menu.querySelector("[observes='blockedPopupAllowSite']");
    allow.doCommand();
    yield promiseWaitForCondition(() =>
      popupTabs.length == 2 &&
      popupTabs.every(tab => tab.linkedBrowser.currentURI.spec != "about:blank"));

    gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);

    is(popupTabs[0].linkedBrowser.currentURI.spec, "data:text/plain;charset=utf-8,a", "Popup a");
    is(popupTabs[1].linkedBrowser.currentURI.spec, "data:text/plain;charset=utf-8,b", "Popup b");

    // Clean up.
    gBrowser.removeTab(tab);
    for (let popup of popupTabs) {
      gBrowser.removeTab(popup);
    }
    clearAllPermissionsByPrefix("popup");
    finish();
  });
}
