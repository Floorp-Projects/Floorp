/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const WIDGET_ID = "search-container";
const PREF_NAME = "browser.search.widget.inNavBar";

function checkDefaults() {
  ok(!Services.prefs.getBoolPref(PREF_NAME));
  is(CustomizableUI.getPlacementOfWidget(WIDGET_ID), null);
}

add_task(async function test_defaults() {
  // Verify the default state before the first test.
  checkDefaults();
});

add_task(async function test_syncPreferenceWithWidget() {
  // Moving the widget to any position in the navigation toolbar should turn the
  // preference to true.
  CustomizableUI.addWidgetToArea(WIDGET_ID, CustomizableUI.AREA_NAVBAR);
  ok(Services.prefs.getBoolPref(PREF_NAME));

  // Moving the widget to any position outside of the navigation toolbar should
  // turn the preference back to false.
  CustomizableUI.addWidgetToArea(
    WIDGET_ID,
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  ok(!Services.prefs.getBoolPref(PREF_NAME));
});

add_task(async function test_syncWidgetWithPreference() {
  // setting the preference should move the widget to the navigation toolbar and
  // place it right after the location bar.
  Services.prefs.setBoolPref(PREF_NAME, true);
  let placement = CustomizableUI.getPlacementOfWidget(WIDGET_ID);
  is(placement.area, CustomizableUI.AREA_NAVBAR);
  is(
    placement.position,
    CustomizableUI.getPlacementOfWidget("urlbar-container").position + 1
  );

  // This should move the widget back to the customization palette.
  Services.prefs.setBoolPref(PREF_NAME, false);
  checkDefaults();
});
