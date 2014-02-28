/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kWidgetId = "some-widget";

function assertWidgetExists(aWindow, aExists) {
  if (aExists) {
    ok(aWindow.document.getElementById(kWidgetId),
       "Should have found test widget in the window");
  } else {
    is(aWindow.document.getElementById(kWidgetId), null,
       "Should not have found test widget in the window");
  }
}

// A widget that is created with showInPrivateBrowsing undefined should
// have that value default to true.
add_task(function() {
  let wrapper = CustomizableUI.createWidget({
    id: kWidgetId
  });
  ok(wrapper.showInPrivateBrowsing,
     "showInPrivateBrowsing should have defaulted to true.");
  CustomizableUI.destroyWidget(kWidgetId);
});

// Add a widget via the API with showInPrivateBrowsing set to false
// and ensure it does not appear in pre-existing or newly created
// private windows.
add_task(function() {
  let plain1 = yield openAndLoadWindow();
  let private1 = yield openAndLoadWindow({private: true});
  CustomizableUI.createWidget({
    id: kWidgetId,
    removable: true,
    showInPrivateBrowsing: false
  });
  CustomizableUI.addWidgetToArea(kWidgetId,
                                 CustomizableUI.AREA_NAVBAR);
  assertWidgetExists(plain1, true);
  assertWidgetExists(private1, false);

  // Now open up some new windows. The widget should exist in the new
  // plain window, but not the new private window.
  let plain2 = yield openAndLoadWindow();
  let private2 = yield openAndLoadWindow({private: true});
  assertWidgetExists(plain2, true);
  assertWidgetExists(private2, false);

  // Try moving the widget around and make sure it doesn't get added
  // to the private windows. We'll start by appending it to the tabstrip.
  CustomizableUI.addWidgetToArea(kWidgetId,
                                 CustomizableUI.AREA_TABSTRIP);
  assertWidgetExists(plain1, true);
  assertWidgetExists(plain2, true);
  assertWidgetExists(private1, false);
  assertWidgetExists(private2, false);

  // And then move it to the beginning of the tabstrip.
  CustomizableUI.moveWidgetWithinArea(kWidgetId, 0);
  assertWidgetExists(plain1, true);
  assertWidgetExists(plain2, true);
  assertWidgetExists(private1, false);
  assertWidgetExists(private2, false);

  CustomizableUI.removeWidgetFromArea("some-widget");
  assertWidgetExists(plain1, false);
  assertWidgetExists(plain2, false);
  assertWidgetExists(private1, false);
  assertWidgetExists(private2, false);

  yield Promise.all([plain1, plain2, private1, private2].map(promiseWindowClosed));

  CustomizableUI.destroyWidget("some-widget");
});

// Add a widget via the API with showInPrivateBrowsing set to true,
// and ensure that it appears in pre-existing or newly created
// private browsing windows.
add_task(function() {
  let plain1 = yield openAndLoadWindow();
  let private1 = yield openAndLoadWindow({private: true});

  CustomizableUI.createWidget({
    id: kWidgetId,
    removable: true,
    showInPrivateBrowsing: true
  });
  CustomizableUI.addWidgetToArea(kWidgetId,
                                 CustomizableUI.AREA_NAVBAR);
  assertWidgetExists(plain1, true);
  assertWidgetExists(private1, true);

  // Now open up some new windows. The widget should exist in the new
  // plain window, but not the new private window.
  let plain2 = yield openAndLoadWindow();
  let private2 = yield openAndLoadWindow({private: true});

  assertWidgetExists(plain2, true);
  assertWidgetExists(private2, true);

  // Try moving the widget around and make sure it doesn't get added
  // to the private windows. We'll start by appending it to the tabstrip.
  CustomizableUI.addWidgetToArea(kWidgetId,
                                 CustomizableUI.AREA_TABSTRIP);
  assertWidgetExists(plain1, true);
  assertWidgetExists(plain2, true);
  assertWidgetExists(private1, true);
  assertWidgetExists(private2, true);

  // And then move it to the beginning of the tabstrip.
  CustomizableUI.moveWidgetWithinArea(kWidgetId, 0);
  assertWidgetExists(plain1, true);
  assertWidgetExists(plain2, true);
  assertWidgetExists(private1, true);
  assertWidgetExists(private2, true);

  CustomizableUI.removeWidgetFromArea("some-widget");
  assertWidgetExists(plain1, false);
  assertWidgetExists(plain2, false);
  assertWidgetExists(private1, false);
  assertWidgetExists(private2, false);

  yield Promise.all([plain1, plain2, private1, private2].map(promiseWindowClosed));

  CustomizableUI.destroyWidget("some-widget");
});

add_task(function asyncCleanup() {
  yield resetCustomization();
});
