/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const BUTTONID = "test-widget-saved-earlier";
const AREAID = "test-area-saved-earlier";

var hadSavedState;
function test() {
  let gSavedState = CustomizableUI.getTestOnlyInternalProp("gSavedState");
  hadSavedState = gSavedState != null;
  if (!hadSavedState) {
    gSavedState = { placements: {} };
    CustomizableUI.setTestOnlyInternalProp("gSavedState", gSavedState);
  }
  gSavedState.placements[AREAID] = [BUTTONID];
  // Put bogus stuff in the saved state for the nav-bar, so as to check the current placements
  // override this one...
  gSavedState.placements[CustomizableUI.AREA_NAVBAR] = ["bogus-navbar-item"];

  CustomizableUI.setTestOnlyInternalProp("gDirty", true);
  CustomizableUI.getTestOnlyInternalProp("CustomizableUIInternal").saveState();

  let newSavedState = JSON.parse(
    Services.prefs.getCharPref("browser.uiCustomization.state")
  );
  let savedArea = Array.isArray(newSavedState.placements[AREAID]);
  ok(
    savedArea,
    "Should have re-saved the state, even though the area isn't registered"
  );

  if (savedArea) {
    placementArraysEqual(AREAID, newSavedState.placements[AREAID], [BUTTONID]);
  }
  ok(
    !CustomizableUI.getTestOnlyInternalProp("gPlacements").has(AREAID),
    "Placements map shouldn't have been affected"
  );

  let savedNavbar = Array.isArray(
    newSavedState.placements[CustomizableUI.AREA_NAVBAR]
  );
  ok(savedNavbar, "Should have saved nav-bar contents");
  if (savedNavbar) {
    placementArraysEqual(
      CustomizableUI.AREA_NAVBAR,
      newSavedState.placements[CustomizableUI.AREA_NAVBAR],
      CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR)
    );
  }
}

registerCleanupFunction(function () {
  if (!hadSavedState) {
    CustomizableUI.setTestOnlyInternalProp("gSavedState", null);
  } else {
    let gSavedState = CustomizableUI.getTestOnlyInternalProp("gSavedState");
    let savedPlacements = gSavedState.placements;
    delete savedPlacements[AREAID];
    let realNavBarPlacements = CustomizableUI.getWidgetIdsInArea(
      CustomizableUI.AREA_NAVBAR
    );
    savedPlacements[CustomizableUI.AREA_NAVBAR] = realNavBarPlacements;
  }
  CustomizableUI.setTestOnlyInternalProp("gDirty", true);
  CustomizableUI.getTestOnlyInternalProp("CustomizableUIInternal").saveState();
});
