/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTestToolbarId = "test-empty-drag";

// Attempting to switch quickly from one tab to another to see whether the state changes
// correctly.
add_task(function CheckBasicCustomizeMode() {
  yield startCustomizing();
  ok(CustomizationHandler.isCustomizing(), "We should be in customize mode");
  yield endCustomizing();
  ok(!CustomizationHandler.isCustomizing(), "We should not be in customize mode");
});
add_task(function CheckQuickCustomizeModeSwitch() {
  let tab1 = gBrowser.addTab("about:newtab");
  gBrowser.selectedTab = tab1;
  let tab2 = gBrowser.addTab("about:customizing");
  let tab3 = gBrowser.addTab("about:newtab");
  gBrowser.selectedTab = tab2;
  try {
    yield waitForCondition(() => CustomizationHandler.isEnteringCustomizeMode);
  } catch (ex) {
    Cu.reportError(ex);
  }
  ok(CustomizationHandler.isEnteringCustomizeMode, "Should be entering customize mode");
  gBrowser.selectedTab = tab3;
  try {
    yield waitForCondition(() => !CustomizationHandler.isEnteringCustomizeMode && !CustomizationHandler.isCustomizing());
  } catch (ex) {
    Cu.reportError(ex);
  }
  ok(!CustomizationHandler.isCustomizing(), "Should not be entering customize mode");
  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab3);
});

add_task(function asyncCleanup() {
  yield endCustomizing();
});

