/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";
const kWidgetId = "test-981418-widget-onbeforecreated";

// Should be able to add broken view widget
add_task(async function testAddOnBeforeCreatedWidget() {
  let viewShownDeferred = Promise.defer();
  let onBeforeCreatedCalled = false;
  let widgetSpec = {
    id: kWidgetId,
    type: "view",
    viewId: kWidgetId + "idontexistyet",
    onBeforeCreated(doc) {
      let view = doc.createElement("panelview");
      view.id = kWidgetId + "idontexistyet";
      document.getElementById("PanelUI-multiView").appendChild(view);
      onBeforeCreatedCalled = true;
    },
    onViewShowing() {
      viewShownDeferred.resolve();
    }
  };

  let noError = true;
  try {
    CustomizableUI.createWidget(widgetSpec);
    CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_NAVBAR);
  } catch (ex) {
    Cu.reportError(ex);
    noError = false;
  }
  ok(noError, "Should not throw an exception trying to add the widget.");
  ok(onBeforeCreatedCalled, "onBeforeCreated should have been called");

  let widgetNode = document.getElementById(kWidgetId);
  ok(widgetNode, "Widget should exist");
  if (widgetNode) {
    try {
      widgetNode.click();

      let tempPanel = document.getElementById("customizationui-widget-panel");
      let panelShownPromise = promisePanelElementShown(window, tempPanel);

      let shownTimeout = setTimeout(() => viewShownDeferred.reject("Panel not shown within 20s"), 20000);
      await viewShownDeferred.promise;
      await panelShownPromise;
      clearTimeout(shownTimeout);
      ok(true, "Found view shown");

      let panelHiddenPromise = promisePanelElementHidden(window, tempPanel);
      tempPanel.hidePopup();
      await panelHiddenPromise;

      CustomizableUI.addWidgetToArea(kWidgetId, CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
      await waitForOverflowButtonShown();
      await document.getElementById("nav-bar").overflowable.show();

      viewShownDeferred = Promise.defer();
      widgetNode.click();

      shownTimeout = setTimeout(() => viewShownDeferred.reject("Panel not shown within 20s"), 20000);
      await viewShownDeferred.promise;
      clearTimeout(shownTimeout);
      ok(true, "Found view shown");

      let panelHidden = promiseOverflowHidden(window);
      PanelUI.overflowPanel.hidePopup();
      await panelHidden;
    } catch (ex) {
      ok(false, "Unexpected exception (like a timeout for one of the yields) " +
                "when testing view widget.");
    }
  }

  noError = true;
  try {
    CustomizableUI.destroyWidget(kWidgetId);
  } catch (ex) {
    Cu.reportError(ex);
    noError = false;
  }
  ok(noError, "Should not throw an exception trying to remove the broken view widget.");
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
