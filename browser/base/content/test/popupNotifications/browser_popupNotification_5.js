/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
}

var gNotification;

var tests = [
  // panel updates should fire the showing and shown callbacks again.
  { id: "Test#1",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);

      this.notifyObj.showingCallbackTriggered = false;
      this.notifyObj.shownCallbackTriggered = false;

      // Force an update of the panel. This is typically called
      // automatically when receiving 'activate' or 'TabSelect' events,
      // but from a setTimeout, which is inconvenient for the test.
      PopupNotifications._update();

      checkPopup(popup, this.notifyObj);

      this.notification.remove();
    },
    onHidden() { }
  },
  // A first dismissed notification shouldn't stop _update from showing a second notification
  { id: "Test#2",
    run() {
      this.notifyObj1 = new BasicNotification(this.id);
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notifyObj1.options.dismissed = true;
      this.notification1 = showNotification(this.notifyObj1);

      this.notifyObj2 = new BasicNotification(this.id);
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notifyObj2.options.dismissed = true;
      this.notification2 = showNotification(this.notifyObj2);

      this.notification2.dismissed = false;
      PopupNotifications._update();
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj2);
      this.notification1.remove();
      this.notification2.remove();
    },
    onHidden(popup) { }
  },
  // The anchor icon should be shown for notifications in background windows.
  { id: "Test#3",
    async run() {
      let notifyObj = new BasicNotification(this.id);
      notifyObj.options.dismissed = true;

      let win = await BrowserTestUtils.openNewBrowserWindow();

      // Open the notification in the original window, now in the background.
      let notification = showNotification(notifyObj);
      let anchor = document.getElementById("default-notification-icon");
      is(anchor.getAttribute("showing"), "true", "the anchor is shown");
      notification.remove();

      await BrowserTestUtils.closeWindow(win);
      await waitForWindowReadyForPopupNotifications(window);

      goNext();
    }
  },
  // Test that persistent doesn't allow the notification to persist after
  // navigation.
  { id: "Test#4",
    async run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.addOptions({
        persistent: true
      });
      this.notification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      this.complete = false;

      await promiseTabLoadEvent(gBrowser.selectedTab, "http://example.org/");
      await promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");

      // This code should not be executed.
      ok(false, "Should have removed the notification after navigation");
      // Properly dismiss and cleanup in case the unthinkable happens.
      this.complete = true;
      triggerSecondaryCommand(popup, 0);
    },
    onHidden(popup) {
      ok(!this.complete, "Should have hidden the notification after navigation");
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that persistent allows the notification to persist until explicitly
  // dismissed.
  { id: "Test#5",
    async run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.addOptions({
        persistent: true
      });
      this.notification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      this.complete = false;

      // Notification should persist after attempt to dismiss by clicking on the
      // content area.
      let browser = gBrowser.selectedBrowser;
      await BrowserTestUtils.synthesizeMouseAtCenter("body", {}, browser);

      // Notification should be hidden after dismissal via Don't Allow.
      this.complete = true;
      triggerSecondaryCommand(popup, 0);
    },
    onHidden(popup) {
      ok(this.complete, "Should have hidden the notification after clicking Not Now");
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that persistent panels are still open after switching to another tab
  // and back.
  { id: "Test#6a",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.persistent = true;
      gNotification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
    },
    onHidden(popup) {
      ok(true, "Should have hidden the notification after tab switch");
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Second part of the previous test that compensates for the limitation in
  // runNextTest that expects a single onShown/onHidden invocation per test.
  { id: "Test#6b",
    run() {
      let id = PopupNotifications.panel.firstChild.getAttribute("popupid");
      ok(id.endsWith("Test#6a"), "Should have found the notification from Test6a");
      ok(PopupNotifications.isPanelOpen, "Should have shown the popup again after getting back to the tab");
      gNotification.remove();
      gNotification = null;
      goNext();
    }
  },
  // Test that persistent panels are still open after switching to another
  // window and back.
  { id: "Test#7",
    async run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
      let firstTab = gBrowser.selectedTab;

      await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

      let shown = waitForNotificationPanel();
      let notifyObj = new BasicNotification(this.id);
      notifyObj.options.persistent = true;
      this.notification = showNotification(notifyObj);
      await shown;

      ok(notifyObj.shownCallbackTriggered, "Should have triggered the shown event");
      ok(notifyObj.showingCallbackTriggered, "Should have triggered the showing event");
      // Reset to false so that we can ensure these are not fired a second time.
      notifyObj.shownCallbackTriggered = false;
      notifyObj.showingCallbackTriggered = false;
      let timeShown = this.notification.timeShown;

      let promiseWin = BrowserTestUtils.waitForNewWindow();
      gBrowser.replaceTabWithWindow(firstTab);
      let win = await promiseWin;

      let anchor = win.document.getElementById("default-notification-icon");
      win.PopupNotifications._reshowNotifications(anchor);
      ok(win.PopupNotifications.panel.childNodes.length == 0,
         "no notification displayed in new window");

      await BrowserTestUtils.closeWindow(win);
      await waitForWindowReadyForPopupNotifications(window);

      let id = PopupNotifications.panel.firstChild.getAttribute("popupid");
      ok(id.endsWith("Test#7"), "Should have found the notification from Test7");
      ok(PopupNotifications.isPanelOpen,
         "Should have kept the popup on the first window");
      ok(!notifyObj.dismissalCallbackTriggered,
         "Should not have triggered a dismissed event");
      ok(!notifyObj.shownCallbackTriggered,
         "Should not have triggered a second shown event");
      ok(!notifyObj.showingCallbackTriggered,
         "Should not have triggered a second showing event");
      ok(this.notification.timeShown > timeShown,
         "should have updated timeShown to restart the security delay");

      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;

      goNext();
    }
  },
  // Test that only the first persistent notification is shown on update
  { id: "Test#8",
    run() {
      this.notifyObj1 = new BasicNotification(this.id);
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notifyObj1.options.persistent = true;
      this.notification1 = showNotification(this.notifyObj1);

      this.notifyObj2 = new BasicNotification(this.id);
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notifyObj2.options.persistent = true;
      this.notification2 = showNotification(this.notifyObj2);

      PopupNotifications._update();
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj1);
      this.notification1.remove();
      this.notification2.remove();
    },
    onHidden(popup) { }
  },
  // Test that persistent notifications are shown stacked by anchor on update
  { id: "Test#9",
    run() {
      this.notifyObj1 = new BasicNotification(this.id);
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notifyObj1.options.persistent = true;
      this.notification1 = showNotification(this.notifyObj1);

      this.notifyObj2 = new BasicNotification(this.id);
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notifyObj2.options.persistent = true;
      this.notification2 = showNotification(this.notifyObj2);

      this.notifyObj3 = new BasicNotification(this.id);
      this.notifyObj3.id += "_3";
      this.notifyObj3.anchorID = "default-notification-icon";
      this.notifyObj3.options.persistent = true;
      this.notification3 = showNotification(this.notifyObj3);

      PopupNotifications._update();
    },
    onShown(popup) {
      let notifications = popup.childNodes;
      is(notifications.length, 2, "two notifications displayed");
      let [notification1, notification2] = notifications;
      is(notification1.id, this.notifyObj1.id + "-notification", "id 1 matches");
      is(notification2.id, this.notifyObj3.id + "-notification", "id 2 matches");

      this.notification1.remove();
      this.notification2.remove();
      this.notification3.remove();
    },
    onHidden(popup) { }
  },
  // Test that on closebutton click, only the persistent notification
  // that contained the closebutton loses its persistent status.
  { id: "Test#10",
    run() {
      this.notifyObj1 = new BasicNotification(this.id);
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "geo-notification-icon";
      this.notifyObj1.options.persistent = true;
      this.notifyObj1.options.hideClose = false;
      this.notification1 = showNotification(this.notifyObj1);

      this.notifyObj2 = new BasicNotification(this.id);
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notifyObj2.options.persistent = true;
      this.notifyObj2.options.hideClose = false;
      this.notification2 = showNotification(this.notifyObj2);

      this.notifyObj3 = new BasicNotification(this.id);
      this.notifyObj3.id += "_3";
      this.notifyObj3.anchorID = "geo-notification-icon";
      this.notifyObj3.options.persistent = true;
      this.notifyObj3.options.hideClose = false;
      this.notification3 = showNotification(this.notifyObj3);

      PopupNotifications._update();
    },
    onShown(popup) {
      let notifications = popup.childNodes;
      is(notifications.length, 3, "three notifications displayed");
      EventUtils.synthesizeMouseAtCenter(notifications[1].closebutton, {});
    },
    onHidden(popup) {
      let notifications = popup.childNodes;
      is(notifications.length, 2, "two notifications displayed");

      ok(this.notification1.options.persistent, "notification 1 is persistent");
      ok(!this.notification2.options.persistent, "notification 2 is not persistent");
      ok(this.notification3.options.persistent, "notification 3 is persistent");

      this.notification1.remove();
      this.notification2.remove();
      this.notification3.remove();
    }
  },
  // Test clicking the anchor icon.
  // Clicking the anchor of an already visible persistent notification should
  // focus the main action button, but not cause additional showing/shown event
  // callback calls.
  // Clicking the anchor of a dismissed notification should show it, even when
  // the currently displayed notification is a persistent one.
  { id: "Test#11",
    async run() {
      await SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});

      function clickAnchor(notifyObj) {
        let anchor = document.getElementById(notifyObj.anchorID);
        EventUtils.synthesizeMouseAtCenter(anchor, {});
      }

      let popup = PopupNotifications.panel;

      let notifyObj1 = new BasicNotification(this.id);
      notifyObj1.id += "_1";
      notifyObj1.anchorID = "default-notification-icon";
      notifyObj1.options.persistent = true;
      let shown = waitForNotificationPanel();
      let notification1 = showNotification(notifyObj1);
      await shown;
      checkPopup(popup, notifyObj1);
      ok(!notifyObj1.dismissalCallbackTriggered,
         "Should not have dismissed the notification");
      notifyObj1.shownCallbackTriggered = false;
      notifyObj1.showingCallbackTriggered = false;

      // Click the anchor. This should focus the closebutton
      // (because it's the first focusable element), but not
      // call event callbacks on the notification object.
      clickAnchor(notifyObj1);
      is(document.activeElement, popup.childNodes[0].closebutton);
      ok(!notifyObj1.dismissalCallbackTriggered,
         "Should not have dismissed the notification");
      ok(!notifyObj1.shownCallbackTriggered,
         "Should have triggered the shown event again");
      ok(!notifyObj1.showingCallbackTriggered,
         "Should have triggered the showing event again");

      // Add another notification.
      let notifyObj2 = new BasicNotification(this.id);
      notifyObj2.id += "_2";
      notifyObj2.anchorID = "geo-notification-icon";
      notifyObj2.options.dismissed = true;
      let notification2 = showNotification(notifyObj2);

      // Click the anchor of the second notification, this should dismiss the
      // first notification.
      shown = waitForNotificationPanel();
      clickAnchor(notifyObj2);
      await shown;
      checkPopup(popup, notifyObj2);
      ok(notifyObj1.dismissalCallbackTriggered,
         "Should have dismissed the first notification");

      // Click the anchor of the first notification, it should be shown again.
      shown = waitForNotificationPanel();
      clickAnchor(notifyObj1);
      await shown;
      checkPopup(popup, notifyObj1);
      ok(notifyObj2.dismissalCallbackTriggered,
         "Should have dismissed the second notification");

      // Cleanup.
      notification1.remove();
      notification2.remove();
      goNext();
    }
  },
];
