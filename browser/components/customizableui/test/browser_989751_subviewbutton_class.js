/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kCustomClass = "acustomclassnoonewilluse";
const kDevPanelId = "PanelUI-developer-tools";
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
    ChromeUtils.import(
      "resource://devtools/shared/loader/Loader.jsm",
      {}
    ).require("devtools/client/framework/devtools-browser");

    info(
      "Checking for items without the subviewbutton class in " +
        buttonId +
        " widget"
    );
    let menu = document.getElementById(menuId);
    insertClassNameToMenuChildren(menu);

    CustomizableUI.addWidgetToArea(
      buttonId,
      CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
    );

    await waitForOverflowButtonShown();

    await document.getElementById("nav-bar").overflowable.show();

    let button = document.getElementById(buttonId);
    button.click();

    await BrowserTestUtils.waitForEvent(PanelUI.overflowPanel, "ViewShown");
    let subview = document.getElementById(subviewId);
    ok(subview.firstElementChild, "Subview should have a kid");

    // The Developer Panel contains the Customize Toolbar item,
    // as well as the Developer Tools items (bug 1703150). We only want to query for
    // the Developer Tools items in this case.
    let query = "#appmenu-developer-tools-view toolbarbutton";
    let subviewchildren = subview.querySelectorAll(query);

    for (let i = 0; i < subviewchildren.length; i++) {
      let item = subviewchildren[i];
      let itemReadable =
        "Item '" + item.label + "' (classes: " + item.className + ")";
      ok(
        item.classList.contains("subviewbutton"),
        itemReadable + " should have the subviewbutton class."
      );
      if (i == 0) {
        ok(
          item.classList.contains(kCustomClass),
          itemReadable + " should still have its own class, too."
        );
      }
    }

    let panelHiddenPromise = promiseOverflowHidden(window);
    PanelUI.overflowPanel.hidePopup();
    await panelHiddenPromise;

    CustomizableUI.reset();
  };
}

add_task(
  checkSubviewButtonClass(
    "menuWebDeveloperPopup",
    "developer-button",
    kDevPanelId
  )
);

registerCleanupFunction(function() {
  tempElement.classList.remove(kCustomClass);
  tempElement = null;
});
