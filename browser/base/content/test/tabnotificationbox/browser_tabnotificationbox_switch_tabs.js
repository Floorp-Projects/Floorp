/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function assertNotificationBoxHidden(reason, browser) {
  let notificationBox = gBrowser.readNotificationBox(browser);

  if (!notificationBox) {
    ok(!notificationBox, `Notification box has not been created ${reason}`);
    return;
  }

  let name = notificationBox._stack.getAttribute("name");
  ok(name, `Notification box has a name ${reason}`);

  let { selectedViewName } = notificationBox._stack.parentElement;
  ok(
    selectedViewName != name,
    `Box is not shown ${reason} ${selectedViewName} != ${name}`
  );
}

function assertNotificationBoxShown(reason, browser) {
  let notificationBox = gBrowser.readNotificationBox(browser);
  ok(notificationBox, `Notification box has been created ${reason}`);

  let name = notificationBox._stack.getAttribute("name");
  ok(name, `Notification box has a name ${reason}`);

  let { selectedViewName } = notificationBox._stack.parentElement;
  is(selectedViewName, name, `Box is shown ${reason}`);
}

function createNotification({ browser, label, value, priority }) {
  let notificationBox = gBrowser.getNotificationBox(browser);
  let notification = notificationBox.appendNotification(
    label,
    value,
    null,
    notificationBox[priority],
    []
  );
  return notification;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.proton.enabled", true]],
  });
});

add_task(async function testNotificationInBackgroundTab() {
  let firstTab = gBrowser.selectedTab;

  // Navigating to a page should not create the notification box
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let notificationBox = gBrowser.readNotificationBox(browser);
    ok(!notificationBox, "The notification box has not been created");

    gBrowser.selectedTab = firstTab;
    assertNotificationBoxHidden("initial first tab");

    createNotification({
      browser,
      label: "My notification body",
      value: "test-notification",
      priority: "PRIORITY_INFO_LOW",
    });

    gBrowser.selectedTab = gBrowser.getTabForBrowser(browser);
    assertNotificationBoxShown("notification created");
  });
});

add_task(async function testNotificationInActiveTab() {
  // Open about:blank so the notification box isn't created on tab open.
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    ok(!gBrowser.readNotificationBox(browser), "No notifications for new tab");

    createNotification({
      browser,
      label: "Notification!",
      value: "test-notification",
      priority: "PRIORITY_INFO_LOW",
    });
    assertNotificationBoxShown("after appendNotification");
  });
});

add_task(async function testNotificationMultipleTabs() {
  let tabOne = gBrowser.selectedTab;
  let tabTwo = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:blank",
  });
  let tabThree = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "https://example.com",
  });
  let browserOne = tabOne.linkedBrowser;
  let browserTwo = tabTwo.linkedBrowser;
  let browserThree = tabThree.linkedBrowser;

  is(gBrowser.selectedBrowser, browserThree, "example.com selected");

  let notificationBoxOne = gBrowser.readNotificationBox(browserOne);
  let notificationBoxTwo = gBrowser.readNotificationBox(browserTwo);
  let notificationBoxThree = gBrowser.readNotificationBox(browserThree);

  ok(!notificationBoxOne, "no initial tab box");
  ok(!notificationBoxTwo, "no about:blank box");
  ok(!notificationBoxThree, "no example.com box");

  // Verify the correct box is shown after creating tabs.
  assertNotificationBoxHidden("after open", browserOne);
  assertNotificationBoxHidden("after open", browserTwo);
  assertNotificationBoxHidden("after open", browserThree);

  createNotification({
    browser: browserTwo,
    label: "Test blank",
    value: "blank",
    priority: "PRIORITY_INFO_LOW",
  });
  notificationBoxTwo = gBrowser.readNotificationBox(browserTwo);
  ok(notificationBoxTwo, "Notification box was created");

  // Verify the selected browser's notification box is still hidden.
  assertNotificationBoxHidden("hidden create", browserTwo);
  assertNotificationBoxHidden("other create", browserThree);

  createNotification({
    browser: browserThree,
    label: "Test active tab",
    value: "active",
    priority: "PRIORITY_CRITICAL_LOW",
  });
  // Verify the selected browser's notification box is still shown.
  assertNotificationBoxHidden("active create", browserTwo);
  assertNotificationBoxShown("active create", browserThree);

  gBrowser.selectedTab = tabTwo;

  // Verify the notification box for the tab that has one gets shown.
  assertNotificationBoxShown("tab switch", browserTwo);
  assertNotificationBoxHidden("tab switch", browserThree);

  BrowserTestUtils.removeTab(tabTwo);
  BrowserTestUtils.removeTab(tabThree);
});
