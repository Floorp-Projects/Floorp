/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let kWidgetId = "test-removable-widget-default";
const kNavBar = CustomizableUI.AREA_NAVBAR;
let widgetCounter = 0;
let gTests = [
  {
    desc: "Sanity checks",
    run: function() {
      let brokenSpec = {id: kWidgetId + (widgetCounter++), removable: false};
      SimpleTest.doesThrow(function() CustomizableUI.createWidget(brokenSpec),
                           "Creating non-removable widget without defaultArea should throw.");

      // Widget without removable set should be removable:
      let wrapper = CustomizableUI.createWidget({id: kWidgetId + (widgetCounter++)});
      ok(CustomizableUI.isWidgetRemovable(wrapper.id), "Should be removable by default.");
      CustomizableUI.destroyWidget(wrapper.id);
    }
  },
  {
    desc: "Test non-removable widget with defaultArea",
    run: function() {
      // Non-removable widget with defaultArea should work:
      let spec = {id: kWidgetId + (widgetCounter++), removable: false,
                  defaultArea: kNavBar};
      let widgetWrapper;
      try {
        widgetWrapper = CustomizableUI.createWidget(spec);
      } catch (ex) {
        ok(false, "Creating a non-removable widget with a default area should not throw.");
        return;
      }

      let placement = CustomizableUI.getPlacementOfWidget(spec.id);
      ok(placement, "Widget should be placed.");
      is(placement.area, kNavBar, "Widget should be in navbar");
      let singleWrapper = widgetWrapper.forWindow(window);
      ok(singleWrapper, "Widget should exist in window.");
      ok(singleWrapper.node, "Widget node should exist in window.");
      let expectedParent = CustomizableUI.getCustomizeTargetForArea(kNavBar, window);
      is(singleWrapper.node.parentNode, expectedParent, "Widget should be in navbar.");

      let otherWin = yield openAndLoadWindow(true);
      placement = CustomizableUI.getPlacementOfWidget(spec.id);
      ok(placement, "Widget should be placed.");
      is(placement && placement.area, kNavBar, "Widget should be in navbar");

      singleWrapper = widgetWrapper.forWindow(otherWin);
      ok(singleWrapper, "Widget should exist in other window.");
      if (singleWrapper) {
        ok(singleWrapper.node, "Widget node should exist in other window.");
        if (singleWrapper.node) {
          let expectedParent = CustomizableUI.getCustomizeTargetForArea(kNavBar, otherWin);
          is(singleWrapper.node.parentNode, expectedParent,
             "Widget should be in navbar in other window.");
        }
      }
      otherWin.close();
    }
  },
];

function asyncCleanup() {
  yield resetCustomization();
}

function cleanup() {
  removeCustomToolbars();
}

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanup);
  runTests(gTests, asyncCleanup);
}


