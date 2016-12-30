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
  // Popup Notifications main actions should catch exceptions from callbacks
  { id: "Test#1",
    run() {
      this.testNotif = new ErrorNotification(this.id);
      showNotification(this.testNotif);
    },
    onShown(popup) {
      checkPopup(popup, this.testNotif);
      triggerMainCommand(popup);
    },
    onHidden(popup) {
      ok(this.testNotif.mainActionClicked, "main action has been triggered");
    }
  },
  // Popup Notifications secondary actions should catch exceptions from callbacks
  { id: "Test#2",
    run() {
      this.testNotif = new ErrorNotification(this.id);
      showNotification(this.testNotif);
    },
    onShown(popup) {
      checkPopup(popup, this.testNotif);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden(popup) {
      ok(this.testNotif.secondaryActionClicked, "secondary action has been triggered");
    }
  },
  // Existing popup notification shouldn't disappear when adding a dismissed notification
  { id: "Test#3",
    run() {
      this.notifyObj1 = new BasicNotification(this.id);
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notification1 = showNotification(this.notifyObj1);
    },
    onShown(popup) {
      // Now show a dismissed notification, and check that it doesn't clobber
      // the showing one.
      this.notifyObj2 = new BasicNotification(this.id);
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notifyObj2.options.dismissed = true;
      this.notification2 = showNotification(this.notifyObj2);

      checkPopup(popup, this.notifyObj1);

      // check that both anchor icons are showing
      is(document.getElementById("default-notification-icon").getAttribute("showing"), "true",
         "notification1 anchor should be visible");
      is(document.getElementById("geo-notification-icon").getAttribute("showing"), "true",
         "notification2 anchor should be visible");

      dismissNotification(popup);
    },
    onHidden(popup) {
      this.notification1.remove();
      this.notification2.remove();
    }
  },
  // Showing should be able to modify the popup data
  { id: "Test#4",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      let normalCallback = this.notifyObj.options.eventCallback;
      this.notifyObj.options.eventCallback = function(eventName) {
        if (eventName == "showing") {
          this.mainAction.label = "Alternate Label";
        }
        normalCallback.call(this, eventName);
      };
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      // checkPopup checks for the matching label. Note that this assumes that
      // this.notifyObj.mainAction is the same as notification.mainAction,
      // which could be a problem if we ever decided to deep-copy.
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden() { }
  },
  // Moving a tab to a new window should remove non-swappable notifications.
  { id: "Test#5",
    *run() {
      yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

      let notifyObj = new BasicNotification(this.id);

      let shown = waitForNotificationPanel();
      showNotification(notifyObj);
      yield shown;

      let promiseWin = BrowserTestUtils.waitForNewWindow();
      gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
      let win = yield promiseWin;

      let anchor = win.document.getElementById("default-notification-icon");
      win.PopupNotifications._reshowNotifications(anchor);
      ok(win.PopupNotifications.panel.childNodes.length == 0,
         "no notification displayed in new window");
      ok(notifyObj.swappingCallbackTriggered, "the swapping callback was triggered");
      ok(notifyObj.removedCallbackTriggered, "the removed callback was triggered");

      yield BrowserTestUtils.closeWindow(win);
      yield waitForWindowReadyForPopupNotifications(window);

      goNext();
    }
  },
  // Moving a tab to a new window should preserve swappable notifications.
  { id: "Test#6",
    *run() {
      yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
      let notifyObj = new BasicNotification(this.id);
      let originalCallback = notifyObj.options.eventCallback;
      notifyObj.options.eventCallback = function(eventName) {
        originalCallback(eventName);
        return eventName == "swapping";
      };

      let shown = waitForNotificationPanel();
      let notification = showNotification(notifyObj);
      yield shown;

      let promiseWin = BrowserTestUtils.waitForNewWindow();
      gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
      let win = yield promiseWin;
      yield waitForWindowReadyForPopupNotifications(win);

      yield new Promise(resolve => {
        let callback = notification.options.eventCallback;
        notification.options.eventCallback = function(eventName) {
          callback(eventName);
          if (eventName == "shown") {
            resolve();
          }
        };
        info("Showing the notification again");
        notification.reshow();
      });

      checkPopup(win.PopupNotifications.panel, notifyObj);
      ok(notifyObj.swappingCallbackTriggered, "the swapping callback was triggered");

      yield BrowserTestUtils.closeWindow(win);
      yield waitForWindowReadyForPopupNotifications(window);

      goNext();
    }
  },
  // the main action callback can keep the notification.
  { id: "Test#8",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.mainAction.dismiss = true;
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback was triggered");
      ok(!this.notifyObj.removedCallbackTriggered, "removed callback wasn't triggered");
      this.notification.remove();
    }
  },
  // a secondary action callback can keep the notification.
  { id: "Test#9",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.secondaryActions[0].dismiss = true;
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback was triggered");
      ok(!this.notifyObj.removedCallbackTriggered, "removed callback wasn't triggered");
      this.notification.remove();
    }
  },
  // returning true in the showing callback should dismiss the notification.
  { id: "Test#10",
    run() {
      let notifyObj = new BasicNotification(this.id);
      let originalCallback = notifyObj.options.eventCallback;
      notifyObj.options.eventCallback = function(eventName) {
        originalCallback(eventName);
        return eventName == "showing";
      };

      let notification = showNotification(notifyObj);
      ok(notifyObj.showingCallbackTriggered, "the showing callback was triggered");
      ok(!notifyObj.shownCallbackTriggered, "the shown callback wasn't triggered");
      notification.remove();
      goNext();
    }
  }
];
