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
  // Test optional params
  { id: "Test#1",
    run: function() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.secondaryActions = undefined;
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test that icons appear
  { id: "Test#2",
    run: function() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.id = "geolocation";
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");
      dismissNotification(popup);
    },
    onHidden: function(popup) {
      let icon = document.getElementById("geo-notification-icon");
      isnot(icon.boxObject.width, 0,
            "geo anchor should be visible after dismissal");
      this.notification.remove();
      is(icon.boxObject.width, 0,
         "geo anchor should not be visible after removal");
    }
  },

  // Test that persistence allows the notification to persist across reloads
  { id: "Test#3",
    run: function* () {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.addOptions({
        persistence: 2
      });
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function* (popup) {
      this.complete = false;
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.org/");
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/")
      // Next load will remove the notification
      this.complete = true;
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.org/");
    },
    onHidden: function(popup) {
      ok(this.complete, "Should only have hidden the notification after 3 page loads");
      ok(this.notifyObj.removedCallbackTriggered, "removal callback triggered");
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that a timeout allows the notification to persist across reloads
  { id: "Test#4",
    run: function* () {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");
      this.notifyObj = new BasicNotification(this.id);
      // Set a timeout of 10 minutes that should never be hit
      this.notifyObj.addOptions({
        timeout: Date.now() + 600000
      });
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function* (popup) {
      this.complete = false;
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.org/");
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");
      // Next load will hide the notification
      this.notification.options.timeout = Date.now() - 1;
      this.complete = true;
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.org/");
    },
    onHidden: function(popup) {
      ok(this.complete, "Should only have hidden the notification after the timeout was passed");
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },
  // Test that setting persistWhileVisible allows a visible notification to
  // persist across location changes
  { id: "Test#5",
    run: function* () {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.addOptions({
        persistWhileVisible: true
      });
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function* (popup) {
      this.complete = false;

      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.org/");
      yield promiseTabLoadEvent(gBrowser.selectedTab, "http://example.com/");
      // Notification should persist across location changes
      this.complete = true;
      dismissNotification(popup);
    },
    onHidden: function(popup) {
      ok(this.complete, "Should only have hidden the notification after it was dismissed");
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  },

  // Test that nested icon nodes correctly activate popups
  { id: "Test#6",
    run: function() {
      // Add a temporary box as the anchor with a button
      this.box = document.createElement("box");
      PopupNotifications.iconBox.appendChild(this.box);

      let button = document.createElement("button");
      button.setAttribute("label", "Please click me!");
      this.box.appendChild(button);

      // The notification should open up on the box
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = this.box.id = "nested-box";
      this.notifyObj.addOptions({dismissed: true});
      this.notification = showNotification(this.notifyObj);

      // This test places a normal button in the notification area, which has
      // standard GTK styling and dimensions. Due to the clip-path, this button
      // gets clipped off, which makes it necessary to synthesize the mouse click
      // a little bit downward. To be safe, I adjusted the x-offset with the same
      // amount.
      EventUtils.synthesizeMouse(button, 4, 4, {});
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function(popup) {
      this.notification.remove();
      this.box.parentNode.removeChild(this.box);
    }
  },
  // Test that popupnotifications without popups have anchor icons shown
  { id: "Test#7",
    run: function* () {
      let notifyObj = new BasicNotification(this.id);
      notifyObj.anchorID = "geo-notification-icon";
      notifyObj.addOptions({neverShow: true});
      let promiseTopic = promiseTopicObserved("PopupNotifications-updateNotShowing");
      showNotification(notifyObj);
      yield promiseTopic;
      isnot(document.getElementById("geo-notification-icon").boxObject.width, 0,
            "geo anchor should be visible");
      goNext();
    }
  },
  // Test notification close button
  { id: "Test#9",
    run: function() {
      this.notifyObj = new BasicNotification(this.id);
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];
      EventUtils.synthesizeMouseAtCenter(notification.closebutton, {});
    },
    onHidden: function(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test notification when chrome is hidden
  { id: "Test#10",
    run: function() {
      window.locationbar.visible = false;
      this.notifyObj = new BasicNotification(this.id);
      this.notification = showNotification(this.notifyObj);
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      is(popup.anchorNode.className, "tabbrowser-tab", "notification anchored to tab");
      dismissNotification(popup);
    },
    onHidden: function(popup) {
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      this.notification.remove();
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
      window.locationbar.visible = true;
    }
  },
  // Test that dismissed popupnotifications can be opened on about:blank
  // (where the rest of the identity block is disabled)
  { id: "Test#11",
    run: function() {
      this.oldSelectedTab = gBrowser.selectedTab;
      gBrowser.selectedTab = gBrowser.addTab("about:blank");

      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({dismissed: true});
      this.notification = showNotification(this.notifyObj);

      EventUtils.synthesizeMouse(document.getElementById("geo-notification-icon"), 0, 0, {});
    },
    onShown: function(popup) {
      checkPopup(popup, this.notifyObj);
      dismissNotification(popup);
    },
    onHidden: function(popup) {
      this.notification.remove();
      gBrowser.removeTab(gBrowser.selectedTab);
      gBrowser.selectedTab = this.oldSelectedTab;
    }
  }
];
