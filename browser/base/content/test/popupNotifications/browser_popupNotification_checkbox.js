/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  setup();
}

function checkCheckbox(checkbox, label, checked = false, hidden = false) {
  is(checkbox.label, label, "Checkbox should have the correct label");
  is(checkbox.hidden, hidden, "Checkbox should be shown");
  is(checkbox.checked, checked, "Checkbox should be checked by default");
}

function checkMainAction(notification, disabled = false) {
  let mainAction = notification.button;
  let warningLabel = notification.querySelector(".popup-notification-warning");
  is(warningLabel.hidden, !disabled, "Warning label should be shown");
  is(mainAction.disabled, disabled, "MainAction should be disabled");
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
  // Test that passing the checkbox field shows the checkbox.
  {
    id: "show_checkbox",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
      };
      gNotification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      checkCheckbox(notification.checkbox, "This is a checkbox");
      triggerMainCommand(popup);
    },
    onHidden() {},
  },

  // Test checkbox being checked by default
  {
    id: "checkbox_checked",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.checkbox = {
        label: "Check this",
        checked: true,
      };
      gNotification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      checkCheckbox(notification.checkbox, "Check this", true);
      triggerMainCommand(popup);
    },
    onHidden() {},
  },

  // Test checkbox passing the checkbox state on mainAction
  {
    id: "checkbox_passCheckboxChecked_mainAction",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.mainAction.callback = ({ checkboxChecked }) =>
        (this.mainActionChecked = checkboxChecked);
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
      };
      gNotification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox");
      await promiseElementVisible(checkbox);
      EventUtils.synthesizeMouseAtCenter(checkbox, {});
      checkCheckbox(checkbox, "This is a checkbox", true);
      triggerMainCommand(popup);
    },
    onHidden() {
      is(
        this.mainActionChecked,
        true,
        "mainAction callback is passed the correct checkbox value"
      );
    },
  },

  // Test checkbox passing the checkbox state on secondaryAction
  {
    id: "checkbox_passCheckboxChecked_secondaryAction",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.secondaryActions = [
        {
          label: "Test Secondary",
          accessKey: "T",
          callback: ({ checkboxChecked }) =>
            (this.secondaryActionChecked = checkboxChecked),
        },
      ];
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
      };
      gNotification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox");
      await promiseElementVisible(checkbox);
      EventUtils.synthesizeMouseAtCenter(checkbox, {});
      checkCheckbox(checkbox, "This is a checkbox", true);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden() {
      is(
        this.secondaryActionChecked,
        true,
        "secondaryAction callback is passed the correct checkbox value"
      );
    },
  },

  // Test checkbox preserving its state through re-opening the doorhanger
  {
    id: "checkbox_reopen",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
        checkedState: {
          disableMainAction: true,
          warningLabel: "Testing disable",
        },
      };
      gNotification = showNotification(this.notifyObj);
    },
    async onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox");
      await promiseElementVisible(checkbox);
      EventUtils.synthesizeMouseAtCenter(checkbox, {});
      dismissNotification(popup);
    },
    async onHidden(popup) {
      let icon = document.getElementById("default-notification-icon");
      let shown = waitForNotificationPanel();
      EventUtils.synthesizeMouseAtCenter(icon, {});
      await shown;
      let notification = popup.children[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox", true);
      checkMainAction(notification, true);
      gNotification.remove();
    },
  },

  // Test no checkbox hides warning label
  {
    id: "no_checkbox",
    run() {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.checkbox = null;
      gNotification = showNotification(this.notifyObj);
    },
    onShown(popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.children[0];
      checkCheckbox(notification.checkbox, "", false, true);
      checkMainAction(notification);
      triggerMainCommand(popup);
    },
    onHidden() {},
  },
];

// Test checkbox disabling the main action in different combinations
["checkedState", "uncheckedState"].forEach(function(state) {
  [true, false].forEach(function(checked) {
    tests.push({
      id: `checkbox_disableMainAction_${state}_${
        checked ? "checked" : "unchecked"
      }`,
      run() {
        this.notifyObj = new BasicNotification(this.id);
        this.notifyObj.options.checkbox = {
          label: "This is a checkbox",
          checked,
          [state]: {
            disableMainAction: true,
            warningLabel: "Testing disable",
          },
        };
        gNotification = showNotification(this.notifyObj);
      },
      async onShown(popup) {
        checkPopup(popup, this.notifyObj);
        let notification = popup.children[0];
        let checkbox = notification.checkbox;
        let disabled =
          (state === "checkedState" && checked) ||
          (state === "uncheckedState" && !checked);

        checkCheckbox(checkbox, "This is a checkbox", checked);
        checkMainAction(notification, disabled);
        await promiseElementVisible(checkbox);
        EventUtils.synthesizeMouseAtCenter(checkbox, {});
        checkCheckbox(checkbox, "This is a checkbox", !checked);
        checkMainAction(notification, !disabled);
        EventUtils.synthesizeMouseAtCenter(checkbox, {});
        checkCheckbox(checkbox, "This is a checkbox", checked);
        checkMainAction(notification, disabled);

        // Unblock the main command if it's currently disabled.
        if (disabled) {
          EventUtils.synthesizeMouseAtCenter(checkbox, {});
        }
        triggerMainCommand(popup);
      },
      onHidden() {},
    });
  });
});
