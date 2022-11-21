/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_no_addons_themes_widget() {
  if (!gUnifiedExtensions.isEnabled) {
    ok(true, "Skip task because Unified Extensions UI is disabled");
    return;
  }

  const addonsAndThemesWidgetId = "add-ons-button";

  // Add the button to the navbar, which should not do anything because the
  // add-ons and themes button should not exist when the unified extensions
  // pref is enabled.
  CustomizableUI.addWidgetToArea(
    addonsAndThemesWidgetId,
    CustomizableUI.AREA_NAVBAR
  );

  let addonsButton = document.getElementById(addonsAndThemesWidgetId);
  is(addonsButton, null, "expected no add-ons and themes button");
});
