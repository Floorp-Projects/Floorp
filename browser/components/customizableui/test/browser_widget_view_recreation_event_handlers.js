/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const kWidgetId = "test-browser_widget_view_recreation_event_handlers";
const kWidgetViewId = kWidgetId + "-view";

// Should be able to add broken view widget
add_task(async function testAddOnBeforeCreatedWidget() {
  // Create and attach the view.
  const view = document.createXULElement("panelview");
  view.id = kWidgetViewId;
  document.getElementById("appMenu-viewCache").appendChild(view);

  let onViewShowing = false;

  function createAndClickWidget() {
    onViewShowing = false;

    // Create a "view" type widget.
    CustomizableUI.createWidget({
      id: kWidgetId,
      type: "view",
      viewId: kWidgetViewId,
      onViewShowing() {
        onViewShowing = true;
      },
    });
    CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_NAVBAR);

    // Click the widget.
    document.getElementById(kWidgetId).click();

    // Wait for it to be shown.
    return subviewShown(view);
  }

  async function hideAndDestroyWidget() {
    // Wait for it to be hidden.
    const panel = view.closest("panel");
    await gCUITestUtils.hidePanelMultiView(panel, () => PanelMultiView.hidePopup(panel));

    // Now destroy the widget.
    CustomizableUI.destroyWidget(kWidgetId);
  }

  await createAndClickWidget();
  ok(onViewShowing, "onViewShowing should have been called");
  await hideAndDestroyWidget();

  await createAndClickWidget();
  ok(onViewShowing, "onViewShowing should have been called after destroying " +
                    "the widget and recreating it");
  await hideAndDestroyWidget();
});


registerCleanupFunction(async function cleanup() {
  await resetCustomization();
  const view = document.getElementById(kWidgetViewId);
  view.remove();
});
