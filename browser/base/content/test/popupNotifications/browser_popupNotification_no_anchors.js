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
    *run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

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
    *run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        persistence: 1,
      });
      this.notification = showNotification(this.notifyObj);
    },
    *onShown(popup) {
      yield promiseTabLoadEvent(gBrowser.selectedTab, "about:blank");

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
    *run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        dismissed: true,
        persistence: 1,
      });
      this.notification = showNotification(this.notifyObj);

      is(document.getElementById("geo-notification-icon").boxObject.width, 0,
         "geo anchor shouldn't be visible");

      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");

      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");

      EventUtils.synthesizeMouse(document.getElementById("geo-notification-icon"), 0, 0, {});
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
  // Test that popupnotifications are anchored to the identity icon while
  // editing the URL in the location bar, and restored to their anchors when the
  // URL is reverted.
  { id: "Test#4",
    *run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

      let shownInitially = waitForNotificationPanel();
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        persistent: true,
      });
      this.notification = showNotification(this.notifyObj);
      yield shownInitially;

      checkPopup(PopupNotifications.panel, this.notifyObj);

      let shownAgain = waitForNotificationPanel();
      // This will cause the popup to hide and show again.
      gURLBar.select();
      EventUtils.synthesizeKey("*", {});
      // Keep the URL bar empty, so we don't show the awesomebar.
      EventUtils.synthesizeKey("VK_BACK_SPACE", {});
      yield shownAgain;

      is(document.getElementById("geo-notification-icon").boxObject.width, 0,
         "geo anchor shouldn't be visible");
      is(PopupNotifications.panel.anchorNode.id, "identity-icon",
         "notification anchored to identity icon");

      let shownLastTime = waitForNotificationPanel();
      // This will cause the popup to hide and show again.
      EventUtils.synthesizeKey("VK_ESCAPE", {});
      yield shownLastTime;

      checkPopup(PopupNotifications.panel, this.notifyObj);

      let hidden = new Promise(resolve => onPopupEvent("popuphidden", resolve));
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
      yield hidden;

      goNext();
    }
  },
];
