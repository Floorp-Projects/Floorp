/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTests = [
  {
    desc: "Dragging the zoom controls to be before the print button " +
          "should not move any controls.",
    setup: startCustomizing,
    run: function() {
      let zoomControls = document.getElementById("zoom-controls");
      let printButton = document.getElementById("print-button");
      let placementsAfterMove = ["edit-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "zoom-controls",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(zoomControls, printButton);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let newWindowButton = document.getElementById("new-window-button");
      simulateItemDrag(zoomControls, newWindowButton);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Dragging the zoom controls to be before the save button " +
          "should not move any controls.",
    setup: startCustomizing,
    run: function() {
      let zoomControls = document.getElementById("zoom-controls");
      let savePageButton = document.getElementById("save-page-button");
      let placementsAfterMove = ["edit-controls",
                                 "zoom-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(zoomControls, savePageButton);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      ok(CustomizableUI.inDefaultState, "Should be in default state.");
    },
  },
  {
    desc: "Dragging the zoom controls to be before the new-window " +
          "button should not move any widgets.",
    setup: startCustomizing,
    run: function() {
      let zoomControls = document.getElementById("zoom-controls");
      let newWindowButton = document.getElementById("new-window-button");
      let placementsAfterMove = ["edit-controls",
                                 "zoom-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(zoomControls, newWindowButton);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging the zoom controls to be before the history-panelmenu " +
          "should move the zoom-controls in to the row higher than the " +
          "history-panelmenu.",
    setup: startCustomizing,
    run: function() {
      let zoomControls = document.getElementById("zoom-controls");
      let historyPanelMenu = document.getElementById("history-panelmenu");
      let placementsAfterMove = ["edit-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "zoom-controls",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(zoomControls, historyPanelMenu);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let newWindowButton = document.getElementById("new-window-button");
      simulateItemDrag(zoomControls, newWindowButton);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Dragging the zoom controls to be before the preferences-button " +
          "should move the zoom-controls in to the row higher than the " +
          "preferences-button.",
    setup: startCustomizing,
    run: function() {
      let zoomControls = document.getElementById("zoom-controls");
      let preferencesButton = document.getElementById("preferences-button");
      let placementsAfterMove = ["edit-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "zoom-controls",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(zoomControls, preferencesButton);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let newWindowButton = document.getElementById("new-window-button");
      simulateItemDrag(zoomControls, newWindowButton);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Dragging an item from the palette to before the zoom-controls " +
          "should move it and two other buttons before the zoom controls.",
    setup: startCustomizing,
    run: function() {
      let developerButton = document.getElementById("developer-button");
      let zoomControls = document.getElementById("zoom-controls");
      let placementsAfterInsert = ["edit-controls",
                                   "developer-button",
                                   "new-window-button",
                                   "privatebrowsing-button",
                                   "zoom-controls",
                                   "save-page-button",
                                   "print-button",
                                   "history-panelmenu",
                                   "fullscreen-button",
                                   "find-button",
                                   "preferences-button",
                                   "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterInsert);
      simulateItemDrag(developerButton, zoomControls);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterInsert);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let palette = document.getElementById("customization-palette");
      // Check that the palette items are re-wrapped correctly.
      let feedWrapper = document.getElementById("wrapper-feed-button");
      let feedButton = document.getElementById("feed-button");
      is(feedButton.parentNode, feedWrapper,
         "feed-button should be a child of wrapper-feed-button");
      is(feedWrapper.getAttribute("place"), "palette",
         "The feed-button wrapper should have it's place set to 'palette'");
      simulateItemDrag(developerButton, palette);
      is(developerButton.parentNode.tagName, "toolbarpaletteitem",
         "The developer-button should be wrapped by a toolbarpaletteitem");
      let newWindowButton = document.getElementById("new-window-button");
      simulateItemDrag(zoomControls, newWindowButton);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Dragging an item from the palette to before the edit-controls " +
          "should move it and two other buttons before the edit and zoom controls.",
    setup: startCustomizing,
    run: function() {
      let developerButton = document.getElementById("developer-button");
      let editControls = document.getElementById("edit-controls");
      let placementsAfterInsert = ["developer-button",
                                   "new-window-button",
                                   "privatebrowsing-button",
                                   "edit-controls",
                                   "zoom-controls",
                                   "save-page-button",
                                   "print-button",
                                   "history-panelmenu",
                                   "fullscreen-button",
                                   "find-button",
                                   "preferences-button",
                                   "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterInsert);
      simulateItemDrag(developerButton, editControls);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterInsert);
      ok(!CustomizableUI.inDefaultState, "Should no longer be in default state.");
      let palette = document.getElementById("customization-palette");
      // Check that the palette items are re-wrapped correctly.
      let feedWrapper = document.getElementById("wrapper-feed-button");
      let feedButton = document.getElementById("feed-button");
      is(feedButton.parentNode, feedWrapper,
         "feed-button should be a child of wrapper-feed-button");
      is(feedWrapper.getAttribute("place"), "palette",
         "The feed-button wrapper should have it's place set to 'palette'");
      simulateItemDrag(developerButton, palette);
      is(developerButton.parentNode.tagName, "toolbarpaletteitem",
         "The developer-button should be wrapped by a toolbarpaletteitem");
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
    },
  },
  {
    desc: "Dragging the edit-controls to be before the zoom-controls button " +
          "should not move any widgets.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let zoomControls = document.getElementById("zoom-controls");
      let placementsAfterMove = ["edit-controls",
                                 "zoom-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(editControls, zoomControls);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging the edit-controls to be before the new-window-button should " +
          "move the zoom-controls before the edit-controls.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let newWindowButton = document.getElementById("new-window-button");
      let placementsAfterMove = ["zoom-controls",
                                 "edit-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(editControls, newWindowButton);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      let zoomControls = document.getElementById("zoom-controls");
      simulateItemDrag(editControls, zoomControls);
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging the edit-controls to be before the privatebrowsing-button " +
          "should move the edit-controls in to the row higher than the " +
          "privatebrowsing-button.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let privateBrowsingButton = document.getElementById("privatebrowsing-button");
      let placementsAfterMove = ["zoom-controls",
                                 "edit-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(editControls, privateBrowsingButton);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      let zoomControls = document.getElementById("zoom-controls");
      simulateItemDrag(editControls, zoomControls);
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging the edit-controls to be before the save-page-button " +
          "should move the edit-controls in to the row higher than the " +
          "save-page-button.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let savePageButton = document.getElementById("save-page-button");
      let placementsAfterMove = ["zoom-controls",
                                 "edit-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(editControls, savePageButton);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      let zoomControls = document.getElementById("zoom-controls");
      simulateItemDrag(editControls, zoomControls);
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging the edit-controls to the panel itself should append " +
          "the edit controls to the bottom of the panel.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let placementsAfterMove = ["zoom-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button",
                                 "edit-controls"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(editControls, panel);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      let zoomControls = document.getElementById("zoom-controls");
      simulateItemDrag(editControls, zoomControls);
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging the edit-controls to the customization-palette and " +
          "back should work.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let palette = document.getElementById("customization-palette");
      let placementsAfterMove = ["zoom-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      let paletteChildElementCount = palette.childElementCount;
      simulateItemDrag(editControls, palette);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      is(paletteChildElementCount + 1, palette.childElementCount,
         "The palette should have a new child, congratulations!");
      is(editControls.parentNode.id, "wrapper-edit-controls",
         "The edit-controls should be properly wrapped.");
      is(editControls.parentNode.getAttribute("place"), "palette",
         "The edit-controls should have the place of 'palette'.");
      let zoomControls = document.getElementById("zoom-controls");
      simulateItemDrag(editControls, zoomControls);
      is(paletteChildElementCount, palette.childElementCount,
         "The palette child count should have returned to its prior value.");
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging the edit-controls to each of the panel placeholders " +
          "should append the edit-controls to the bottom of the panel.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let numPlaceholders = isInWin8() ? 2 : 3;
      for (let i = 0; i < numPlaceholders; i++) {
        // NB: We can't just iterate over all of the placeholders
        // because each drag-drop action recreates them.
        let placeholder = panel.getElementsByClassName("panel-customization-placeholder")[i];
        let placementsAfterMove = ["zoom-controls",
                                   "new-window-button",
                                   "privatebrowsing-button",
                                   "save-page-button",
                                   "print-button",
                                   "history-panelmenu",
                                   "fullscreen-button",
                                   "find-button",
                                   "preferences-button",
                                   "add-ons-button",
                                   "edit-controls"];
        addSwitchToMetroButtonInWindows8(placementsAfterMove);
        simulateItemDrag(editControls, placeholder);
        assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
        let zoomControls = document.getElementById("zoom-controls");
        simulateItemDrag(editControls, zoomControls);
        ok(CustomizableUI.inDefaultState, "Should still be in default state.");
      }
    },
  },
  {
    desc: "Dragging the developer-button back on to itself should work.",
    setup: startCustomizing,
    run: function() {
      let developerButton = document.getElementById("developer-button");
      is(developerButton.parentNode.tagName, "toolbarpaletteitem",
         "developer-button should be wrapped by a toolbarpaletteitem");
      simulateItemDrag(developerButton, developerButton);
      is(developerButton.parentNode.tagName, "toolbarpaletteitem",
         "developer-button should be wrapped by a toolbarpaletteitem");
      let editControls = document.getElementById("edit-controls");
      is(editControls.parentNode.tagName, "toolbarpaletteitem",
         "edit-controls should be wrapped by a toolbarpaletteitem");
      ok(CustomizableUI.inDefaultState, "Should still be in default state.");
    },
  },
  {
    desc: "Dragging a small button onto the last big button should work.",
    setup: startCustomizing,
    run: function() {
      let editControls = document.getElementById("edit-controls");
      let panel = document.getElementById(CustomizableUI.AREA_PANEL);
      let target = panel.getElementsByClassName("panel-customization-placeholder")[0];
      let placementsAfterMove = ["zoom-controls",
                                 "new-window-button",
                                 "privatebrowsing-button",
                                 "save-page-button",
                                 "print-button",
                                 "history-panelmenu",
                                 "fullscreen-button",
                                 "find-button",
                                 "preferences-button",
                                 "add-ons-button",
                                 "edit-controls"];
      addSwitchToMetroButtonInWindows8(placementsAfterMove);
      simulateItemDrag(editControls, target);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);
      let itemToDrag = "sync-button";
      let button = document.getElementById(itemToDrag);
      if (!isInWin8()) {
        placementsAfterMove.push(itemToDrag);
      } else {
        placementsAfterMove.splice(11, 0, itemToDrag);
      }
      simulateItemDrag(button, editControls);
      assertAreaPlacements(CustomizableUI.AREA_PANEL, placementsAfterMove);

      // Put stuff back:
      let palette = document.getElementById("customization-palette");
      let zoomControls = document.getElementById("zoom-controls");
      simulateItemDrag(button, palette);
      simulateItemDrag(editControls, zoomControls);
      ok(CustomizableUI.inDefaultState, "Should be in default state again.");
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
  requestLongerTimeout(5);
  runTests(gTests, asyncCleanup);
}

