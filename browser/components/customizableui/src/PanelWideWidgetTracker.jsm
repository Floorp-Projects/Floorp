/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["PanelWideWidgetTracker"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");

let gModuleName = "[PanelWideWidgetTracker]";
#include logging.js

let gPanel = CustomizableUI.AREA_PANEL;
// We keep track of the widget placements for the panel locally:
let gPanelPlacements = [];

// All the wide widgets we know of:
let gWideWidgets = new Set();
// All the widgets we know of:
let gSeenWidgets = new Set();

// The class by which we recognize wide widgets:
const kWidePanelItemClass = "panel-combined-item";

// TODO(bug 885574): Merge this constant with the one in CustomizeMode.jsm,
//                   maybe just use a pref for this.
const kColumnsInMenuPanel = 3;

let PanelWideWidgetTracker = {
  // Listeners used to validate panel contents whenever they change:
  onWidgetAdded: function(aWidgetId, aArea, aPosition) {
    if (aArea == gPanel) {
      gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
      let moveForward = this.shouldMoveForward(aWidgetId, aPosition);
      this.adjustWidgets(aWidgetId, moveForward);
    }
  },
  onWidgetMoved: function(aWidgetId, aArea, aOldPosition, aNewPosition) {
    if (aArea == gPanel) {
      gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
      let moveForward = this.shouldMoveForward(aWidgetId, aNewPosition);
      this.adjustWidgets(aWidgetId, moveForward);
    }
  },
  onWidgetRemoved: function(aWidgetId, aPrevArea) {
    if (aPrevArea == gPanel) {
      gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
      let pos = gPanelPlacements.indexOf(aWidgetId);
      this.adjustWidgets(aWidgetId, false);
    }
  },
  onWidgetReset: function(aWidgetId) {
    gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
  },
  // Listener to keep abreast of any new nodes. We use the DOM one because
  // we need access to the actual node's classlist, so we can't use the ones above.
  // Furthermore, onWidgetCreated only fires for API-based widgets, not for XUL ones.
  onWidgetAfterDOMChange: function(aNode, aNextNode, aContainer) {
    if (!gSeenWidgets.has(aNode.id)) {
      if (aNode.classList.contains(kWidePanelItemClass)) {
        gWideWidgets.add(aNode.id);
      }
      gSeenWidgets.add(aNode.id);
    }
  },
  // When widgets get destroyed, we remove them from our sets of stuff we care about:
  onWidgetDestroyed: function(aWidgetId) {
    gSeenWidgets.delete(aWidgetId);
    gWideWidgets.delete(aWidgetId);
  },
  shouldMoveForward: function(aWidgetId, aPosition) {
    let currentWidgetAtPosition = gPanelPlacements[aPosition + 1];
    return gWideWidgets.has(currentWidgetAtPosition) && !gWideWidgets.has(aWidgetId);
  },
  adjustWidgets: function(aWidgetId, aMoveForwards) {
    if (this.adjusting) {
      return;
    }
    this.adjusting = true;
    let widgetsAffected = [w for (w of gPanelPlacements) if (gWideWidgets.has(w))];
    // If we're moving the wide widgets forwards (down/to the right in the panel)
    // we want to start with the last widgets. Otherwise we move widgets over other wide
    // widgets, which might mess up their order. Likewise, if moving backwards we should start with
    // the first widget and work our way down/right from there.
    let compareFn = aMoveForwards ? (function(a, b) a < b) : (function(a, b) a > b)
    widgetsAffected.sort(function(a, b) compareFn(gPanelPlacements.indexOf(a),
                                                  gPanelPlacements.indexOf(b)));
    for (let widget of widgetsAffected) {
      this.adjustPosition(widget, aMoveForwards);
    }
    this.adjusting = false;
  },
  // This function is called whenever an item gets moved in the menu panel. It
  // adjusts the position of widgets within the panel to prevent "gaps" between
  // wide widgets that could be filled up with single column widgets
  adjustPosition: function(aWidgetId, aMoveForwards) {
    // Make sure that there are n % columns = 0 narrow buttons before the widget.
    let placementIndex = gPanelPlacements.indexOf(aWidgetId);
    let prevSiblingCount = 0;
    let fixedPos = null;
    while (placementIndex--) {
      let thisWidgetId = gPanelPlacements[placementIndex];
      if (gWideWidgets.has(thisWidgetId)) {
        continue;
      }
      let widgetWrapper = CustomizableUI.getWidget(gPanelPlacements[placementIndex]);
      // This widget might not actually exist:
      if (!widgetWrapper) {
        continue;
      }
      // This widget might still not actually exist:
      if (widgetWrapper.provider == CustomizableUI.PROVIDER_XUL &&
          widgetWrapper.instances.length == 0) {
        continue;
      }

      // Or it might only be there some of the time:
      if (widgetWrapper.provider == CustomizableUI.PROVIDER_API &&
          widgetWrapper.showInPrivateBrowsing === false) {
        if (!fixedPos) {
          fixedPos = placementIndex;
        } else {
          fixedPos = Math.min(fixedPos, placementIndex);
        }
        // We want to position ourselves before this item:
        prevSiblingCount = 0;
      } else {
        prevSiblingCount++;
      }
    }

    if (fixedPos !== null || prevSiblingCount % kColumnsInMenuPanel) {
      let desiredPos = (fixedPos !== null) ? fixedPos : gPanelPlacements.indexOf(aWidgetId);
      let desiredChange = -(prevSiblingCount % kColumnsInMenuPanel);
      if (aMoveForwards && fixedPos == null) {
        // +1 because otherwise we'd count ourselves:
        desiredChange = kColumnsInMenuPanel + desiredChange + 1;
      }
      desiredPos += desiredChange;
      CustomizableUI.moveWidgetWithinArea(aWidgetId, desiredPos);
    }
  },
  init: function() {
    // Initialize our local placements copy and register the listener
    gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
    CustomizableUI.addListener(this);
  },
};
