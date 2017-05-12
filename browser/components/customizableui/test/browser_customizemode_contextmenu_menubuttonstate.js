"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should not be 'pressed' outside customize mode");
  await startCustomizing();

  is(PanelUI.menuButton.getAttribute("open"), "true", "Menu button should be 'pressed' when in customize mode");

  let contextMenu = document.getElementById("customizationPanelItemContextMenu");
  let shownPromise = popupShown(contextMenu);
  let newWindowButton = document.getElementById("wrapper-new-window-button");
  EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
  await shownPromise;
  is(PanelUI.menuButton.getAttribute("open"), "true", "Menu button should be 'pressed' when in customize mode after opening a context menu");

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;
  is(PanelUI.menuButton.getAttribute("open"), "true", "Menu button should be 'pressed' when in customize mode after hiding a context menu");
  await endCustomizing();

  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should not be 'pressed' after ending customize mode");
});

