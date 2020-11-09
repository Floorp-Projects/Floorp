/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the telemetry is correct when the tabbing order overlay is
// activated from the accessibility panel.

const TEST_URI = '<h1 id="h1">header</h1><button id="button">Press Me</button>';

async function toggleDisplayTabbingOrder(checkbox, store, checked) {
  info(`Toggling the checkbox ${checked ? "ON" : "OFF"}.`);
  const onCheckboxChecked = BrowserTestUtils.waitForCondition(
    () => checkbox.checked === checked,
    `Checkbox is ${checked ? "checked" : "unchecked"}.`
  );
  const onToggleChange = waitUntilState(
    store,
    state => state.ui.tabbingOrderDisplayed === checked
  );

  checkbox.click();
  await onCheckboxChecked;
  await onToggleChange;
}

addA11YPanelTask(
  "Test that the telemetry is correct when the tabbing order overlay is " +
    "activated from the accessibility panel.",
  TEST_URI,
  async function({ doc, store }) {
    startTelemetry();
    const checkbox = doc.getElementById(
      "devtools-display-tabbing-order-checkbox"
    );
    await toggleDisplayTabbingOrder(checkbox, store, true);
    await toggleDisplayTabbingOrder(checkbox, store, false);
    checkTelemetry(
      "devtools.accessibility.tabbing_order_activated",
      "",
      1,
      "scalar"
    );
  }
);
