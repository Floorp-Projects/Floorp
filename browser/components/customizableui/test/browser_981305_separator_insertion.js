/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var tempElements = [];

function insertTempItemsIntoMenu(parentMenu) {
  // Last element is null to insert at the end:
  let beforeEls = [
    parentMenu.firstElementChild,
    parentMenu.lastElementChild,
    null,
  ];
  for (let i = 0; i < beforeEls.length; i++) {
    let sep = document.createXULElement("menuseparator");
    tempElements.push(sep);
    parentMenu.insertBefore(sep, beforeEls[i]);
    let menu = document.createXULElement("menu");
    tempElements.push(menu);
    parentMenu.insertBefore(menu, beforeEls[i]);
    // And another separator for good measure:
    sep = document.createXULElement("menuseparator");
    tempElements.push(sep);
    parentMenu.insertBefore(sep, beforeEls[i]);
  }
}

async function checkSeparatorInsertion(menuId, buttonId, subviewId) {
  info("Checking for duplicate separators in " + buttonId + " widget");
  let menu = document.getElementById(menuId);
  insertTempItemsIntoMenu(menu);

  CustomizableUI.addWidgetToArea(
    buttonId,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();

  let button = document.getElementById(buttonId);
  button.click();
  let subview = document.getElementById(subviewId);
  await BrowserTestUtils.waitForEvent(subview, "ViewShown");

  let subviewBody = subview.firstElementChild;
  ok(subviewBody.firstElementChild, "Subview should have a kid");
  is(
    subviewBody.firstElementChild.localName,
    "toolbarbutton",
    "There should be no separators to start with"
  );

  for (let kid of subviewBody.children) {
    if (kid.localName == "menuseparator") {
      ok(
        kid.previousElementSibling &&
          kid.previousElementSibling.localName != "menuseparator",
        "Separators should never have another separator next to them, and should never be the first node."
      );
    }
  }

  let panelHiddenPromise = promiseOverflowHidden(window);
  PanelUI.overflowPanel.hidePopup();
  await panelHiddenPromise;

  CustomizableUI.reset();
}

add_task(async function check_devtools_separator() {
  const protonEnabled =
    Services.prefs.getBoolPref("browser.proton.doorhangers.enabled", false) &&
    Services.prefs.getBoolPref("browser.proton.enabled", false);
  const panelviewId = protonEnabled ? "appmenu-moreTools" : "PanelUI-developer";

  await checkSeparatorInsertion(
    "menuWebDeveloperPopup",
    "developer-button",
    panelviewId
  );
});

registerCleanupFunction(function() {
  for (let el of tempElements) {
    el.remove();
  }
  tempElements = null;
});
