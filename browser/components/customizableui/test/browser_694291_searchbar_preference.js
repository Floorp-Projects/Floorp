/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const WIDGET_ID = "search-container";
const PREF_NAME = "browser.search.widget.inNavBar";

function checkDefaults() {
  // If the following defaults change, then the DEFAULT_AREA_PLACEMENTS of
  // UITelemetry.jsm, the navbarPlacements of CustomizableUI.jsm, and the
  // position and attributes of the search-container element in browser.xul
  // should also change at the same time.
  ok(Services.prefs.getBoolPref(PREF_NAME));
  let placement = CustomizableUI.getPlacementOfWidget(WIDGET_ID);
  is(placement.area, CustomizableUI.AREA_NAVBAR);
  is(placement.position,
     CustomizableUI.getPlacementOfWidget("urlbar-container").position + 1);
}

add_task(async function test_defaults() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});

  // Verify the default state before the first test.
  checkDefaults();
});

add_task(async function test_syncPreferenceWithWidget() {
  // Moving the widget to any position outside of the navigation toolbar should
  // turn the preference to false.
  CustomizableUI.addWidgetToArea(WIDGET_ID, CustomizableUI.AREA_PANEL);
  ok(!Services.prefs.getBoolPref(PREF_NAME));

  // Moving the widget back to any position in the navigation toolbar should
  // turn the preference to true again.
  CustomizableUI.addWidgetToArea(WIDGET_ID, CustomizableUI.AREA_NAVBAR);
  ok(Services.prefs.getBoolPref(PREF_NAME));
});

add_task(async function test_syncWidgetWithPreference() {
  // This should move the widget the customization palette.
  Services.prefs.setBoolPref(PREF_NAME, false);
  is(CustomizableUI.getPlacementOfWidget(WIDGET_ID), null);

  // This should return the widget to its default placement.
  Services.prefs.setBoolPref(PREF_NAME, true);
  checkDefaults();
});
