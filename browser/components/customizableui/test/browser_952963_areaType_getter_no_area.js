/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kToolbarName = "test-unregisterArea-areaType";
const kUnregisterAreaTestWidget = "test-widget-for-unregisterArea-areaType";
const kTestWidget = "test-widget-no-area-areaType";
registerCleanupFunction(removeCustomToolbars);

function checkAreaType(widget) {
  try {
    is(widget.areaType, null, "areaType should be null");
  } catch (ex) {
    info("Fetching areaType threw: " + ex);
    ok(false, "areaType getter shouldn't throw.");
  }
}

// widget wrappers in unregisterArea'd areas and nowhere shouldn't throw when checking areaTypes.
add_task(function() {
  // Using the ID before it's been created will imply a XUL wrapper; we'll test
  // an API-based wrapper below
  let toolbarNode = createToolbarWithPlacements(kToolbarName, [kUnregisterAreaTestWidget]);
  CustomizableUI.unregisterArea(kToolbarName);
  toolbarNode.remove();

  let w = CustomizableUI.getWidget(kUnregisterAreaTestWidget);
  checkAreaType(w);

  w = CustomizableUI.getWidget(kTestWidget);
  checkAreaType(w);

  let spec = {id: kUnregisterAreaTestWidget, type: 'button', removable: true,
              label: "areaType test", tooltiptext: "areaType test"};
  CustomizableUI.createWidget(spec);
  let toolbarNode = createToolbarWithPlacements(kToolbarName, [kUnregisterAreaTestWidget]);
  CustomizableUI.unregisterArea(kToolbarName);
  toolbarNode.remove();
  w = CustomizableUI.getWidget(spec.id);
  checkAreaType(w);
  CustomizableUI.removeWidgetFromArea(kUnregisterAreaTestWidget);
  checkAreaType(w);
  //XXXgijs: ensure cleanup function doesn't barf:
  gAddedToolbars.delete(kToolbarName);
});

add_task(function asyncCleanup() {
  yield resetCustomization();
});

