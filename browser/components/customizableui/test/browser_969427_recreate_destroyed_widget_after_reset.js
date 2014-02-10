/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getPlacementArea(id) {
  let placement = CustomizableUI.getPlacementOfWidget(id);
  return placement && placement.area;
}

// Check that a destroyed widget recreated after a reset call goes to
// the navigation bar.
add_task(function() {
  const kWidgetId = "test-recreate-after-reset";
  let spec = {id: kWidgetId, label: "Test re-create after reset.",
              removable: true, defaultArea: CustomizableUI.AREA_NAVBAR};

  CustomizableUI.createWidget(spec);
  is(getPlacementArea(kWidgetId), CustomizableUI.AREA_NAVBAR,
     "widget is in the navigation bar");

  CustomizableUI.destroyWidget(kWidgetId);
  isnot(getPlacementArea(kWidgetId), CustomizableUI.AREA_NAVBAR,
        "widget removed from the navigation bar");

  CustomizableUI.reset();

  CustomizableUI.createWidget(spec);
  is(getPlacementArea(kWidgetId), CustomizableUI.AREA_NAVBAR,
     "widget recreated and added back to the nav bar");

  CustomizableUI.destroyWidget(kWidgetId);
});
