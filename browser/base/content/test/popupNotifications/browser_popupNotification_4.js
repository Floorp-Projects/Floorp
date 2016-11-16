/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
  goNext();
}

var tests = [
  // Popup Notifications main actions should catch exceptions from callbacks
  { id: "Test#1",
    run: function() {
      this.testNotif = new ErrorNotification();
      showNotification(this.testNotif);
    },
    onShown: function(popup) {
      checkPopup(popup, this.testNotif);
      triggerMainCommand(popup);
    },
    onHidden: function(popup) {
      ok(this.testNotif.mainActionClicked, "main action has been triggered");
    }
  },
  // Popup Notifications secondary actions should catch exceptions from callbacks
  { id: "Test#2",
    run: function() {
      this.testNotif = new ErrorNotification();
      showNotification(this.testNotif);
    },
    onShown: function(popup) {
      checkPopup(popup, this.testNotif);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden: function(popup) {
      ok(this.testNotif.secondaryActionClicked, "secondary action has been triggered");
    }
  },
  // Existing popup notification shouldn't disappear when adding a dismissed notification
  { id: "Test#3",
    run: function() {
      this.notifyObj1 = new BasicNotification(this.id);
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notification1 = showNotification(this.notifyObj1);
    },
    onShown: function(popup) {
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
    onHidden: function(popup) {
      this.notification1.remove();
      this.notification2.remove();
    }
  },
  // Showing should be able to modify the popup data
  { id: "Test#4",
    run: function() {
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
    onShown: function(popup) {
      // checkPopup checks for the matching label. Note that this assumes that
      // this.notifyObj.mainAction is the same as notification.mainAction,
      // which could be a problem if we ever decided to deep-copy.
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden: function() { }
  },
  // Moving a tab to a new window should remove non-swappable notifications.
  { id: "Test#5",
    run: function() {
      gBrowser.selectedTab = gBrowser.addTab("about:blank");
      let notifyObj = new BasicNotification(this.id);
      showNotification(notifyObj);
      let win = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
      whenDelayedStartupFinished(win, function() {
        let anchor = win.document.getElementById("default-notification-icon");
        win.PopupNotifications._reshowNotifications(anchor);
        ok(win.PopupNotifications.panel.childNodes.length == 0,
           "no notification displayed in new window");
        ok(notifyObj.swappingCallbackTriggered, "the swapping callback was triggered");
        ok(notifyObj.removedCallbackTriggered, "the removed callback was triggered");
        win.close();
        goNext();
      });
    }
  },
  // Moving a tab to a new window should preserve swappable notifications.
  { id: "Test#6",
    run: function* () {
      let originalBrowser = gBrowser.selectedBrowser;
      let originalWindow = window;

      gBrowser.selectedTab = gBrowser.addTab("about:blank");
      let notifyObj = new BasicNotification(this.id);
      let originalCallback = notifyObj.options.eventCallback;
      notifyObj.options.eventCallback = function(eventName) {
        originalCallback(eventName);
        return eventName == "swapping";
      };

      let notification = showNotification(notifyObj);
      let win = gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
      yield whenDelayedStartupFinished(win);

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

      // These are the same checks that PopupNotifications.jsm makes before it
      // allows a notification to open. Do not go to the next test until we are
      // sure that its attempt to display a notification will not fail.
      yield BrowserTestUtils.waitForCondition(() => originalBrowser.docShellIsActive,
                                              "The browser should be active");
      let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
      yield BrowserTestUtils.waitForCondition(() => fm.activeWindow == originalWindow,
                                              "The window should be active")

      goNext();
    }
  },
  // the hideNotNow option
  { id: "Test#7",
    run: function() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.hideNotNow = true;
      this.notifyObj.mainAction.dismiss = true;
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      // checkPopup verifies that the Not Now item is hidden, and that no separator is added.
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden: function(popup) {
      this.notification.remove();
    }
  },
  // the main action callback can keep the notification.
  { id: "Test#8",
    run: function() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.mainAction.dismiss = true;
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);
    },
    onHidden: function(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback was triggered");
      ok(!this.notifyObj.removedCallbackTriggered, "removed callback wasn't triggered");
      this.notification.remove();
    }
  },
  // a secondary action callback can keep the notification.
  { id: "Test#9",
    run: function() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.secondaryActions[0].dismiss = true;
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden: function(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback was triggered");
      ok(!this.notifyObj.removedCallbackTriggered, "removed callback wasn't triggered");
      this.notification.remove();
    }
  },
  // returning true in the showing callback should dismiss the notification.
  { id: "Test#10",
    run: function() {
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
  },
  // panel updates should fire the showing and shown callbacks again.
  { id: "Test#11",
    run: function() {
      this.notifyObj = new BasicNotification(this.id);
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
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
    onHidden: function() { }
  },
  // A first dismissed notification shouldn't stop _update from showing a second notification
  { id: "Test#12",
    run: function() {
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
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj2);
      this.notification1.remove();
      this.notification2.remove();
    },
    onHidden: function(popup) { }
  },
  // The anchor icon should be shown for notifications in background windows.
  { id: "Test#13",
    run: function() {
      let notifyObj = new BasicNotification(this.id);
      notifyObj.options.dismissed = true;
      let win = gBrowser.replaceTabWithWindow(gBrowser.addTab("about:blank"));
      whenDelayedStartupFinished(win, function() {
        showNotification(notifyObj);
        let anchor = document.getElementById("default-notification-icon");
        is(anchor.getAttribute("showing"), "true", "the anchor is shown");
        win.close();
        goNext();
      });
    }
  }
];
