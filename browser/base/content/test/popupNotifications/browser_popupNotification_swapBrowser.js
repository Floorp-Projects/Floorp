/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_SECURITY_DELAY = 1;
const TEST_WINDOW_TIME_OPEN = 100;

add_setup(async function () {
  // Set the lowest non-zero security delay for PopupNotification actions so we can test
  // if the delay causes inability to press the buttons on PopupNotifications when windows
  // are swapped
  // Geo Timeout Pref is set to 0, to ensure that the test does not wait location provider
  // to start
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.notification_enable_delay", TEST_SECURITY_DELAY],
      ["geo.timeout", 0],
    ],
  });
});

// Function to check if Popup Notifications has been shown
function waitForNotificationPanel(window, callback) {
  let listener = event => {
    if (event.target != window.PopupNotifications.panel) {
      return;
    }
    window.PopupNotifications.panel.removeEventListener("popupshown", listener);
    gActiveListeners.delete(listener);
    executeSoon(() => callback.call(window.PopupNotifications.panel));
  };
  gActiveListeners.set(listener, "popupshown");
  window.PopupNotifications.panel.addEventListener("popupshown", listener);
}

// Testing that the Popup Notification security delay is compared to the
// global process timer instead of the window specific process timer
// as if it is based on the window specific process timer and the tab
// with the Popup Notification is moved to another window with a lower
// window specific process counter, it would block the user from interacting
// with the buttons on the panel because of the security delay
add_task(async function transferPopupNotificationToNewWindowAndResolve() {
  await ensureSecurityDelayReady();

  let window = await BrowserTestUtils.openNewBrowserWindow();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "https://test1.example.com/"
  );

  let browser = tab.linkedBrowser;

  // This timeout is used simulate the a window being open for an extended period of
  // time before a Popup Notification is shown so that when the tab containing the
  // Popup Notification is moved to a new window there is large enough difference
  // between the initial windows interal timer and the new windows interal timer so
  // that it would impact the security delay if it was based on the windows interal timer
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, TEST_WINDOW_TIME_OPEN));

  // Open Geolocation popup
  let popupShownPromise = new Promise(resolve => {
    waitForNotificationPanel(window, function () {
      resolve(this);
    });
  });
  await SpecialPowers.spawn(browser, [], async function () {
    content.navigator.geolocation.getCurrentPosition(() => {});
  });
  await popupShownPromise;

  let notification = window.PopupNotifications.getNotification("geolocation");
  ok(
    window.PopupNotifications.isPanelOpen && notification,
    "Geolocation notification is open"
  );

  // Move Tab with Popup Notification to a new window with its own
  // performance.now() counter
  let promiseWin = BrowserTestUtils.waitForNewWindow();
  let newWindow = window.gBrowser.replaceTabWithWindow(tab);
  await promiseWin;
  await waitForWindowReadyForPopupNotifications(newWindow);
  let timeNow = Cu.now();

  // Ensure security delay is completed
  await ensureSecurityDelayReady(timeNow);

  // Ensure Popup is still open
  ok(
    newWindow.PopupNotifications.isPanelOpen,
    "Geolocation notification is open"
  );

  let popupHidden = BrowserTestUtils.waitForEvent(
    newWindow.PopupNotifications.panel,
    "popuphidden"
  );

  // Attempt to resolve the Popup
  let acceptBtn = newWindow.PopupNotifications.panel.querySelector(
    ".popup-notification-primary-button"
  );
  acceptBtn.click();

  await popupHidden;
  // Esnure the Popup has been resolved
  Assert.ok(
    !newWindow.PopupNotifications.isPanelOpen,
    "Geolocation popup is hidden"
  );

  await BrowserTestUtils.closeWindow(newWindow);
  await BrowserTestUtils.closeWindow(window);
});
