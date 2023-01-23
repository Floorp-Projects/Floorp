/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if Unified Extensions UI is enabled that resetting the toolbars
 * puts all browser action buttons into the AREA_ADDONS area.
 */
add_task(async function test_reset_with_unified_extensions_ui() {
  const kWebExtensionWidgetIDs = [
    "ext0-browser-action",
    "ext1-browser-action",
    "ext2-browser-action",
    "ext3-browser-action",
    "ext4-browser-action",
    "ext5-browser-action",
    "ext6-browser-action",
    "ext7-browser-action",
    "ext8-browser-action",
    "ext9-browser-action",
    "ext10-browser-action",
  ];

  for (let widgetID of kWebExtensionWidgetIDs) {
    CustomizableUI.createWidget({
      id: widgetID,
      label: "Test extension widget",
      defaultArea: CustomizableUI.AREA_NAVBAR,
      webExtension: true,
    });
  }

  // Now let's put these browser actions in a bunch of different places.
  // Regardless of where they go, we're going to expect them in AREA_ADDONS
  // after we reset.
  CustomizableUI.addWidgetToArea(
    kWebExtensionWidgetIDs[0],
    CustomizableUI.AREA_TABSTRIP
  );
  CustomizableUI.addWidgetToArea(
    kWebExtensionWidgetIDs[1],
    CustomizableUI.AREA_TABSTRIP
  );

  // macOS doesn't have AREA_MENUBAR registered, so we'll leave these widgets
  // behind in the AREA_NAVBAR there, and put them into the menubar on the
  // other platforms.
  if (AppConstants.platform != "macosx") {
    CustomizableUI.addWidgetToArea(
      kWebExtensionWidgetIDs[2],
      CustomizableUI.AREA_MENUBAR
    );
    CustomizableUI.addWidgetToArea(
      kWebExtensionWidgetIDs[3],
      CustomizableUI.AREA_MENUBAR
    );
  }

  CustomizableUI.addWidgetToArea(
    kWebExtensionWidgetIDs[4],
    CustomizableUI.AREA_BOOKMARKS
  );
  CustomizableUI.addWidgetToArea(
    kWebExtensionWidgetIDs[5],
    CustomizableUI.AREA_BOOKMARKS
  );
  CustomizableUI.addWidgetToArea(
    kWebExtensionWidgetIDs[6],
    CustomizableUI.AREA_ADDONS
  );
  CustomizableUI.addWidgetToArea(
    kWebExtensionWidgetIDs[7],
    CustomizableUI.AREA_ADDONS
  );

  CustomizableUI.reset();

  // Let's force the Unified Extensions panel to register itself now if it
  // wasn't already done. Using the getter should be sufficient.
  Assert.ok(gUnifiedExtensions.panel, "Should have found the panel.");

  for (let widgetID of kWebExtensionWidgetIDs) {
    let { area } = CustomizableUI.getPlacementOfWidget(widgetID);
    Assert.equal(area, CustomizableUI.AREA_ADDONS);
    // Let's double-check that they're actually in there in the DOM too.
    let widget = CustomizableUI.getWidget(widgetID).forWindow(window);
    Assert.equal(widget.node.parentElement.id, CustomizableUI.AREA_ADDONS);
    CustomizableUI.destroyWidget(widgetID);
  }
});
