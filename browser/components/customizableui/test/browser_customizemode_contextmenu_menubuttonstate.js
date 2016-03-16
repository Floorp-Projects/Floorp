"use strict";

add_task(function*() {
  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should not be 'pressed' outside customize mode");
  yield startCustomizing();

  is(PanelUI.menuButton.getAttribute("open"), "true", "Menu button should be 'pressed' when in customize mode");

  let contextMenu = document.getElementById("customizationPanelItemContextMenu");
  let shownPromise = popupShown(contextMenu);
  let newWindowButton = document.getElementById("wrapper-new-window-button");
  EventUtils.synthesizeMouse(newWindowButton, 2, 2, {type: "contextmenu", button: 2});
  yield shownPromise;
  is(PanelUI.menuButton.getAttribute("open"), "true", "Menu button should be 'pressed' when in customize mode after opening a context menu");

  let hiddenContextPromise = popupHidden(contextMenu);
  contextMenu.hidePopup();
  yield hiddenContextPromise;
  is(PanelUI.menuButton.getAttribute("open"), "true", "Menu button should be 'pressed' when in customize mode after hiding a context menu");
  yield endCustomizing();

  ok(!PanelUI.menuButton.hasAttribute("open"), "Menu button should not be 'pressed' after ending customize mode");
});

