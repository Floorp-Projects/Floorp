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
  // Test checkbox being checked by default
  {
    id: "without_learn_more",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      let link = notification.querySelector(
        ".popup-notification-learnmore-link"
      );
      ok(!link.href, "no href");
      is(
        window.getComputedStyle(link).getPropertyValue("display"),
        "none",
        "link hidden"
      );
      dismissNotification(popup);
    },
    onHidden() {},
  },

  // Test that passing the learnMoreURL field sets up the link.
  {
    id: "with_learn_more",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.learnMoreURL = "https://mozilla.org";
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      let link = notification.querySelector(
        ".popup-notification-learnmore-link"
      );
      is(link.textContent, "Learn more", "correct label");
      is(link.href, "https://mozilla.org", "correct href");
      isnot(
        window.getComputedStyle(link).getPropertyValue("display"),
        "none",
        "link not hidden"
      );
      dismissNotification(popup);
    },
    onHidden() {},
  },
];
