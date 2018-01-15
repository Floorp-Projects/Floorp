/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
}

let buttonPressed = false;

function commandTriggered() {
  buttonPressed = true;
}

var tests = [
  // This test ensures that the accesskey closes the popup.
  { id: "Test#1",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      showNotification(this.notifyObj);
    },
    onShown(popup) {
      window.addEventListener("command", commandTriggered, true);
      checkPopup(popup, this.notifyObj);
      EventUtils.synthesizeKey("VK_ALT", { type: "keydown" });
      EventUtils.synthesizeKey("M", { altKey: true });
      EventUtils.synthesizeKey("VK_ALT", { type: "keyup" });

      // If bug xxx was present, then the popup would be in the
      // process of being hidden right now.
      isnot(popup.state, "hiding", "popup is not hiding");
    },
    onHidden(popup) {
      window.removeEventListener("command", commandTriggered, true);
      ok(buttonPressed, "button pressed");
    }
  }
 ];
