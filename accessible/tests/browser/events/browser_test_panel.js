/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// Verify we receive hide and show notifications when the chrome
// XUL alert is closed or opened. Mac expects both notifications to
// properly communicate live region changes.
async function runTests(browser) {
  ok(PopupNotifications, "PopupNotifications object exists");
  ok(PopupNotifications.panel, "PopupNotifications panel exists");

  // When available, the popup panel makes itself a child of the chrome window.
  // To verify it isn't accessible without reproducing the entirety of the chrome
  // window tree, we check instead that the panel is not accessible.
  ok(!isAccessible(PopupNotifications.panel), "Popup panel is not accessible");

  const panelShown = waitForEvent(EVENT_SHOW, PopupNotifications.panel);
  const notification = PopupNotifications.show(
    browser,
    "test-notification",
    "hello world",
    PopupNotifications.panel.id
  );

  await panelShown;

  ok(isAccessible(PopupNotifications.panel), "Popup panel is accessible");
  testAccessibleTree(PopupNotifications.panel, {
    ALERT: [
      { LABEL: [{ TEXT_LEAF: [] }] },
      { PUSHBUTTON: [] },
      { PUSHBUTTON: [] },
    ],
  });
  // Verify the popup panel is associated with the chrome window.
  is(
    PopupNotifications.panel.ownerGlobal,
    getMainChromeWindow(window),
    "Popup panel is associated with the chrome window"
  );

  const panelHidden = waitForEvent(EVENT_HIDE, PopupNotifications.panel);
  PopupNotifications.remove(notification);

  await panelHidden;

  ok(!isAccessible(PopupNotifications.panel), "Popup panel is not accessible");
}

addAccessibleTask(``, runTests);
