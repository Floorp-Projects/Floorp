/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that widgets provided by extensions can be added to the
 * ADDONS area, but all other widgets cannot.
 */
add_task(async function test_only_extension_widgets_in_addons_area() {
  registerCleanupFunction(async () => {
    await CustomizableUI.reset();
  });

  Assert.ok(
    !CustomizableUI.canWidgetMoveToArea(
      "home-button",
      CustomizableUI.AREA_ADDONS
    ),
    "Cannot move a built-in button to the ADDONS area."
  );

  // Now double-check that we cannot accidentally default a non-extension
  // widget into the ADDONS area.
  const kTestDynamicWidget = "a-test-widget";
  CustomizableUI.createWidget({
    id: kTestDynamicWidget,
    label: "Test widget",
    defaultArea: CustomizableUI.AREA_ADDONS,
  });
  Assert.equal(
    CustomizableUI.getPlacementOfWidget(kTestDynamicWidget),
    null,
    "An attempt to put a non-extension widget into the ADDONS area by default should fail."
  );
  CustomizableUI.destroyWidget(kTestDynamicWidget);

  const kWebExtensionButtonID1 = "a-test-extension-button";

  CustomizableUI.createWidget({
    id: kWebExtensionButtonID1,
    label: "Test extension widget",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    webExtension: true,
  });

  Assert.ok(
    CustomizableUI.canWidgetMoveToArea(
      kWebExtensionButtonID1,
      CustomizableUI.AREA_ADDONS
    ),
    "Can move extension button to the addons area."
  );

  CustomizableUI.destroyWidget(kWebExtensionButtonID1);

  // Now check that extension buttons can default to the ADDONS area, if need
  // be.

  const kWebExtensionButtonID2 = "a-test-extension-button-2";

  CustomizableUI.createWidget({
    id: kWebExtensionButtonID2,
    label: "Test extension widget 2",
    defaultArea: CustomizableUI.AREA_ADDONS,
    webExtension: true,
  });

  Assert.equal(
    CustomizableUI.getPlacementOfWidget(kWebExtensionButtonID2)?.area,
    CustomizableUI.AREA_ADDONS,
    "An attempt to put an extension widget into the ADDONS area by default should work."
  );

  CustomizableUI.destroyWidget(kWebExtensionButtonID2);
});
