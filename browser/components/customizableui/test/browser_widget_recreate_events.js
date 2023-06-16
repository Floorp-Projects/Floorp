"use strict";

const widgetData = {
  id: "test-widget",
  type: "view",
  viewId: "PanelUI-testbutton",
  label: "test widget label",
  onViewShowing() {},
  onViewHiding() {},
};

async function simulateWidgetOpen() {
  let testWidgetButton = document.getElementById("test-widget");
  let testWidgetShowing = BrowserTestUtils.waitForEvent(
    document,
    "popupshowing",
    true
  );
  testWidgetButton.click();
  await testWidgetShowing;
}

async function simulateWidgetClose() {
  let panel = document.getElementById("customizationui-widget-panel");
  let panelHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");

  panel.hidePopup();
  await panelHidden;
}

function createPanelView() {
  let panelView = document.createXULElement("panelview");
  panelView.id = "PanelUI-testbutton";
  let vbox = document.createXULElement("vbox");
  panelView.appendChild(vbox);
  return panelView;
}

/**
 * Check that panel view/hide events are added back,
 * if widget is destroyed and created again in one session.
 */
add_task(async function () {
  let viewCache = document.getElementById("appMenu-viewCache");
  let panelView = createPanelView();
  viewCache.appendChild(panelView);

  CustomizableUI.createWidget(widgetData);
  CustomizableUI.addWidgetToArea("test-widget", "nav-bar");

  // Simulate clicking and wait for the open
  // so we ensure the lazy event creation is done.
  await simulateWidgetOpen();

  let listeners = Services.els.getListenerInfoFor(panelView);
  ok(
    listeners.some(info => info.type == "ViewShowing"),
    "ViewShowing event added"
  );
  ok(
    listeners.some(info => info.type == "ViewHiding"),
    "ViewHiding event added"
  );

  await simulateWidgetClose();
  CustomizableUI.destroyWidget("test-widget");

  listeners = Services.els.getListenerInfoFor(panelView);
  // Ensure the events got removed after destorying the widget.
  ok(
    !listeners.some(info => info.type == "ViewShowing"),
    "ViewShowing event removed"
  );
  ok(
    !listeners.some(info => info.type == "ViewHiding"),
    "ViewHiding event removed"
  );

  CustomizableUI.createWidget(widgetData);
  // Simulate clicking and wait for the open
  // so we ensure the lazy event creation is done.
  // We need to do this again because we destroyed the widget.
  await simulateWidgetOpen();

  listeners = Services.els.getListenerInfoFor(panelView);
  ok(
    listeners.some(info => info.type == "ViewShowing"),
    "ViewShowing event added again"
  );
  ok(
    listeners.some(info => info.type == "ViewHiding"),
    "ViewHiding event added again"
  );

  await simulateWidgetClose();
  CustomizableUI.destroyWidget("test-widget");
  panelView.remove();
  CustomizableUI.reset();
});
