/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_addons_themes_widget() {
  const addonsAndThemesWidgetId = "add-ons-button";

  ok(
    !Services.prefs.getBoolPref("extensions.unifiedExtensions.enabled", false),
    "expected unified extensions pref to be disabled"
  );

  CustomizableUI.addWidgetToArea(
    addonsAndThemesWidgetId,
    CustomizableUI.AREA_NAVBAR
  );

  let addonsButton = document.getElementById(addonsAndThemesWidgetId);
  ok(addonsButton, "expected add-ons and themes button");
});
