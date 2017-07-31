"use strict";

add_task(async function() {
  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should not be 'pressed' outside customize mode");
  ok(!PanelUI.menuButton.hasAttribute("disabled"), "Menu button should not be disabled outside of customize mode");
  await startCustomizing();

  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should still not be 'pressed' when in customize mode");
  is(PanelUI.menuButton.getAttribute("disabled"), "true", "Menu button should be disabled in customize mode");

  let contextMenu = document.getElementById("customizationPaletteItemContextMenu");
  let shownPromise = popupShown(contextMenu);
  let newWindowButton = document.getElementById("wrapper-new-window-button");
  EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
  await shownPromise;
  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should still not be 'pressed' when in customize mode after opening a context menu");
  is(PanelUI.menuButton.getAttribute("disabled"), "true", "Menu button should still be disabled in customize mode");
  is(PanelUI.menuButton.getAttribute("disabled"), "true", "Menu button should still be disabled in customize mode after opening context menu");

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  await hiddenContextPromise;
  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should still not be 'pressed' when in customize mode after hiding a context menu");
  is(PanelUI.menuButton.getAttribute("disabled"), "true", "Menu button should still be disabled in customize mode after hiding context menu");
  await endCustomizing();

  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should not be 'pressed' after ending customize mode");
  ok(!PanelUI.menuButton.hasAttribute("disabled"), "Menu button should not be disabled after ending customize mode");
});

