/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function checkSpacers() {
  let bsPass = ChromeUtils.import(
    "resource:///modules/CustomizableUI.jsm",
    null
  );
  let navbarWidgets = CustomizableUI.getWidgetIdsInArea("nav-bar");
  let currentSetWidgets = bsPass.CustomizableUIInternal._getCurrentWidgetsInContainer(
    document.getElementById("nav-bar")
  );
  navbarWidgets = navbarWidgets.filter(w => CustomizableUI.isSpecialWidget(w));
  currentSetWidgets = currentSetWidgets.filter(w =>
    CustomizableUI.isSpecialWidget(w)
  );
  Assert.deepEqual(
    navbarWidgets,
    currentSetWidgets,
    "Should have the same 'special' widgets in currentset and placements"
  );
}

/**
 * Check that after a reset, CUI's internal bookkeeping correctly deals with flexible spacers.
 */
add_task(async function() {
  await startCustomizing();
  checkSpacers();

  CustomizableUI.addWidgetToArea(
    "spring",
    "nav-bar",
    4 /* Insert before the last extant spacer */
  );
  await gCustomizeMode.reset();
  checkSpacers();
  await endCustomizing();
});
