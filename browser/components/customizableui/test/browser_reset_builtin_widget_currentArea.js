/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that if we move a non-default, but builtin, widget to another area,
// and then reset things, the currentArea is updated correctly.
add_task(async function reset_should_not_keep_currentArea() {
  CustomizableUI.addWidgetToArea("save-page-button", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  // We can't check currentArea directly; check areaType which is based on it:
  is(CustomizableUI.getWidget("save-page-button").areaType,
     CustomizableUI.TYPE_MENU_PANEL,
     "Button should know it's in the overflow panel");
  CustomizableUI.reset();
  is(CustomizableUI.getWidget("save-page-button").areaType,
     null,
     "Button should know it's not in the overflow panel anymore");
});

// Check that if we move a default, builtin, widget to the palette,
// then reset things, then undo reset, that the currentArea is updated correctly.
add_task(async function reset_should_not_keep_currentArea() {
  const kButtonId = "sidebar-button";
  // We can't check currentArea directly; check areaType which is based on it:
  is(CustomizableUI.getWidget(kButtonId).areaType,
     CustomizableUI.TYPE_TOOLBAR,
     "Button should know it's in the toolbar");
  CustomizableUI.removeWidgetFromArea(kButtonId);
  is(CustomizableUI.getWidget(kButtonId).areaType,
     null,
     "Button should know it's no longer in the toolbar");
  CustomizableUI.reset();
  is(CustomizableUI.getWidget(kButtonId).areaType,
     CustomizableUI.TYPE_TOOLBAR,
     "Button should know it's in the toolbar again");
  CustomizableUI.undoReset();
  is(CustomizableUI.getWidget(kButtonId).areaType,
     null,
     "Button should know it's no longer in the toolbar again");
});

registerCleanupFunction(() => CustomizableUI.reset());
