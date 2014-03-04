/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function customizeToolbarAndKeepIt() {
  ok(gNavToolbox.toolbarset, "There should be a toolbarset");
  let toolbarID = "testAustralisCustomToolbar";
  gNavToolbox.appendCustomToolbar(toolbarID, "");
  let toolbarDOMID = getToolboxCustomToolbarId(toolbarID);
  let toolbarElement = document.getElementById(toolbarDOMID);
  ok(toolbarElement, "There should be a toolbar");
  if (!toolbarElement) {
    ok(false, "No toolbar created, bailing out of the test.");
    return;
  }
  is(toolbarElement.nextSibling, gNavToolbox.toolbarset,
     "Toolbar should have been inserted in toolbox, before toolbarset element");
  let cuiAreaType = CustomizableUI.getAreaType(toolbarDOMID);
  is(cuiAreaType, CustomizableUI.TYPE_TOOLBAR,
     "CustomizableUI should know the area and think it's a toolbar");
  if (cuiAreaType != CustomizableUI.TYPE_TOOLBAR) {
    ok(false, "Toolbar not registered successfully, bailing out of the test.");
    toolbarElement.remove();
    return;
  }
  ok(!CustomizableUI.getWidgetIdsInArea(toolbarDOMID).length, "There should be no widgets in the area yet.");
  CustomizableUI.addWidgetToArea("open-file-button", toolbarDOMID, 0);
  ok(toolbarElement.hasChildNodes(), "Toolbar should now have a button.");
  assertAreaPlacements(toolbarDOMID, ["open-file-button"]);

  yield startCustomizing();
  // First, exit customize mode without doing anything, and verify the toolbar doesn't get removed.
  yield endCustomizing();
  ok(!CustomizableUI.inDefaultState, "Shouldn't be in default state, the toolbar should still be there.");
  cuiAreaType = CustomizableUI.getAreaType(toolbarDOMID);
  is(cuiAreaType, CustomizableUI.TYPE_TOOLBAR,
     "CustomizableUI should still know the area and think it's a toolbar");
  ok(toolbarElement.parentNode, "Toolbar should still be in the DOM.");
  ok(toolbarElement.hasChildNodes(), "Toolbar should still have items in it.");
  assertAreaPlacements(toolbarDOMID, ["open-file-button"]);

  // Then customize again, and this time empty out the toolbar and verify it *does* get removed.
  yield startCustomizing();
  let openFileButton = document.getElementById("open-file-button");
  let palette = document.getElementById("customization-palette");
  simulateItemDrag(openFileButton, palette);
  ok(!CustomizableUI.inDefaultState, "Shouldn't be in default state because there's still a non-collapsed toolbar.");
  ok(!toolbarElement.hasChildNodes(), "Toolbar should have no more child nodes.");

  toolbarElement.collapsed = true;
  ok(CustomizableUI.inDefaultState, "Should be in default state because there's now just a collapsed toolbar.");
  toolbarElement.collapsed = false;
  ok(!CustomizableUI.inDefaultState, "Shouldn't be in default state because there's a non-collapsed toolbar again.");
  yield endCustomizing();
  ok(CustomizableUI.inDefaultState, "Should be in default state because the toolbar should have been removed.");
  ok(!toolbarElement.parentNode, "Toolbar should no longer be in the DOM.");
  cuiAreaType = CustomizableUI.getAreaType(toolbarDOMID);
  is(cuiAreaType, null, "CustomizableUI should have forgotten all about the area");
});

add_task(function resetShouldDealWithCustomToolbars() {
  ok(gNavToolbox.toolbarset, "There should be a toolbarset");
  let toolbarID = "testAustralisCustomToolbar";
  gNavToolbox.appendCustomToolbar(toolbarID, "");
  let toolbarDOMID = getToolboxCustomToolbarId(toolbarID);
  let toolbarElement = document.getElementById(toolbarDOMID);
  ok(toolbarElement, "There should be a toolbar");
  if (!toolbarElement) {
    ok(false, "No toolbar created, bailing out of the test.");
    return;
  }
  is(toolbarElement.nextSibling, gNavToolbox.toolbarset,
     "Toolbar should have been inserted in toolbox, before toolbarset element");
  let cuiAreaType = CustomizableUI.getAreaType(toolbarDOMID);
  is(cuiAreaType, CustomizableUI.TYPE_TOOLBAR,
     "CustomizableUI should know the area and think it's a toolbar");
  if (cuiAreaType != CustomizableUI.TYPE_TOOLBAR) {
    ok(false, "Toolbar not registered successfully, bailing out of the test.");
    toolbarElement.remove();
    return;
  }
  ok(!CustomizableUI.getWidgetIdsInArea(toolbarDOMID).length, "There should be no widgets in the area yet.");
  CustomizableUI.addWidgetToArea("sync-button", toolbarDOMID, 0);
  ok(toolbarElement.hasChildNodes(), "Toolbar should now have a button.");
  assertAreaPlacements(toolbarDOMID, ["sync-button"]);

  CustomizableUI.reset();

  ok(CustomizableUI.inDefaultState, "Should be in default state after reset.");
  let syncButton = document.getElementById("sync-button");
  ok(!syncButton, "Sync button shouldn't be in the document anymore.");
  ok(gNavToolbox.palette.querySelector("#sync-button"), "Sync button should be in the palette");
  ok(!toolbarElement.hasChildNodes(), "Toolbar should have no more child nodes.");
  ok(!toolbarElement.parentNode, "Toolbar should no longer be in the DOM.");
  cuiAreaType = CustomizableUI.getAreaType(toolbarDOMID);
  is(cuiAreaType, null, "CustomizableUI should have forgotten all about the area");
});

