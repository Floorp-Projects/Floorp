/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Should be able to add broken view widget
add_task(function testAddbrokenViewWidget() {
  const kWidgetId = 'test-877006-broken-widget';
  let widgetSpec = {
    id: kWidgetId,
    type: 'view',
    viewId: 'idontexist',
    /* Empty handler so we try to attach it maybe? */
    onViewShowing: function() {
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
  ok(noError, "Should not throw an exception trying to add a broken view widget.");

  noError = true;
  try {
    CustomizableUI.destroyWidget(kWidgetId);
  } catch (ex) {
    Cu.reportError(ex);
    noError = false;
  }
  ok(noError, "Should not throw an exception trying to remove the broken view widget.");
});

add_task(function* asyncCleanup() {
  yield resetCustomization();
});
