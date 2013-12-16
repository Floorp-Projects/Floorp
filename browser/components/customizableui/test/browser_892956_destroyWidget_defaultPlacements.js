/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kWidgetId = "test-892956-destroyWidget-defaultPlacement";

// destroyWidget should clean up defaultPlacements if the widget had a defaultArea
add_task(function() {
  ok(CustomizableUI.inDefaultState, "Should be in the default state when we start");

  let widgetSpec = {
    id: kWidgetId,
    defaultArea: CustomizableUI.AREA_NAVBAR
  };
  CustomizableUI.createWidget(widgetSpec);
  CustomizableUI.destroyWidget(kWidgetId);
  ok(CustomizableUI.inDefaultState, "Should be in the default state when we finish");
});

add_task(function asyncCleanup() {
  yield resetCustomization();
});
