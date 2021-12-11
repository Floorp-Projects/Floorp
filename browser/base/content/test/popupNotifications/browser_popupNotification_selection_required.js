/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
}

function promiseElementVisible(element) {
  // HTMLElement.offsetParent is null when the element is not visisble
  // (or if the element has |position: fixed|). See:
  // https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/offsetParent
  return TestUtils.waitForCondition(
    () => element.offsetParent !== null,
    "Waiting for element to be visible"
  );
}

var gNotification;

var tests = [
  // Test that passing selection required prevents the button from clicking
  {
    id: "require_selection_check",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
      };
      gNotification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      notification.setAttribute("invalidselection", true);
      await promiseElementVisible(notification.checkbox);
      EventUtils.synthesizeMouseAtCenter(notification.checkbox, {});
      ok(
        notification.button.disabled,
        "should be disabled when invalidselection"
      );
      notification.removeAttribute("invalidselection");
      EventUtils.synthesizeMouseAtCenter(notification.checkbox, {});
      ok(
        !notification.button.disabled,
        "should not be disabled when invalidselection is not present"
      );
      triggerMainCommand(popup);
    },
    onHidden() {},
  },
];
