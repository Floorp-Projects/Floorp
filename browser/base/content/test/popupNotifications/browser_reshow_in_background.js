"use strict";

/**
 * Tests that when PopupNotifications for background tabs are reshown, they
 * don't show up in the foreground tab, but only in the background tab that
 * they belong to.
 */
add_task(async function test_background_notifications_dont_reshow_in_foreground() {
  // Our initial tab will be A. Let's open two more tabs B and C, but keep
  // A selected. Then, we'll trigger a PopupNotification in C, and then make
  // it reshow.
  let tabB = BrowserTestUtils.addTab(gBrowser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(tabB.linkedBrowser);

  let tabC = BrowserTestUtils.addTab(gBrowser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(tabC.linkedBrowser);

  let seenEvents = [];

  let options = {
    dismissed: false,
    eventCallback(popupEvent) {
      seenEvents.push(popupEvent);
    },
  };

  let notification =
    PopupNotifications.show(tabC.linkedBrowser, "test-notification",
                            "", "plugins-notification-icon",
                            null, null, options);
  Assert.deepEqual(seenEvents, [], "Should have seen no events yet.");

  await BrowserTestUtils.switchTab(gBrowser, tabB);
  Assert.deepEqual(seenEvents, [], "Should have seen no events yet.");

  notification.reshow();
  Assert.deepEqual(seenEvents, [], "Should have seen no events yet.");

  let panelShown =
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
  await BrowserTestUtils.switchTab(gBrowser, tabC);
  await panelShown;

  Assert.equal(seenEvents.length, 2, "Should have seen two events.");
  Assert.equal(seenEvents[0], "showing", "Should have said popup was showing.");
  Assert.equal(seenEvents[1], "shown", "Should have said popup was shown.");

  let panelHidden =
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  PopupNotifications.remove(notification);
  await panelHidden;

  await BrowserTestUtils.removeTab(tabB);
  await BrowserTestUtils.removeTab(tabC);
});
