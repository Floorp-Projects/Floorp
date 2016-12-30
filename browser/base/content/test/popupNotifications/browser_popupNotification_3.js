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
  // Test notification is removed when dismissed if removeOnDismissal is true
  { id: "Test#1",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.addOptions({
        removeOnDismissal: true
      });
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden(popup) {
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test multiple notification icons are shown
  { id: "Test#2",
    run() {
      this.notifyObj1 = new BasicNotification(this.id);
      this.notifyObj1.id += "_1";
      this.notifyObj1.anchorID = "default-notification-icon";
      this.notification1 = showNotification(this.notifyObj1);

      this.notifyObj2 = new BasicNotification(this.id);
      this.notifyObj2.id += "_2";
      this.notifyObj2.anchorID = "geo-notification-icon";
      this.notification2 = showNotification(this.notifyObj2);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj2);

      // check notifyObj1 anchor icon is showing
      isnot(document.getElementById("default-notification-icon").boxObject.width, 0,
            "default anchor should be visible");
      // check notifyObj2 anchor icon is showing
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");

      dismissNotification(popup);
    },
    onHidden(popup) {
      this.notification1.remove();
      ok(this.notifyObj1.removedCallbackTriggered, "removed callback triggered");

      this.notification2.remove();
      ok(this.notifyObj2.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test that multiple notification icons are removed when switching tabs
  { id: "Test#3",
    *run() {
      // show the notification on old tab.
      this.notifyObjOld = new BasicNotification(this.id);
      this.notifyObjOld.anchorID = "default-notification-icon";
      this.notificationOld = showNotification(this.notifyObjOld);

      // switch tab
      this.oldSelectedTab = gBrowser.selectedTab;
      yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

      // show the notification on new tab.
      this.notifyObjNew = new BasicNotification(this.id);
      this.notifyObjNew.anchorID = "geo-notification-icon";
      this.notificationNew = showNotification(this.notifyObjNew);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObjNew);

      // check notifyObjOld anchor icon is removed
      is(document.getElementById("default-notification-icon").boxObject.width, 0,
         "default anchor shouldn't be visible");
      // check notifyObjNew anchor icon is showing
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");

      dismissNotification(popup);
    },
    onHidden(popup) {
      this.notificationNew.remove();
      gBrowser.removeTab(gBrowser.selectedTab);

      gBrowser.selectedTab = this.oldSelectedTab;
      this.notificationOld.remove();
    }
  },
  // test security delay - too early
  { id: "Test#4",
    run() {
      // Set the security delay to 100s
      PopupNotifications.buttonDelay = 100000;

      this.notifyObj = new BasicNotification(this.id);
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      triggerMainCommand(popup);

      // Wait to see if the main command worked
      executeSoon(function delayedDismissal() {
        dismissNotification(popup);
      });

    },
    onHidden(popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked because it was too soon");
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback was triggered");
    }
  },
  // test security delay - after delay
  { id: "Test#5",
    run() {
      // Set the security delay to 10ms
      PopupNotifications.buttonDelay = 10;

      this.notifyObj = new BasicNotification(this.id);
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);

      // Wait until after the delay to trigger the main action
      setTimeout(function delayedDismissal() {
        triggerMainCommand(popup);
      }, 500);

    },
    onHidden(popup) {
      ok(this.notifyObj.mainActionClicked, "mainAction was clicked after the delay");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback was not triggered");
      PopupNotifications.buttonDelay = PREF_SECURITY_DELAY_INITIAL;
    }
  },
  // reload removes notification
  { id: "Test#6",
    *run() {
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");
      let notifyObj = new BasicNotification(this.id);
      notifyObj.options.eventCallback = function(eventName) {
        if (eventName == "removed") {
          ok(true, "Notification removed in background tab after reloading");
          goNext();
        }
      };
      showNotification(notifyObj);
      executeSoon(function() {
        gBrowser.selectedBrowser.reload();
      });
    }
  },
  // location change in background tab removes notification
  { id: "Test#7",
    *run() {
      let oldSelectedTab = gBrowser.selectedTab;
      let newTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
      gBrowser.selectedTab = oldSelectedTab;
      let browser = gBrowser.getBrowserForTab(newTab);

      let notifyObj = new BasicNotification(this.id);
      notifyObj.browser = browser;
      notifyObj.options.eventCallback = function(eventName) {
        if (eventName == "removed") {
          ok(true, "Notification removed in background tab after reloading");
          executeSoon(function() {
            gBrowser.removeTab(newTab);
            goNext();
          });
        }
      };
      showNotification(notifyObj);
      executeSoon(function() {
        browser.reload();
      });
    }
  },
  // Popup notification anchor shouldn't disappear when a notification with the same ID is re-added in a background tab
  { id: "Test#8",
    *run() {
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");
      let originalTab = gBrowser.selectedTab;
      let bgTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
      let anchor = document.createElement("box");
      anchor.id = "test26-anchor";
      anchor.className = "notification-anchor-icon";
      PopupNotifications.iconBox.appendChild(anchor);

      gBrowser.selectedTab = originalTab;

      let fgNotifyObj = new BasicNotification(this.id);
      fgNotifyObj.anchorID = anchor.id;
      fgNotifyObj.options.dismissed = true;
      let fgNotification = showNotification(fgNotifyObj);

      let bgNotifyObj = new BasicNotification(this.id);
      bgNotifyObj.anchorID = anchor.id;
      bgNotifyObj.browser = gBrowser.getBrowserForTab(bgTab);
      // show the notification in the background tab ...
      let bgNotification = showNotification(bgNotifyObj);
      // ... and re-show it
      bgNotification = showNotification(bgNotifyObj);

      ok(fgNotification.id, "notification has id");
      is(fgNotification.id, bgNotification.id, "notification ids are the same");
      is(anchor.getAttribute("showing"), "true", "anchor still showing");

      fgNotification.remove();
      gBrowser.removeTab(bgTab);
      goNext();
    }
  },
  // location change in an embedded frame should not remove a notification
  { id: "Test#9",
    *run() {
      yield promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html;charset=utf8,<iframe%20id='iframe'%20src='http://example.com/'>");
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.eventCallback = function(eventName) {
        if (eventName == "removed") {
          ok(false, "Notification removed from browser when subframe navigated");
        }
      };
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      let self = this;
      let progressListener = {
        onLocationChange: function onLocationChange() {
          gBrowser.removeProgressListener(progressListener);

	  executeSoon(() => {
            let notification = PopupNotifications.getNotification(self.notifyObj.id,
                                                                  self.notifyObj.browser);
            ok(notification != null, "Notification remained when subframe navigated");
            self.notifyObj.options.eventCallback = undefined;

            notification.remove();
	  });
        },
      };

      info("Adding progress listener and performing navigation");
      gBrowser.addProgressListener(progressListener);
      content.document.getElementById("iframe")
                      .setAttribute("src", "http://example.org/");
    },
    onHidden() {}
  },
  // Popup Notifications should catch exceptions from callbacks
  { id: "Test#10",
    run() {
      this.testNotif1 = new BasicNotification(this.id);
      this.testNotif1.message += " 1";
      this.notification1 = showNotification(this.testNotif1);
      this.testNotif1.options.eventCallback = function(eventName) {
        info("notifyObj1.options.eventCallback: " + eventName);
        if (eventName == "dismissed") {
          throw new Error("Oops 1!");
        }
      };

      this.testNotif2 = new BasicNotification(this.id);
      this.testNotif2.message += " 2";
      this.testNotif2.id += "-2";
      this.testNotif2.options.eventCallback = function(eventName) {
        info("notifyObj2.options.eventCallback: " + eventName);
        if (eventName == "dismissed") {
          throw new Error("Oops 2!");
        }
      };
      this.notification2 = showNotification(this.testNotif2);
    },
    onShown(popup) {
      is(popup.childNodes.length, 2, "two notifications are shown");
      dismissNotification(popup);
    },
    onHidden() {
      this.notification1.remove();
      this.notification2.remove();
    }
  }
];
