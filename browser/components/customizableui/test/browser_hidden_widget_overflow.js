/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if only hidden widgets are overflowed that the
 * OverflowableToolbar won't show the overflow panel anchor.
 */

const kHiddenButtonID = "fake-hidden-button";
const kDisplayNoneButtonID = "display-none-button";
const kWebExtensionButtonID1 = "fake-webextension-button-1";
const kWebExtensionButtonID2 = "fake-webextension-button-2";
let gWin = null;

add_setup(async function () {
  gWin = await BrowserTestUtils.openNewBrowserWindow();

  // To make it easier to write a test where we can control overflowing
  // for a test that can run in a bunch of environments with slightly
  // different rules on when things will overflow, we'll go ahead and
  // just remove everything removable from the nav-bar by default. Then
  // we'll add our hidden item, and a single WebExtension item, and
  // force toolbar overflow.
  let widgetIDs = CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR);
  for (let widgetID of widgetIDs) {
    if (CustomizableUI.isWidgetRemovable(widgetID)) {
      CustomizableUI.removeWidgetFromArea(widgetID);
    }
  }

  CustomizableUI.createWidget({
    id: kWebExtensionButtonID1,
    label: "Test WebExtension widget 1",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    webExtension: true,
  });

  CustomizableUI.createWidget({
    id: kWebExtensionButtonID2,
    label: "Test WebExtension widget 2",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    webExtension: true,
  });

  // Let's force the WebExtension widgets to be significantly wider. This
  // just makes it easier to ensure that both of these (which are to the left
  // of the hidden widget) get overflowed.
  for (let webExtID of [kWebExtensionButtonID1, kWebExtensionButtonID2]) {
    let webExtNode = CustomizableUI.getWidget(webExtID).forWindow(gWin).node;
    webExtNode.style.minWidth = "100px";
  }

  CustomizableUI.createWidget({
    id: kHiddenButtonID,
    label: "Test hidden=true widget",
    defaultArea: CustomizableUI.AREA_NAVBAR,
  });

  // Now hide the button with hidden=true so that it has no dimensions.
  let hiddenButtonNode =
    CustomizableUI.getWidget(kHiddenButtonID).forWindow(gWin).node;
  hiddenButtonNode.hidden = true;

  CustomizableUI.createWidget({
    id: kDisplayNoneButtonID,
    label: "Test display:none widget",
    defaultArea: CustomizableUI.AREA_NAVBAR,
  });

  // Now hide the button with display: none so that it has no dimensions.
  let displayNoneButtonNode =
    CustomizableUI.getWidget(kDisplayNoneButtonID).forWindow(gWin).node;
  displayNoneButtonNode.style.display = "none";

  registerCleanupFunction(async () => {
    CustomizableUI.destroyWidget(kWebExtensionButtonID1);
    CustomizableUI.destroyWidget(kWebExtensionButtonID2);
    CustomizableUI.destroyWidget(kHiddenButtonID);
    CustomizableUI.destroyWidget(kDisplayNoneButtonID);
    await BrowserTestUtils.closeWindow(gWin);
    await CustomizableUI.reset();
  });
});

add_task(async function test_hidden_widget_overflow() {
  gWin.resizeTo(kForceOverflowWidthPx, window.outerHeight);

  // Wait until the left-most fake WebExtension button is overflowing.
  let webExtNode = CustomizableUI.getWidget(kWebExtensionButtonID1).forWindow(
    gWin
  ).node;
  await BrowserTestUtils.waitForMutationCondition(
    webExtNode,
    { attributes: true },
    () => {
      return webExtNode.hasAttribute("overflowedItem");
    }
  );

  let hiddenButtonNode =
    CustomizableUI.getWidget(kHiddenButtonID).forWindow(gWin).node;
  Assert.ok(
    hiddenButtonNode.hasAttribute("overflowedItem"),
    "Hidden button should be overflowed."
  );

  let overflowButton = gWin.document.getElementById("nav-bar-overflow-button");

  Assert.ok(
    !BrowserTestUtils.isVisible(overflowButton),
    "Overflow panel button should be hidden."
  );
});
