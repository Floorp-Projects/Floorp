/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";
const kWidgetId = "test-981418-widget-onbeforecreated";

// Should be able to add broken view widget
add_task(async function testAddOnBeforeCreatedWidget() {
  let onBeforeCreatedCalled = false;
  let widgetSpec = {
    id: kWidgetId,
    type: "view",
    viewId: kWidgetId + "idontexistyet",
    onBeforeCreated(doc) {
      let view = doc.createElement("panelview");
      view.id = kWidgetId + "idontexistyet";
      document.getElementById("appMenu-viewCache").appendChild(view);
      onBeforeCreatedCalled = true;
    }
  };

  CustomizableUI.createWidget(widgetSpec);
  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_NAVBAR);

  ok(onBeforeCreatedCalled, "onBeforeCreated should have been called");

  let widgetNode = document.getElementById(kWidgetId);
  let viewNode = document.getElementById(kWidgetId + "idontexistyet");
  ok(widgetNode, "Widget should exist");
  ok(viewNode, "Panelview should exist");
  widgetNode.click();

  let tempPanel = document.getElementById("customizationui-widget-panel");
  let panelShownPromise = promisePanelElementShown(window, tempPanel);

  await Promise.all([
    BrowserTestUtils.waitForEvent(viewNode, "ViewShown"),
    panelShownPromise
  ]);

  let panelHiddenPromise = promisePanelElementHidden(window, tempPanel);
  tempPanel.hidePopup();
  await panelHiddenPromise;

  CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  await waitForOverflowButtonShown();
  await document.getElementById("nav-bar").overflowable.show();

  widgetNode.click();

  await BrowserTestUtils.waitForEvent(viewNode, "ViewShown");

  let panelHidden = promiseOverflowHidden(window);
  PanelUI.overflowPanel.hidePopup();
  await panelHidden;

  CustomizableUI.destroyWidget(kWidgetId);
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
