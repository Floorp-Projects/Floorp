/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kCustomClass = "acustomclassnoonewilluse";
let tempElement = null;

function insertClassNameToMenuChildren(parentMenu) {
  let el = parentMenu.querySelector("menuitem:first-of-type");
  el.classList.add(kCustomClass);
  tempElement = el;
}

function checkSubviewButtonClass(menuId, buttonId, subviewId) {
  return function() {
    info("Checking for items without the subviewbutton class in " + buttonId + " widget");
    let menu = document.getElementById(menuId);
    insertClassNameToMenuChildren(menu);

    let placement = CustomizableUI.getPlacementOfWidget(buttonId);
    let changedPlacement = false;
    if (!placement || placement.area != CustomizableUI.AREA_PANEL) {
      CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_PANEL);
      changedPlacement = true;
    }
    yield PanelUI.show();

    let button = document.getElementById(buttonId);
    button.click();

    yield waitForCondition(() => !PanelUI.multiView.hasAttribute("transitioning"));
    let subview = document.getElementById(subviewId);
    ok(subview.firstChild, "Subview should have a kid");
    let subviewchildren = subview.querySelectorAll("toolbarbutton");
    for (let i = 0; i < subviewchildren.length; i++) {
      let item = subviewchildren[i];
      let itemReadable = "Item '" + item.label + "' (classes: " + item.className + ")";
      ok(item.classList.contains("subviewbutton"), itemReadable + " should have the subviewbutton class.");
      if (i == 0) {
        ok(item.classList.contains(kCustomClass), itemReadable + " should still have its own class, too.");
      }
    }

    let panelHiddenPromise = promisePanelHidden(window);
    PanelUI.hide();
    yield panelHiddenPromise;

    if (changedPlacement) {
      CustomizableUI.reset();
    }
  };
}

add_task(checkSubviewButtonClass("menuWebDeveloperPopup", "developer-button", "PanelUI-developerItems"));
add_task(checkSubviewButtonClass("viewSidebarMenu", "sidebar-button", "PanelUI-sidebarItems"));

registerCleanupFunction(function() {
  tempElement.classList.remove(kCustomClass)
  tempElement = null;
});
