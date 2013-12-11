/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTests = [
  {
    desc: "One orphaned item should have two placeholders next to it.",
    setup: startCustomizing,
    run: function() {
      let btn = document.getElementById("developer-button");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

      if (!isInWin8()) {
        placements = placements.concat(["developer-button"]);
        simulateItemDrag(btn, panel);
        ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      } else {
        ok(CustomizableUI.inDefaultState, "Should be in default state.");
      }

      assertAreaPlacements(CustomizableUI.AREA_PANEL, placements);
      is(getVisiblePlaceholderCount(panel), 2, "Should only have 2 visible placeholders before exiting");

      yield endCustomizing();
      yield startCustomizing();
      is(getVisiblePlaceholderCount(panel), 2, "Should only have 2 visible placeholders after re-entering");

      if (!isInWin8()) {
        let palette = document.getElementById("customization-palette");
        simulateItemDrag(btn, palette);
      }
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Two orphaned items should have one placeholder next to them (case 1).",
    setup: startCustomizing,
    run: function() {
      let btn = document.getElementById("developer-button");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

      let placementsAfterAppend = placements.concat(["developer-button"]);
      simulateItemDrag(btn, panel);

      if (!isInWin8()) {
        placementsAfterAppend = placementsAfterAppend.concat(["sync-button"]);
        btn = document.getElementById("sync-button");
        simulateItemDrag(btn, panel);
      }

      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholders before exiting");

      yield endCustomizing();
      yield startCustomizing();
      is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholders after re-entering");

      let palette = document.getElementById("customization-palette");
      simulateItemDrag(btn, palette);

      if (!isInWin8()) {
        btn = document.getElementById("developer-button");
        simulateItemDrag(btn, palette);
      }
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Two orphaned items should have one placeholder next to them (case 2).",
    setup: startCustomizing,
    run: function() {
      let btn = document.getElementById("add-ons-button");
      let btn2 = document.getElementById("switch-to-metro-button");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let palette = document.getElementById("customization-palette");
      let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

      let placementsAfterAppend = placements.filter(p => p != btn.id);
      simulateItemDrag(btn, palette);

      if (isInWin8()) {
        placementsAfterAppend = placementsAfterAppend.filter(p => p != btn2.id);
        simulateItemDrag(btn2, palette);
      }

      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholders before exiting");

      yield endCustomizing();
      yield startCustomizing();
      is(getVisiblePlaceholderCount(panel), 1, "Should only have 1 visible placeholders after re-entering");

      simulateItemDrag(btn, panel);

      if (isInWin8()) {
        simulateItemDrag(btn2, panel);
      }

      assertAreaPlacements(CustomizableUI.AREA_PANEL, placements);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "A wide widget at the bottom of the panel should have three placeholders after it.",
    setup: startCustomizing,
    run: function() {
      let btn = document.getElementById("edit-controls");
      let metroBtn = document.getElementById("switch-to-metro-button");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let palette = document.getElementById("customization-palette");
      let placements = getAreaWidgetIds(CustomizableUI.AREA_PANEL);

      if (isInWin8()) {
        // Remove switch-to-metro-button
        placements.pop();
        simulateItemDrag(metroBtn, palette);
      }

      let placementsAfterAppend = placements.concat([placements.shift()]);
      simulateItemDrag(btn, panel);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterAppend);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      is(getVisiblePlaceholderCount(panel), 3, "Should have 3 visible placeholders before exiting");

      yield endCustomizing();
      yield startCustomizing();
      is(getVisiblePlaceholderCount(panel), 3, "Should have 3 visible placeholders after re-entering");

      if (isInWin8()) {
        simulateItemDrag(metroBtn, panel);
      }
      let zoomControls = document.getElementById("zoom-controls");
      simulateItemDrag(btn, zoomControls);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "The default placements should have three placeholders at the bottom (or 2 in win8).",
    setup: startCustomizing,
    run: function() {
      let numPlaceholders = isInWin8() ? 2 : 3;
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      ok(CustomizableUI.inDefaultState, "Should be in default state.");
      is(getVisiblePlaceholderCount(panel), numPlaceholders, "Should have " + numPlaceholders + " visible placeholders before exiting");

      yield endCustomizing();
      yield startCustomizing();
      is(getVisiblePlaceholderCount(panel), numPlaceholders, "Should have " + numPlaceholders + " visible placeholders after re-entering");

      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
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

function getVisiblePlaceholderCount(aPanel) {
  let visiblePlaceholders = aPanel.querySelectorAll(".panel-customization-placeholder:not([hidden=true])");
  return visiblePlaceholders.length;
}
