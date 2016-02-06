/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const BUTTONID = "test-widget-saved-earlier";
const AREAID = "test-area-saved-earlier";

var hadSavedState;
function test() {
  // Hack our way into the module to fake a saved state that isn't there...
  let backstagePass = Cu.import("resource:///modules/CustomizableUI.jsm", {});
  hadSavedState = backstagePass.gSavedState != null;
  if (!hadSavedState) {
    backstagePass.gSavedState = {placements: {}};
  }
  backstagePass.gSavedState.placements[AREAID] = [BUTTONID];
  // Put bogus stuff in the saved state for the nav-bar, so as to check the current placements
  // override this one...
  backstagePass.gSavedState.placements[CustomizableUI.AREA_NAVBAR] = ["bogus-navbar-item"];

  backstagePass.gDirty = true;
  backstagePass.CustomizableUIInternal.saveState();

  let newSavedState = JSON.parse(Services.prefs.getCharPref("browser.uiCustomization.state"));
  let savedArea = Array.isArray(newSavedState.placements[AREAID]);
  ok(savedArea, "Should have re-saved the state, even though the area isn't registered");

  if (savedArea) {
    placementArraysEqual(AREAID, newSavedState.placements[AREAID], [BUTTONID]);
  }
  ok(!backstagePass.gPlacements.has(AREAID), "Placements map shouldn't have been affected");

  let savedNavbar = Array.isArray(newSavedState.placements[CustomizableUI.AREA_NAVBAR]);
  ok(savedNavbar, "Should have saved nav-bar contents");
  if (savedNavbar) {
    placementArraysEqual(CustomizableUI.AREA_NAVBAR, newSavedState.placements[CustomizableUI.AREA_NAVBAR],
                         CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR));
  }
}

registerCleanupFunction(function() {
  let backstagePass = Cu.import("resource:///modules/CustomizableUI.jsm", {});
  if (!hadSavedState) {
    backstagePass.gSavedState = null;
  } else {
    let savedPlacements = backstagePass.gSavedState.placements;
    delete savedPlacements[AREAID];
    let realNavBarPlacements = CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR);
    savedPlacements[CustomizableUI.AREA_NAVBAR] = realNavBarPlacements;
  }
  backstagePass.gDirty = true;
  backstagePass.CustomizableUIInternal.saveState();
});

