/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These are shared between test #4 to #5
var wrongBrowserNotificationObject = new BasicNotification("wrongBrowser");
var wrongBrowserNotification;

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
}

var tests = [
  { id: "Test#1",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden(popup) {
      ok(this.notifyObj.mainActionClicked, "mainAction was clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  { id: "Test#2",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden(popup) {
      ok(this.notifyObj.secondaryActionClicked, "secondaryAction was clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  { id: "Test#2b",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.secondaryActions.push({
        label: "Extra Secondary Action",
        accessKey: "E",
        callback: () => this.extraSecondaryActionClicked = true,
      });
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      triggerSecondaryCommand(popup, 1);
    },
    onHidden(popup) {
      ok(this.extraSecondaryActionClicked, "extra secondary action was clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  { id: "Test#2c",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.secondaryActions.push({
        label: "Extra Secondary Action",
        accessKey: "E",
        callback: () => ok(false, "unexpected callback invocation"),
      }, {
        label: "Other Extra Secondary Action",
        accessKey: "O",
        callback: () => this.extraSecondaryActionClicked = true,
      });
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      triggerSecondaryCommand(popup, 2);
    },
    onHidden(popup) {
      ok(this.extraSecondaryActionClicked, "extra secondary action was clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  { id: "Test#3",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // test opening a notification for a background browser
  // Note: test 4 to 6 share a tab.
  { id: "Test#4",
    async run() {
      let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com/");
      await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
      isnot(gBrowser.selectedTab, tab, "new tab isn't selected");
      wrongBrowserNotificationObject.browser = gBrowser.getBrowserForTab(tab);
      let promiseTopic = promiseTopicObserved("PopupNotifications-backgroundShow");
      wrongBrowserNotification = showNotification(wrongBrowserNotificationObject);
      await promiseTopic;
      is(PopupNotifications.isPanelOpen, false, "panel isn't open");
      ok(!wrongBrowserNotificationObject.mainActionClicked, "main action wasn't clicked");
      ok(!wrongBrowserNotificationObject.secondaryActionClicked, "secondary action wasn't clicked");
      ok(!wrongBrowserNotificationObject.dismissalCallbackTriggered, "dismissal callback wasn't called");
      goNext();
    }
  },
  // now select that browser and test to see that the notification appeared
  { id: "Test#5",
    run() {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.tabs[gBrowser.tabs.length - 1];
    },
    onShown(popup) {
      checkPopup(popup, wrongBrowserNotificationObject);
      is(PopupNotifications.isPanelOpen, true, "isPanelOpen getter doesn't lie");

      // switch back to the old browser
      gBrowser.selectedTab = this.oldSelectedTab;
    },
    onHidden(popup) {
      // actually remove the notification to prevent it from reappearing
      ok(wrongBrowserNotificationObject.dismissalCallbackTriggered, "dismissal callback triggered due to tab switch");
      wrongBrowserNotification.remove();
      ok(wrongBrowserNotificationObject.removedCallbackTriggered, "removed callback triggered");
      wrongBrowserNotification = null;
    }
  },
  // test that the removed notification isn't shown on browser re-select
  { id: "Test#6",
    async run() {
      let promiseTopic = promiseTopicObserved("PopupNotifications-updateNotShowing");
      gBrowser.selectedTab = gBrowser.tabs[gBrowser.tabs.length - 1];
      await promiseTopic;
      is(PopupNotifications.isPanelOpen, false, "panel isn't open");
      gBrowser.removeTab(gBrowser.selectedTab);
      goNext();
    }
  },
  // Test that two notifications with the same ID result in a single displayed
  // notification.
  { id: "Test#7",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      // Show the same notification twice
      this.notification1 = showNotification(this.notifyObj);
      this.notification2 = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      this.notification2.remove();
    },
    onHidden(popup) {
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test that two notifications with different IDs are displayed
  { id: "Test#8",
    run() {
      this.testNotif1 = new BasicNotification(this.id);
      this.testNotif1.message += " 1";
      showNotification(this.testNotif1);
      this.testNotif2 = new BasicNotification(this.id);
      this.testNotif2.message += " 2";
      this.testNotif2.id += "-2";
      showNotification(this.testNotif2);
    },
    onShown(popup) {
      is(popup.childNodes.length, 2, "two notifications are shown");
      // Trigger the main command for the first notification, and the secondary
      // for the second. Need to do mainCommand first since the secondaryCommand
      // triggering is async.
      triggerMainCommand(popup);
      is(popup.childNodes.length, 1, "only one notification left");
      triggerSecondaryCommand(popup, 0);
    },
    onHidden(popup) {
      ok(this.testNotif1.mainActionClicked, "main action #1 was clicked");
      ok(!this.testNotif1.secondaryActionClicked, "secondary action #1 wasn't clicked");
      ok(!this.testNotif1.dismissalCallbackTriggered, "dismissal callback #1 wasn't called");

      ok(!this.testNotif2.mainActionClicked, "main action #2 wasn't clicked");
      ok(this.testNotif2.secondaryActionClicked, "secondary action #2 was clicked");
      ok(!this.testNotif2.dismissalCallbackTriggered, "dismissal callback #2 wasn't called");
    }
  },
  // Test notification without mainAction or secondaryActions, it should fall back
  // to a default button that dismisses the notification in place of the main action.
  { id: "Test#9",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.mainAction = null;
      this.notifyObj.secondaryActions = null;
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      triggerMainCommand(popup);
    },
    onHidden(popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test notification without mainAction but with secondaryActions, it should fall back
  // to a default button that dismisses the notification in place of the main action
  // and ignore the passed secondaryActions.
  { id: "Test#10",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.mainAction = null;
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      let notification = popup.childNodes[0];
      is(notification.getAttribute("secondarybuttonhidden"), "true", "secondary button is hidden");
      triggerMainCommand(popup);
    },
    onHidden(popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test two notifications with different anchors
  { id: "Test#11",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.firstNotification = showNotification(this.notifyObj);
      this.notifyObj2 = new BasicNotification(this.id);
      this.notifyObj2.id += "-2";
      this.notifyObj2.anchorID = "addons-notification-icon";
      // Second showNotification() overrides the first
      this.secondNotification = showNotification(this.notifyObj2);
    },
    onShown(popup) {
      // This also checks that only one element is shown.
      checkPopup(popup, this.notifyObj2);
      is(document.getElementById("geo-notification-icon").boxObject.width, 0,
         "geo anchor shouldn't be visible");
      dismissNotification(popup);
    },
    onHidden(popup) {
      // Remove the notifications
      this.firstNotification.remove();
      this.secondNotification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
      ok(this.notifyObj2.removedCallbackTriggered, "removed callback triggered");
    }
  }
];
