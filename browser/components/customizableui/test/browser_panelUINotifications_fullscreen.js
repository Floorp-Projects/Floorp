"use strict";

Cu.import("resource://gre/modules/AppMenuNotifications.jsm");

add_task(async function testFullscreen() {
  let doc = document;

  is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
  let mainActionCalled = false;
  let mainAction = {
    callback: () => { mainActionCalled = true; }
  };
  AppMenuNotifications.showNotification("update-manual", mainAction);

  isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
  let notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
  is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
  let doorhanger = notifications[0];
  is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popuphidden");
  EventUtils.synthesizeKey("VK_F11", {});
  await popuphiddenPromise;
  await new Promise(executeSoon);
  is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");

  FullScreen.showNavToolbox();
  is(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is displaying on PanelUI button.");

  let popupshownPromise = BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popupshown");
  EventUtils.synthesizeKey("VK_F11", {});
  await popupshownPromise;
  await new Promise(executeSoon);
  isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
  isnot(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is not displaying on PanelUI button.");

  let mainActionButton = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "button");
  mainActionButton.click();
  ok(mainActionCalled, "Main action callback was called");
  is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
  is(PanelUI.menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
});
