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

let PanelWideWidgetTracker = {
  // Listeners used to validate panel contents whenever they change:
  onWidgetAdded: function(aWidgetId, aArea, aPosition) {
    if (aArea == gPanel) {
      let moveForward = this.shouldMoveForward(aWidgetId, aPosition);
      this.adjustWidgets(aWidgetId, aPosition, moveForward);
    }
  },
  onWidgetMoved: function(aWidgetId, aArea, aOldPosition, aNewPosition) {
    if (aArea == gPanel) {
      let moveForward = this.shouldMoveForward(aWidgetId, aNewPosition);
      this.adjustWidgets(aWidgetId, Math.min(aOldPosition, aNewPosition), moveForward);
    }
  },
  onWidgetRemoved: function(aWidgetId, aPrevArea) {
    if (aPrevArea == gPanel) {
      let pos = gPanelPlacements.indexOf(aWidgetId);
      this.adjustWidgets(aWidgetId, pos);
    }
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
    gSeenWidgets.remove(aWidgetId);
    gWideWidgets.remove(aWidgetId);
  },
  shouldMoveForward: function(aWidgetId, aPosition) {
    let currentWidgetAtPosition = gPanelPlacements[aPosition];
    return gWideWidgets.has(currentWidgetAtPosition) && !gWideWidgets.has(aWidgetId);
  },
  adjustWidgets: function(aWidgetId, aPosition, aMoveForwards) {
    if (this.adjustmentStack == 0) {
      this.movingForward = aMoveForwards;
    }
    gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
    // First, make a list of all the widgets that are *after* the insertion/moving point.
    let widgetsAffected = [];
    for (let widget of gWideWidgets) {
      let wideWidgetPos = gPanelPlacements.indexOf(widget);
      // This would just be wideWidgetPos >= aPosition, except that if we start re-arranging
      // widgets, we would re-enter here because obviously the wide widget ends up in its
      // own position, and we'd never stop trying to rearrange things.
      // So instead, we check the wide widget is after the insertion point *or*
      // this is the first move, and the widget is exactly on the insertion point
      if (wideWidgetPos > aPosition || (!this.adjustmentStack && wideWidgetPos == aPosition)) {
        widgetsAffected.push(widget);
      }
    }
    if (!widgetsAffected.length) {
      return;
    }
    widgetsAffected.sort(function(a, b) gPanelPlacements.indexOf(a) < gPanelPlacements.indexOf(b));
    this.adjustmentStack++;
    this.adjustPosition(widgetsAffected[0]);
    this.adjustmentStack--;
    if (this.adjustmentStack == 0) {
      delete this.movingForward;
    }
  },
  // This function is called whenever an item gets moved in the menu panel. It
  // adjusts the position of widgets within the panel to reduce single-column
  // buttons from being placed in a row by themselves.
  adjustPosition: function(aWidgetId) {
    // TODO(bug 885574): Merge this constant with the one in CustomizeMode.jsm,
    //                   maybe just use a pref for this.
    const kColumnsInMenuPanel = 3;

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
      if (this.movingForward) {
        // Add 1 because we're moving forward, and we would otherwise count the widget itself.
        desiredPos += (kColumnsInMenuPanel - (prevSiblingCount % kColumnsInMenuPanel)) + 1;
      } else {
        desiredPos -= prevSiblingCount % kColumnsInMenuPanel;
      }
      // We don't need to move all of the items in this pass, because
      // this move will trigger adjustPosition to get called again. The
      // function will stop recursing when it finds that there is no
      // more work that is needed.
      CustomizableUI.moveWidgetWithinArea(aWidgetId, desiredPos);
    }
  },
  adjustmentStack: 0,
  init: function() {
    // Initialize our local placements copy and register the listener
    gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
    CustomizableUI.addListener(this);
  },
};
