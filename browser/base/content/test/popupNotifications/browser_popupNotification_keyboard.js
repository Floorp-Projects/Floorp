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
  // Test that for persistent notifications,
  // the secondary action is triggered by pressing the escape key.
  { id: "Test#1",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.persistent = true;
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    },
    onHidden(popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked");
      ok(this.notifyObj.secondaryActionClicked, "secondaryAction was clicked");
      ok(!this.notifyObj.dismissalCallbackTriggered, "dismissal callback wasn't triggered");
      ok(this.notifyObj.removedCallbackTriggered, "removed callback triggered");
    }
  },
  // Test that for non-persistent notifications, the escape key dismisses the notification.
  { id: "Test#2",
    *run() {
      yield waitForWindowReadyForPopupNotifications(window);
      this.notifyObj = new BasicNotification(this.id);
      this.notification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    },
    onHidden(popup) {
      ok(!this.notifyObj.mainActionClicked, "mainAction was not clicked");
      ok(!this.notifyObj.secondaryActionClicked, "secondaryAction was not clicked");
      ok(this.notifyObj.dismissalCallbackTriggered, "dismissal callback triggered");
      ok(!this.notifyObj.removedCallbackTriggered, "removed callback was not triggered");
      this.notification.remove();
    }
  },
  // Test that the space key on an anchor element focuses an active notification
  { id: "Test#3",
    *run() {
      yield SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.anchorID = "geo-notification-icon";
      this.notifyObj.addOptions({
        persistent: true,
      });
      this.notification = showNotification(this.notifyObj);
    },
    *onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let anchor = document.getElementById(this.notifyObj.anchorID);
      anchor.focus();
      is(document.activeElement, anchor);
      EventUtils.synthesizeKey(" ", {});
      is(document.activeElement, popup.childNodes[0].closebutton);
      this.notification.remove();
    },
    onHidden(popup) { }
  },
  // Test that you can switch between active notifications with the space key
  // and that the notification is focused on selection.
  { id: "Test#4",
    *run() {
      yield SpecialPowers.pushPrefEnv({"set": [["accessibility.tabfocus", 7]]});

      let notifyObj1 = new BasicNotification(this.id);
      notifyObj1.id += "_1";
      notifyObj1.anchorID = "default-notification-icon";
      notifyObj1.addOptions({
        hideClose: true,
        checkbox: {
          label: "Test that elements inside the panel can be focused",
        },
        persistent: true,
      });
      let opened = waitForNotificationPanel();
      let notification1 = showNotification(notifyObj1);
      yield opened;

      let notifyObj2 = new BasicNotification(this.id);
      notifyObj2.id += "_2";
      notifyObj2.anchorID = "geo-notification-icon";
      notifyObj2.addOptions({
        persistent: true,
      });
      opened = waitForNotificationPanel();
      let notification2 = showNotification(notifyObj2);
      let popup = yield opened;

      // Make sure notification 2 is visible
      checkPopup(popup, notifyObj2);

      // Activate the anchor for notification 1 and wait until it's shown.
      let anchor = document.getElementById(notifyObj1.anchorID);
      anchor.focus();
      is(document.activeElement, anchor);
      opened = waitForNotificationPanel();
      EventUtils.synthesizeKey(" ", {});
      popup = yield opened;
      checkPopup(popup, notifyObj1);

      is(document.activeElement, popup.childNodes[0].checkbox);

      // Activate the anchor for notification 2 and wait until it's shown.
      anchor = document.getElementById(notifyObj2.anchorID);
      anchor.focus();
      is(document.activeElement, anchor);
      opened = waitForNotificationPanel();
      EventUtils.synthesizeKey(" ", {});
      popup = yield opened;
      checkPopup(popup, notifyObj2);

      is(document.activeElement, popup.childNodes[0].closebutton);

      notification1.remove();
      notification2.remove();
      goNext();
    },
  },
];
