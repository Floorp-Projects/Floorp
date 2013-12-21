/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kWidgetId = "test-destroy-non-removable-defaultArea";

add_task(function() {
  let spec = {id: kWidgetId, label: "Test non-removable defaultArea re-adding.",
              removable: false, defaultArea: CustomizableUI.AREA_NAVBAR};
  CustomizableUI.createWidget(spec);
  let placement = CustomizableUI.getPlacementOfWidget(kWidgetId);
  ok(placement, "Should have placed the widget.");
  is(placement && placement.area, CustomizableUI.AREA_NAVBAR, "Widget should be in navbar");
  CustomizableUI.destroyWidget(kWidgetId);
  CustomizableUI.removeWidgetFromArea(kWidgetId);

  CustomizableUI.createWidget(spec);
  ok(placement, "Should have placed the widget.");
  is(placement && placement.area, CustomizableUI.AREA_NAVBAR, "Widget should be in navbar");
  CustomizableUI.destroyWidget(kWidgetId);
  CustomizableUI.removeWidgetFromArea(kWidgetId);

  const kPrefCustomizationAutoAdd = "browser.uiCustomization.autoAdd";
  Services.prefs.setBoolPref(kPrefCustomizationAutoAdd, false);
  CustomizableUI.createWidget(spec);
  ok(placement, "Should have placed the widget.");
  is(placement && placement.area, CustomizableUI.AREA_NAVBAR, "Widget should be in navbar");
  CustomizableUI.destroyWidget(kWidgetId);
  CustomizableUI.removeWidgetFromArea(kWidgetId);
  Services.prefs.clearUserPref(kPrefCustomizationAutoAdd);
});

