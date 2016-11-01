/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TOOLBARID = "test-toolbar-added-during-customize-mode";

// The ID of a button that is not placed (ie, is in the palette) by default
const kNonPlacedWidgetId = "open-file-button";

add_task(function*() {
  yield startCustomizing();
  let toolbar = createToolbarWithPlacements(TOOLBARID, []);
  CustomizableUI.addWidgetToArea(kNonPlacedWidgetId, TOOLBARID);
  let button = document.getElementById(kNonPlacedWidgetId);
  ok(button, "Button should exist.");
  is(button.parentNode.localName, "toolbarpaletteitem", "Button's parent node should be a wrapper.");

  simulateItemDrag(button, gNavToolbox.palette);
  ok(!CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId), "Button moved to the palette");
  ok(gNavToolbox.palette.querySelector(`#${kNonPlacedWidgetId}`), "Button really is in palette.");

  button.scrollIntoView();
  simulateItemDrag(button, toolbar);
  ok(CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId), "Button moved out of palette");
  is(CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId).area, TOOLBARID, "Button's back on toolbar");
  ok(toolbar.querySelector(`#${kNonPlacedWidgetId}`), "Button really is on toolbar.");

  yield endCustomizing();
  isnot(button.parentNode.localName, "toolbarpaletteitem", "Button's parent node should not be a wrapper outside customize mode.");
  yield startCustomizing();

  is(button.parentNode.localName, "toolbarpaletteitem", "Button's parent node should be a wrapper back in customize mode.");

  simulateItemDrag(button, gNavToolbox.palette);
  ok(!CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId), "Button moved to the palette");
  ok(gNavToolbox.palette.querySelector(`#${kNonPlacedWidgetId}`), "Button really is in palette.");

  ok(!CustomizableUI.inDefaultState, "Not in default state while toolbar is not collapsed yet.");
  setToolbarVisibility(toolbar, false);
  ok(CustomizableUI.inDefaultState, "In default state while toolbar is collapsed.");

  setToolbarVisibility(toolbar, true);

  info("Check that removing the area registration from within customize mode works");
  CustomizableUI.unregisterArea(TOOLBARID);
  ok(CustomizableUI.inDefaultState, "Now that the toolbar is no longer registered, should be in default state.");
  ok(!gCustomizeMode.areas.has(toolbar), "Toolbar shouldn't be known to customize mode.");

  CustomizableUI.registerArea(TOOLBARID, {legacy: true, defaultPlacements: []});
  CustomizableUI.registerToolbarNode(toolbar, []);
  ok(!CustomizableUI.inDefaultState, "Now that the toolbar is registered again, should no longer be in default state.");
  ok(gCustomizeMode.areas.has(toolbar), "Toolbar should be known to customize mode again.");

  button.scrollIntoView();
  simulateItemDrag(button, toolbar);
  ok(CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId), "Button moved out of palette");
  is(CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId).area, TOOLBARID, "Button's back on toolbar");
  ok(toolbar.querySelector(`#${kNonPlacedWidgetId}`), "Button really is on toolbar.");

  let otherWin = yield openAndLoadWindow({}, true);
  let otherTB = otherWin.document.createElementNS(kNSXUL, "toolbar");
  otherTB.id = TOOLBARID;
  otherTB.setAttribute("customizable", "true");
  let wasInformedCorrectlyOfAreaAppearing = false;
  let listener = {
    onAreaNodeRegistered: function(aArea, aNode) {
      if (aNode == otherTB) {
        wasInformedCorrectlyOfAreaAppearing = true;
      }
    }
  };
  CustomizableUI.addListener(listener);
  otherWin.gNavToolbox.appendChild(otherTB);
  ok(wasInformedCorrectlyOfAreaAppearing, "Should have been told area was registered.");
  CustomizableUI.removeListener(listener);

  ok(otherTB.querySelector(`#${kNonPlacedWidgetId}`), "Button is on other toolbar, too.");

  simulateItemDrag(button, gNavToolbox.palette);
  ok(!CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId), "Button moved to the palette");
  ok(gNavToolbox.palette.querySelector(`#${kNonPlacedWidgetId}`), "Button really is in palette.");
  ok(!otherTB.querySelector(`#${kNonPlacedWidgetId}`), "Button is in palette in other window, too.");

  button.scrollIntoView();
  simulateItemDrag(button, toolbar);
  ok(CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId), "Button moved out of palette");
  is(CustomizableUI.getPlacementOfWidget(kNonPlacedWidgetId).area, TOOLBARID, "Button's back on toolbar");
  ok(toolbar.querySelector(`#${kNonPlacedWidgetId}`), "Button really is on toolbar.");
  ok(otherTB.querySelector(`#${kNonPlacedWidgetId}`), "Button is on other toolbar, too.");

  let wasInformedCorrectlyOfAreaDisappearing = false;
  // XXXgijs So we could be using promiseWindowClosed here. However, after
  // repeated random oranges, I'm instead relying on onWindowClosed below to
  // fire appropriately - it is linked to an unload event as well, and so
  // reusing it prevents a potential race between unload handlers where the
  // one from promiseWindowClosed could fire before the onWindowClosed
  // (and therefore onAreaNodeRegistered) one, causing the test to fail.
  let windowCloseDeferred = Promise.defer();
  listener = {
    onAreaNodeUnregistered: function(aArea, aNode, aReason) {
      if (aArea == TOOLBARID) {
        is(aNode, otherTB, "Should be informed about other toolbar");
        is(aReason, CustomizableUI.REASON_WINDOW_CLOSED, "Reason should be correct.");
        wasInformedCorrectlyOfAreaDisappearing = (aReason === CustomizableUI.REASON_WINDOW_CLOSED);
      }
    },
    onWindowClosed: function(aWindow) {
      if (aWindow == otherWin) {
        windowCloseDeferred.resolve(aWindow);
      } else {
        info("Other window was closed!");
        info("Other window title: " + (aWindow.document && aWindow.document.title));
        info("Our window title: " + (otherWin.document && otherWin.document.title));
      }
    },
  };
  CustomizableUI.addListener(listener);
  otherWin.close();
  let windowClosed = yield windowCloseDeferred.promise;

  is(windowClosed, otherWin, "Window should have sent onWindowClosed notification.");
  ok(wasInformedCorrectlyOfAreaDisappearing, "Should be told about window closing.");
  // Closing the other window should not be counted against this window's customize mode:
  is(button.parentNode.localName, "toolbarpaletteitem", "Button's parent node should still be a wrapper.");
  ok(gCustomizeMode.areas.has(toolbar), "Toolbar should still be a customizable area for this customize mode instance.");

  yield gCustomizeMode.reset();

  yield endCustomizing();

  CustomizableUI.removeListener(listener);
  wasInformedCorrectlyOfAreaDisappearing = false;
  listener = {
    onAreaNodeUnregistered: function(aArea, aNode, aReason) {
      if (aArea == TOOLBARID) {
        is(aNode, toolbar, "Should be informed about this window's toolbar");
        is(aReason, CustomizableUI.REASON_AREA_UNREGISTERED, "Reason for final removal should be correct.");
        wasInformedCorrectlyOfAreaDisappearing = (aReason === CustomizableUI.REASON_AREA_UNREGISTERED);
      }
    },
  }
  CustomizableUI.addListener(listener);
  removeCustomToolbars();
  ok(wasInformedCorrectlyOfAreaDisappearing, "Should be told about area being unregistered.");
  CustomizableUI.removeListener(listener);
  ok(CustomizableUI.inDefaultState, "Should be fine after exiting customize mode.");
});
