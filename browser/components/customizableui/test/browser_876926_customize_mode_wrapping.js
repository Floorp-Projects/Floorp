/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kXULWidgetId = "sync-button";
const kAPIWidgetId = "feed-button";
const kPanel = CustomizableUI.AREA_PANEL;
const kToolbar = CustomizableUI.AREA_NAVBAR;
const kVisiblePalette = "customization-palette";
const kPlaceholderClass = "panel-customization-placeholder";

function checkWrapper(id) {
  is(document.querySelectorAll("#wrapper-" + id).length, 1, "There should be exactly 1 wrapper for " + id + " in the customizing window.");
}

let move = {
  "drag": function(id, target) {
    let targetNode = document.getElementById(target);
    if (targetNode.customizationTarget) {
      targetNode = targetNode.customizationTarget;
    }
    simulateItemDrag(document.getElementById(id), targetNode);
  },
  "dragToItem": function(id, target) {
    let targetNode = document.getElementById(target);
    if (targetNode.customizationTarget) {
      targetNode = targetNode.customizationTarget;
    }
    let items = targetNode.querySelectorAll("toolbarpaletteitem:not(." + kPlaceholderClass + ")");
    if (target == kPanel) {
      targetNode = items[items.length - 1];
    } else {
      targetNode = items[0];
    }
    simulateItemDrag(document.getElementById(id), targetNode);
  },
  "API": function(id, target) {
    if (target == kVisiblePalette) {
      return CustomizableUI.removeWidgetFromArea(id);
    }
    return CustomizableUI.addWidgetToArea(id, target, null);
  }
};

function isLast(containerId, defaultPlacements, id) {
  assertAreaPlacements(containerId, defaultPlacements.concat([id]));
  is(document.getElementById(containerId).customizationTarget.lastChild.firstChild.id, id,
     "Widget " + id + " should be in " + containerId + " in customizing window.");
  is(otherWin.document.getElementById(containerId).customizationTarget.lastChild.id, id,
     "Widget " + id + " should be in " + containerId + " in other window.");
}

function isFirst(containerId, defaultPlacements, id) {
  assertAreaPlacements(containerId, [id].concat(defaultPlacements));
  is(document.getElementById(containerId).customizationTarget.firstChild.firstChild.id, id,
     "Widget " + id + " should be in " + containerId + " in customizing window.");
  is(otherWin.document.getElementById(containerId).customizationTarget.firstChild.id, id,
     "Widget " + id + " should be in " + containerId + " in other window.");
}

function checkToolbar(id, method) {
  // Place at start of the toolbar:
  let toolbarPlacements = getAreaWidgetIds(kToolbar);
  move[method](id, kToolbar);
  if (method == "dragToItem") {
    isFirst(kToolbar, toolbarPlacements, id);
  } else {
    isLast(kToolbar, toolbarPlacements, id);
  }
  checkWrapper(id);
}

function checkPanel(id, method) {
  let panelPlacements = getAreaWidgetIds(kPanel);
  move[method](id, kPanel);
  let children = document.getElementById(kPanel).querySelectorAll("toolbarpaletteitem:not(." + kPlaceholderClass + ")");
  let otherChildren = otherWin.document.getElementById(kPanel).children;
  let newPlacements = panelPlacements.concat([id]);
  // Relative position of the new item from the end:
  let position = -1;
  // For the drag to item case, we drag to the last item, making the dragged item the
  // penultimate item. We can't well use the first item because the panel has complicated
  // rules about rearranging wide items (which, by default, the first two items are).
  if (method == "dragToItem") {
    newPlacements.pop();
    newPlacements.splice(panelPlacements.length - 1, 0, id);
    position = -2;
  }
  assertAreaPlacements(kPanel, newPlacements);
  is(children[children.length + position].firstChild.id, id,
     "Widget " + id + " should be in " + kPanel + " in customizing window.");
  is(otherChildren[otherChildren.length + position].id, id,
     "Widget " + id + " should be in " + kPanel + " in other window.");
  checkWrapper(id);
}

function checkPalette(id, method) {
  // Move back to palette:
  move[method](id, kVisiblePalette);
  ok(CustomizableUI.inDefaultState, "Should end in default state");
  let visibleChildren = gCustomizeMode.visiblePalette.children;
  let expectedChild = method == "dragToItem" ? visibleChildren[0] : visibleChildren[visibleChildren.length - 1];
  is(expectedChild.firstChild.id, id, "Widget " + id + " was moved using " + method + " and should now be wrapped in palette in customizing window.");
  if (id == kXULWidgetId) {
    ok(otherWin.gNavToolbox.palette.querySelector("#" + id), "Widget " + id + " should be in invisible palette in other window.");
  }
  checkWrapper(id);
}

let otherWin;
Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);

// Moving widgets in two windows, one with customize mode and one without, should work.
add_task(function MoveWidgetsInTwoWindows() {
  yield startCustomizing();
  otherWin = yield openAndLoadWindow(null, true);
  yield otherWin.PanelUI.ensureReady();
  ok(CustomizableUI.inDefaultState, "Should start in default state");

  for (let widgetId of [kXULWidgetId, kAPIWidgetId]) {
    for (let method of ["API", "drag", "dragToItem"]) {
      info("Moving widget " + widgetId + " using " + method);
      checkToolbar(widgetId, method);
      checkPanel(widgetId, method);
      checkPalette(widgetId, method);
      checkPanel(widgetId, method);
      checkToolbar(widgetId, method);
      checkPalette(widgetId, method);
    }
  }
  otherWin.close();
  otherWin = null;
  if (otherWin) {
    otherWin.close();
  }
  yield endCustomizing();
});

add_task(function asyncCleanup() {
  Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck");
  yield resetCustomization();
});