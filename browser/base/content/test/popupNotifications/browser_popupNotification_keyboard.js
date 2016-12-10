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
];
