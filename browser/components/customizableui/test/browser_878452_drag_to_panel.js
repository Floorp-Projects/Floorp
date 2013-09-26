/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTests = [
  {
    desc: "Dragging an item from the palette to another button in the panel should work.",
    setup: startCustomizing,
    run: function() {
      let btn = document.getElementById("developer-button");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

      let lastButtonIndex = placements.length - 1;
      let lastButton = placements[lastButtonIndex];
      let placementsAfterInsert = placements.slice(0, lastButtonIndex).concat(["developer-button", lastButton]);
      let lastButtonNode = document.getElementById(lastButton);
      simulateItemDrag(btn, lastButtonNode);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterInsert);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let palette = document.getElementById("customization-palette");
      simulateItemDrag(btn, palette);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Dragging an item from the palette to the panel itself should also work.",
    setup: startCustomizing,
    run: function() {
      let btn = document.getElementById("developer-button");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

      let placementsAfterAppend = placements.concat(["developer-button"]);
      simulateItemDrag(btn, panel);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let palette = document.getElementById("customization-palette");
      simulateItemDrag(btn, palette);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Dragging an item from the palette to an empty panel should also work.",
    setup: function() {
      let widgetIds = getAreaWidgetIds(CustomizableUI.AREA_PANEL);
      while (widgetIds.length) {
        CustomizableUI.removeWidgetFromArea(widgetIds.shift());
      }
      return startCustomizing()
    },
    run: function() {
      let btn = document.getElementById("developer-button");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);

      assertAreaPlacements(panel.id, []);

      let placementsAfterAppend = ["developer-button"];
      simulateItemDrag(btn, panel);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let palette = document.getElementById("customization-palette");
      simulateItemDrag(btn, palette);
      assertAreaPlacements(panel.id, []);
    },
  }
];

function asyncCleanup() {
  yield endCustomizing();
  Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck");
  yield resetCustomization();
}

function test() {
  Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);
  waitForExplicitFinish();
  runTests(gTests, asyncCleanup);
}

