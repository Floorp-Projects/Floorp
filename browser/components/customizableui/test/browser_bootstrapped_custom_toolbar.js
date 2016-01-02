/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

const kTestBarID = "testBar";
const kWidgetID = "characterencoding-button";

function createTestBar(aLegacy) {
  let testBar = document.createElement("toolbar");
  testBar.id = kTestBarID;
  testBar.setAttribute("customizable", "true");
  CustomizableUI.registerArea(kTestBarID, {
    type: CustomizableUI.TYPE_TOOLBAR,
    legacy: aLegacy,
  });
  gNavToolbox.appendChild(testBar);
  return testBar;
}

/**
 * Helper function that does the following:
 *
 * 1) Creates a custom toolbar and registers it
 *    with CustomizableUI. Sets the legacy attribute
 *    of the object passed to registerArea to aLegacy.
 * 2) Adds the widget with ID aWidgetID to that new
 *    toolbar.
 * 3) Enters customize mode and makes sure that the
 *    widget is still in the right toolbar.
 * 4) Exits customize mode, then removes and deregisters
 *    the custom toolbar.
 * 5) Checks that the widget has no placement.
 * 6) Re-adds and re-registers a custom toolbar with the same
 *    ID and options as the first one.
 * 7) Enters customize mode and checks that the widget is
 *    properly back in the toolbar.
 * 8) Exits customize mode, removes and de-registers the
 *    toolbar, and resets the toolbars to default.
 */
function checkRestoredPresence(aWidgetID, aLegacy) {
  return Task.spawn(function* () {
    let testBar = createTestBar(aLegacy);
    CustomizableUI.addWidgetToArea(aWidgetID, kTestBarID);
    let placement = CustomizableUI.getPlacementOfWidget(aWidgetID);
    is(placement.area, kTestBarID,
       "Expected " + aWidgetID + " to be in the test toolbar");

    CustomizableUI.unregisterArea(testBar.id);
    testBar.remove();

    placement = CustomizableUI.getPlacementOfWidget(aWidgetID);
    is(placement, null, "Expected " + aWidgetID + " to be in the palette");

    testBar = createTestBar(aLegacy);

    yield startCustomizing();
    placement = CustomizableUI.getPlacementOfWidget(aWidgetID);
    is(placement.area, kTestBarID,
       "Expected " + aWidgetID + " to be in the test toolbar");
    yield endCustomizing();

    CustomizableUI.unregisterArea(testBar.id);
    testBar.remove();

    yield resetCustomization();
  });
}

add_task(function* () {
  yield checkRestoredPresence("downloads-button", false);
  yield checkRestoredPresence("downloads-button", true);
});

add_task(function* () {
  yield checkRestoredPresence("characterencoding-button", false);
  yield checkRestoredPresence("characterencoding-button", true);
});
