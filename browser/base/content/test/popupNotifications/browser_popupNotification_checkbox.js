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

function checkCheckbox(checkbox, label, checked = false, hidden = false) {
  is(checkbox.label, label, "Checkbox should have the correct label");
  is(checkbox.hidden, hidden, "Checkbox should be shown");
  is(checkbox.checked, checked, "Checkbox should be checked by default");
}

function checkMainAction(notification, disabled = false) {
  let mainAction = notification.button;
  let warningLabel = document.getAnonymousElementByAttribute(notification, "class", "popup-notification-warning");
  is(warningLabel.hidden, !disabled, "Warning label should be shown");
  is(mainAction.disabled, disabled, "MainAction should be disabled");
}

function promiseElementVisible(element) {
  // HTMLElement.offsetParent is null when the element is not visisble
  // (or if the element has |position: fixed|). See:
  // https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/offsetParent
  return BrowserTestUtils.waitForCondition(() => element.offsetParent !== null,
                                          "Waiting for element to be visible");
}

var gNotification;

var tests = [
  // Test that passing the checkbox field shows the checkbox.
  { id: "show_checkbox",
    run: function () {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
      };
      gNotification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];
      checkCheckbox(notification.checkbox, "This is a checkbox");
      triggerMainCommand(popup);
    },
    onHidden: function () { }
  },

  // Test checkbox being checked by default
  { id: "checkbox_checked",
    run: function () {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.options.checkbox = {
        label: "Check this",
        checked: true,
      };
      gNotification = showNotification(this.notifyObj);
    },
    onShown: function (popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];
      checkCheckbox(notification.checkbox, "Check this", true);
      triggerMainCommand(popup);
    },
    onHidden: function () { }
  },

  // Test checkbox passing the checkbox state on mainAction
  { id: "checkbox_passCheckboxChecked_mainAction",
    run: function () {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.mainAction.callback = ({checkboxChecked}) => this.mainActionChecked = checkboxChecked;
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
      };
      gNotification = showNotification(this.notifyObj);
    },
    onShown: function* (popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox");
      yield promiseElementVisible(checkbox);
      EventUtils.synthesizeMouseAtCenter(checkbox, {});
      checkCheckbox(checkbox, "This is a checkbox", true);
      triggerMainCommand(popup);
    },
    onHidden: function () {
      is(this.mainActionChecked, true, "mainAction callback is passed the correct checkbox value");
    }
  },

  // Test checkbox passing the checkbox state on secondaryAction
  { id: "checkbox_passCheckboxChecked_secondaryAction",
    run: function () {
      this.notifyObj = new BasicNotification(this.id);
      this.notifyObj.secondaryActions = [{
        label: "Test Secondary",
        accessKey: "T",
        callback: ({checkboxChecked}) => this.secondaryActionChecked = checkboxChecked,
      }];
      this.notifyObj.options.checkbox = {
        label: "This is a checkbox",
      };
      gNotification = showNotification(this.notifyObj);
    },
    onShown: function* (popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox");
      yield promiseElementVisible(checkbox);
      EventUtils.synthesizeMouseAtCenter(checkbox, {});
      checkCheckbox(checkbox, "This is a checkbox", true);
      triggerSecondaryCommand(popup, 0);
    },
    onHidden: function () {
      is(this.secondaryActionChecked, true, "secondaryAction callback is passed the correct checkbox value");
    }
  },

  // Test checkbox preserving its state through re-opening the doorhanger
  { id: "checkbox_reopen",
    run: function () {
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
    onShown: function* (popup) {
      checkPopup(popup, this.notifyObj);
      let notification = popup.childNodes[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox");
      yield promiseElementVisible(checkbox);
      EventUtils.synthesizeMouseAtCenter(checkbox, {});
      dismissNotification(popup);
    },
    onHidden: function (popup) {
      let icon = document.getElementById("default-notification-icon");
      EventUtils.synthesizeMouseAtCenter(icon, {});
      let notification = popup.childNodes[0];
      let checkbox = notification.checkbox;
      checkCheckbox(checkbox, "This is a checkbox", true);
      checkMainAction(notification, true);
      gNotification.remove();
    }
  },
];

// Test checkbox disabling the main action in different combinations
["checkedState", "uncheckedState"].forEach(function (state) {
  [true, false].forEach(function (checked) {
    tests.push(
      { id: `checkbox_disableMainAction_${state}_${checked ? 'checked' : 'unchecked'}`,
        run: function () {
          this.notifyObj = new BasicNotification(this.id);
          this.notifyObj.options.checkbox = {
            label: "This is a checkbox",
            checked: checked,
            [state]: {
              disableMainAction: true,
              warningLabel: "Testing disable",
            },
          };
          gNotification = showNotification(this.notifyObj);
        },
        onShown: function* (popup) {
          checkPopup(popup, this.notifyObj);
          let notification = popup.childNodes[0];
          let checkbox = notification.checkbox;
          let disabled = (state === "checkedState" && checked) ||
                         (state === "uncheckedState" && !checked);

          checkCheckbox(checkbox, "This is a checkbox", checked);
          checkMainAction(notification, disabled);
          yield promiseElementVisible(checkbox);
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
        onHidden: function () { }
      }
    );
  });
});

