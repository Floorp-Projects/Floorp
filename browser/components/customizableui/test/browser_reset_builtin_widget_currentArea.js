/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that if we move a non-default, but builtin, widget to another area,
// and then reset things, the currentArea is updated correctly.
add_task(async function reset_should_not_keep_currentArea() {
  CustomizableUI.addWidgetToArea(
    "save-page-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  // We can't check currentArea directly; check areaType which is based on it:
  is(
    CustomizableUI.getWidget("save-page-button").areaType,
    CustomizableUI.TYPE_MENU_PANEL,
    "Button should know it's in the overflow panel"
  );
  CustomizableUI.reset();
  ok(
    !CustomizableUI.getWidget("save-page-button").areaType,
    "Button should know it's not in the overflow panel anymore"
  );
});

registerCleanupFunction(() => CustomizableUI.reset());
