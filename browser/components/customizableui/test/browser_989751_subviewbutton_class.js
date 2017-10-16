/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kCustomClass = "acustomclassnoonewilluse";
var tempElement = null;

function insertClassNameToMenuChildren(parentMenu) {
  // Skip hidden menuitem elements, not copied via fillSubviewFromMenuItems.
  let el = parentMenu.querySelector("menuitem:not([hidden])");
  el.classList.add(kCustomClass);
  tempElement = el;
}

function checkSubviewButtonClass(menuId, buttonId, subviewId) {
  return async function() {
    // Initialize DevTools before starting the test in order to create menuitems in
    // menuWebDeveloperPopup.
    Cu.import("resource://devtools/shared/Loader.jsm", {})
        .require("devtools/client/framework/devtools-browser");

    info("Checking for items without the subviewbutton class in " + buttonId + " widget");
    let menu = document.getElementById(menuId);
    insertClassNameToMenuChildren(menu);

    CustomizableUI.addWidgetToArea(buttonId, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);

    await waitForOverflowButtonShown();

    await document.getElementById("nav-bar").overflowable.show();

    let button = document.getElementById(buttonId);
    button.click();

    await BrowserTestUtils.waitForEvent(PanelUI.overflowPanel, "ViewShown");
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

    let panelHiddenPromise = promiseOverflowHidden(window);
    PanelUI.overflowPanel.hidePopup();
    await panelHiddenPromise;

    CustomizableUI.reset();
  };
}

add_task(checkSubviewButtonClass("menuWebDeveloperPopup", "developer-button", "PanelUI-developerItems"));

registerCleanupFunction(function() {
  tempElement.classList.remove(kCustomClass);
  tempElement = null;
});
