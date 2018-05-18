/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
}

var tests = [
  // Test that popupnotifications are anchored to the identity icon on
  // about:blank, where anchor icons are hidden.
  { id: "Test#1",
    async run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      is(document.getElementById("geo-notification-icon").boxObject.width, 0,
         "geo anchor shouldn't be visible");
      is(popup.anchorNode.id, "identity-icon",
         "notification anchored to identity icon");
      dismissNotification(popup);
    },
    onHidden(popup) {
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that popupnotifications are anchored to the identity icon after
  // navigation to about:blank.
  { id: "Test#2",
    async run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        persistence: 1,
      });
      this.notification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      await promiseTabLoadEvent(gBrowser.selectedTab, "about:blank");

      checkPopup(popup, this.notifyObj);
      is(document.getElementById("geo-notification-icon").boxObject.width, 0,
         "geo anchor shouldn't be visible");
      is(popup.anchorNode.id, "identity-icon",
         "notification anchored to identity icon");
      dismissNotification(popup);
    },
    onHidden(popup) {
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that dismissed popupnotifications cannot be opened on about:blank, but
  // can be opened after navigation.
  { id: "Test#3",
    async run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        dismissed: true,
        persistence: 1,
      });
      this.notification = showNotification(this.notifyObj);

      is(document.getElementById("geo-notification-icon").boxObject.width, 0,
         "geo anchor shouldn't be visible");

      await promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");

      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");

      EventUtils.synthesizeMouse(document.getElementById("geo-notification-icon"), 2, 2, {});
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden(popup) {
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that popupnotifications are hidden while editing the URL in the
  // location bar, anchored to the identity icon when the focus is moved away
  // from the location bar, and restored when the URL is reverted.
  { id: "Test#4",
    async run() {
      for (let persistent of [false, true]) {
        let shown = waitForNotificationPanel();
        this.notifyObj = new BasicNotification(this.id);
        this.notifyObj.anchorID = "geo-notification-icon";
        this.notifyObj.addOptions({ persistent });
        this.notification = showNotification(this.notifyObj);
        await shown;

        checkPopup(PopupNotifications.panel, this.notifyObj);

        // Typing in the location bar should hide the notification.
        let hidden = waitForNotificationPanelHidden();
        gURLBar.select();
        EventUtils.sendString("*");
        await hidden;

        is(document.getElementById("geo-notification-icon").boxObject.width, 0,
           "geo anchor shouldn't be visible");

        // Moving focus to the next control should show the notifications again,
        // anchored to the identity icon. We clear the URL bar before moving the
        // focus so that the awesomebar popup doesn't get in the way.
        shown = waitForNotificationPanel();
        EventUtils.synthesizeKey("KEY_Backspace");
        EventUtils.synthesizeKey("KEY_Tab");
        await shown;

        is(PopupNotifications.panel.anchorNode.id, "identity-icon",
           "notification anchored to identity icon");

        // Moving focus to the location bar should hide the notification again.
        hidden = waitForNotificationPanelHidden();
        EventUtils.synthesizeKey("KEY_Tab", {shiftKey: true});
        await hidden;

        // Reverting the URL should show the notification again.
        shown = waitForNotificationPanel();
        EventUtils.synthesizeKey("KEY_Escape");
        await shown;

        checkPopup(PopupNotifications.panel, this.notifyObj);

        hidden = waitForNotificationPanelHidden();
        this.notification.remove();
        await hidden;
      }
      goNext();
    }
  },
  // Test that popupnotifications triggered while editing the URL in the
  // location bar are only shown later when the URL is reverted.
  { id: "Test#5",
    async run() {
      for (let persistent of [false, true]) {
        // Start editing the URL, ensuring that the awesomebar popup is hidden.
        gURLBar.select();
        EventUtils.sendString("*");
        EventUtils.synthesizeKey("KEY_Backspace");

        // Trying to show a notification should display nothing.
        let notShowing = TestUtils.topicObserved("PopupNotifications-updateNotShowing");
        this.notifyObj = new BasicNotification(this.id);
        this.notifyObj.anchorID = "geo-notification-icon";
        this.notifyObj.addOptions({ persistent });
        this.notification = showNotification(this.notifyObj);
        await notShowing;

        // Reverting the URL should show the notification.
        let shown = waitForNotificationPanel();
        EventUtils.synthesizeKey("KEY_Escape");
        await shown;

        checkPopup(PopupNotifications.panel, this.notifyObj);

        let hidden = waitForNotificationPanelHidden();
        this.notification.remove();
        await hidden;
      }

      goNext();
    }
  },
  // Test that persistent panels are still open after switching to another tab
  // and back, even while editing the URL in the new tab.
  { id: "Test#6",
    async run() {
      let shown = waitForNotificationPanel();
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        persistent: true,
      });
      this.notification = showNotification(this.notifyObj);
      await shown;

      // Switching to a new tab should hide the notification.
      let hidden = waitForNotificationPanelHidden();
      this.oldSelectedTab = gBrowser.selectedTab;
      await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
      await hidden;

      // Start editing the URL.
      gURLBar.select();
      EventUtils.sendString("*");

      // Switching to the old tab should show the notification again.
      shown = waitForNotificationPanel();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
      await shown;

      checkPopup(PopupNotifications.panel, this.notifyObj);

      hidden = waitForNotificationPanelHidden();
      this.notification.remove();
      await hidden;

      goNext();
    }
  },
];
