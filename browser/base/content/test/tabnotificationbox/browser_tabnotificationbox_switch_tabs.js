/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function assertNotificationBoxHidden(notificationBox, reason) {
  let { stack } = notificationBox;
  let name = stack.getAttribute("name");
  ok(name, `Notification box has a name ${reason}`);

  let { selectedViewName } = stack.parentElement;
  ok(
    selectedViewName != name,
    `Box is not shown ${reason} ${selectedViewName} != ${name}`
  );
}

function assertNotificationBoxShown(notificationBox, reason) {
  let { stack } = notificationBox;
  let name = stack.getAttribute("name");
  ok(name, `Notification box has a name ${reason}`);

  let { selectedViewName } = stack.parentElement;
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
    set: [["browser.proton.infobars.enabled", true]],
  });
});

add_task(async function testNotificationDeckIsLazy() {
  let deck = document.getElementById("tab-notification-deck");
  ok(!deck, "There is no tab notification deck");
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    createNotification({
      browser,
      label: "First notification",
      value: "first-notification",
      priority: "PRIORITY_INFO_LOW",
    });

    deck = document.getElementById("tab-notification-deck");
    ok(deck, "Creating a notification created the deck");
  });
});

add_task(async function testNotificationInBackgroundTab() {
  let firstTab = gBrowser.selectedTab;

  // Navigating to a page will create the notification box, so go to example.com
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    let notificationBox = gBrowser.readNotificationBox(browser);
    ok(notificationBox, "The notification box has already been created");

    assertNotificationBoxShown(notificationBox, "initial tab creation");
    gBrowser.selectedTab = firstTab;
    assertNotificationBoxHidden(notificationBox, "initial tab creation");

    createNotification({
      browser,
      label: "My notification body",
      value: "test-notification",
      priority: "PRIORITY_INFO_LOW",
    });

    gBrowser.selectedTab = gBrowser.getTabForBrowser(browser);
    assertNotificationBoxShown(notificationBox, "initial tab creation");
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
    let notificationBox = gBrowser.readNotificationBox(browser);
    ok(notificationBox, "Notification box was created");
    assertNotificationBoxShown(notificationBox, "after appendNotification");
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
  ok(notificationBoxThree, "example.com has a notification box");

  // Verify the correct box is shown after creating tabs.
  assertNotificationBoxShown(notificationBoxThree, "after open");

  createNotification({
    browser: browserTwo,
    label: "Test blank",
    value: "blank",
    priority: "PRIORITY_INFO_LOW",
  });
  notificationBoxTwo = gBrowser.readNotificationBox(browserTwo);
  ok(notificationBoxTwo, "Notification box was created");

  // Verify the selected browser's notification box is still shown.
  assertNotificationBoxHidden(notificationBoxTwo, "hidden create");
  assertNotificationBoxShown(notificationBoxThree, "other create");

  createNotification({
    browser: browserThree,
    label: "Test active tab",
    value: "active",
    priority: "PRIORITY_CRITICAL_LOW",
  });
  // Verify the selected browser's notification box is still shown.
  assertNotificationBoxHidden(notificationBoxTwo, "active create");
  assertNotificationBoxShown(notificationBoxThree, "active create");

  gBrowser.selectedTab = tabTwo;

  // Verify the notification box for the tab that has one gets shown.
  assertNotificationBoxShown(notificationBoxTwo, "tab switch");
  assertNotificationBoxHidden(notificationBoxThree, "tab switch");

  BrowserTestUtils.removeTab(tabTwo);
  BrowserTestUtils.removeTab(tabThree);
});
