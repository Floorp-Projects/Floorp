/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kLazyAreaId = "test-890262-lazy-area";
const kWidget1Id  = "test-890262-widget1";
const kWidget2Id  = "test-890262-widget2";

setupArea();

// Destroying a widget after defaulting it to a non-legacy area should work.
add_task(function() {
  CustomizableUI.createWidget({
    id: kWidget1Id,
    removable: true,
    defaultArea: kLazyAreaId
  });
  let noError = true;
  try {
    CustomizableUI.destroyWidget(kWidget1Id);
  } catch (ex) {
    Cu.reportError(ex);
    noError = false;
  }
  ok(noError, "Shouldn't throw an exception for a widget that was created in a not-yet-constructed area");
});

// Destroying a widget after moving it to a non-legacy area should work.
add_task(function() {
  CustomizableUI.createWidget({
    id: kWidget2Id,
    removable: true,
    defaultArea: CustomizableUI.AREA_NAVBAR
  });

  CustomizableUI.addWidgetToArea(kWidget2Id, kLazyAreaId);
  let noError = true;
  try {
    CustomizableUI.destroyWidget(kWidget2Id);
  } catch (ex) {
    Cu.reportError(ex);
    noError = false;
  }
  ok(noError, "Shouldn't throw an exception for a widget that was added to a not-yet-constructed area");
});

add_task(function asyncCleanup() {
  let lazyArea = document.getElementById(kLazyAreaId);
  if (lazyArea) {
    lazyArea.remove();
  }
  try {
    CustomizableUI.unregisterArea(kLazyAreaId);
  } catch (ex) {} // If we didn't register successfully for some reason
  yield resetCustomization();
});

function setupArea() {
  let lazyArea = document.createElementNS(kNSXUL, "hbox");
  lazyArea.id = kLazyAreaId;
  document.getElementById("nav-bar").appendChild(lazyArea);
  CustomizableUI.registerArea(kLazyAreaId, {
    type: CustomizableUI.TYPE_TOOLBAR,
    defaultPlacements: []
  });
}
