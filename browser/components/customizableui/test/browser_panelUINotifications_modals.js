/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppMenuNotifications } = ChromeUtils.import(
  "resource://gre/modules/AppMenuNotifications.jsm"
);

add_task(async function testModals() {
  await SpecialPowers.pushPrefEnv({
    set: [["prompts.windowPromptSubDialog", true]],
  });

  is(
    PanelUI.notificationPanel.state,
    "closed",
    "update-manual doorhanger is closed."
  );
  let mainActionCalled = false;
  let mainAction = {
    callback: () => {
      mainActionCalled = true;
    },
  };
  AppMenuNotifications.showNotification("update-manual", mainAction);

  isnot(
    PanelUI.notificationPanel.state,
    "closed",
    "update-manual doorhanger is showing."
  );
  let notifications = [...PanelUI.notificationPanel.children].filter(
    n => !n.hidden
  );
  is(
    notifications.length,
    1,
    "PanelUI doorhanger is only displaying one notification."
  );
  let doorhanger = notifications[0];
  is(
    doorhanger.id,
    "appMenu-update-manual-notification",
    "PanelUI is displaying the update-manual notification."
  );

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    PanelUI.notificationPanel,
    "popuphidden"
  );

  let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen("accept");
  Services.prompt.asyncAlert(
    window.browsingContext,
    Services.prompt.MODAL_TYPE_INTERNAL_WINDOW,
    "Test alert",
    "Test alert description"
  );
  await popuphiddenPromise;
  is(
    PanelUI.notificationPanel.state,
    "closed",
    "update-manual doorhanger is closed."
  );

  let popupshownPromise = BrowserTestUtils.waitForEvent(
    PanelUI.notificationPanel,
    "popupshown"
  );

  await dialogPromise;
  await popupshownPromise;
  isnot(
    PanelUI.notificationPanel.state,
    "closed",
    "update-manual doorhanger is showing."
  );

  doorhanger.button.click();
  ok(mainActionCalled, "Main action callback was called");
  is(
    PanelUI.notificationPanel.state,
    "closed",
    "update-manual doorhanger is closed."
  );
});
