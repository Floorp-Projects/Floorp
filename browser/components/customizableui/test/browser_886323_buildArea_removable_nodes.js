/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kButtonId = "test-886323-removable-moved-node";
const kLazyAreaId = "test-886323-lazy-area-for-removability-testing";

var gNavBar = document.getElementById(CustomizableUI.AREA_NAVBAR);
var gLazyArea;

// Removable nodes shouldn't be moved by buildArea
add_task(async function() {
  let dummyBtn = createDummyXULButton(kButtonId, "Dummy");
  dummyBtn.setAttribute("removable", "true");
  CustomizableUI.getCustomizationTarget(gNavBar).appendChild(dummyBtn);
  let popupSet = document.getElementById("mainPopupSet");
  gLazyArea = document.createXULElement("panel");
  gLazyArea.id = kLazyAreaId;
  gLazyArea.hidden = true;
  popupSet.appendChild(gLazyArea);
  CustomizableUI.registerArea(kLazyAreaId, {
    type: CustomizableUI.TYPE_MENU_PANEL,
    defaultPlacements: [],
  });
  CustomizableUI.addWidgetToArea(kButtonId, kLazyAreaId);
  assertAreaPlacements(
    kLazyAreaId,
    [kButtonId],
    "Placements should have changed because widget is removable."
  );
  let btn = document.getElementById(kButtonId);
  btn.setAttribute("removable", "false");
  gLazyArea._customizationTarget = gLazyArea;
  CustomizableUI.registerToolbarNode(gLazyArea, []);
  assertAreaPlacements(
    kLazyAreaId,
    [],
    "Placements should no longer include widget."
  );
  is(
    btn.parentNode.id,
    CustomizableUI.getCustomizationTarget(gNavBar).id,
    "Button shouldn't actually have moved as it's not removable"
  );
  btn = document.getElementById(kButtonId);
  if (btn) {
    btn.remove();
  }
  CustomizableUI.removeWidgetFromArea(kButtonId);
  CustomizableUI.unregisterArea(kLazyAreaId);
  gLazyArea.remove();
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
