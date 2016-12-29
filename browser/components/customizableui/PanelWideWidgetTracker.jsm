/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["PanelWideWidgetTracker"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");

var gPanel = CustomizableUI.AREA_PANEL;
// We keep track of the widget placements for the panel locally:
var gPanelPlacements = [];

// All the wide widgets we know of:
var gWideWidgets = new Set();
// All the widgets we know of:
var gSeenWidgets = new Set();

var PanelWideWidgetTracker = {
  // Listeners used to validate panel contents whenever they change:
  onWidgetAdded(aWidgetId, aArea, aPosition) {
    if (aArea == gPanel) {
      gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
      let moveForward = this.shouldMoveForward(aWidgetId, aPosition);
      this.adjustWidgets(aWidgetId, moveForward);
    }
  },
  onWidgetMoved(aWidgetId, aArea, aOldPosition, aNewPosition) {
    if (aArea == gPanel) {
      gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
      let moveForward = this.shouldMoveForward(aWidgetId, aNewPosition);
      this.adjustWidgets(aWidgetId, moveForward);
    }
  },
  onWidgetRemoved(aWidgetId, aPrevArea) {
    if (aPrevArea == gPanel) {
      gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
      this.adjustWidgets(aWidgetId, false);
    }
  },
  onWidgetReset(aWidgetId) {
    gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
  },
  // Listener to keep abreast of any new nodes. We use the DOM one because
  // we need access to the actual node's classlist, so we can't use the ones above.
  // Furthermore, onWidgetCreated only fires for API-based widgets, not for XUL ones.
  onWidgetAfterDOMChange(aNode, aNextNode, aContainer) {
    if (!gSeenWidgets.has(aNode.id)) {
      if (aNode.classList.contains(CustomizableUI.WIDE_PANEL_CLASS)) {
        gWideWidgets.add(aNode.id);
      }
      gSeenWidgets.add(aNode.id);
    }
  },
  // When widgets get destroyed, we remove them from our sets of stuff we care about:
  onWidgetDestroyed(aWidgetId) {
    gSeenWidgets.delete(aWidgetId);
    gWideWidgets.delete(aWidgetId);
  },
  shouldMoveForward(aWidgetId, aPosition) {
    let currentWidgetAtPosition = gPanelPlacements[aPosition + 1];
    let rv = gWideWidgets.has(currentWidgetAtPosition) && !gWideWidgets.has(aWidgetId);
    // We might now think we can move forward, but for that we need at least 2 more small
    // widgets to be present:
    if (rv) {
      let furtherWidgets = gPanelPlacements.slice(aPosition + 2);
      let realWidgets = 0;
      if (furtherWidgets.length >= 2) {
        while (furtherWidgets.length && realWidgets < 2) {
          let w = furtherWidgets.shift();
          if (!gWideWidgets.has(w) && this.checkWidgetStatus(w)) {
            realWidgets++;
          }
        }
      }
      if (realWidgets < 2) {
        rv = false;
      }
    }
    return rv;
  },
  adjustWidgets(aWidgetId, aMoveForwards) {
    if (this.adjusting) {
      return;
    }
    this.adjusting = true;
    let widgetsAffected = gPanelPlacements.filter((w) => gWideWidgets.has(w));
    // If we're moving the wide widgets forwards (down/to the right in the panel)
    // we want to start with the last widgets. Otherwise we move widgets over other wide
    // widgets, which might mess up their order. Likewise, if moving backwards we should start with
    // the first widget and work our way down/right from there.
    let compareFn = aMoveForwards ? ((a, b) => a < b) : ((a, b) => a > b);
    widgetsAffected.sort((a, b) => compareFn(gPanelPlacements.indexOf(a),
                                             gPanelPlacements.indexOf(b)));
    for (let widget of widgetsAffected) {
      this.adjustPosition(widget, aMoveForwards);
    }
    this.adjusting = false;
  },
  // This function is called whenever an item gets moved in the menu panel. It
  // adjusts the position of widgets within the panel to prevent "gaps" between
  // wide widgets that could be filled up with single column widgets
  adjustPosition(aWidgetId, aMoveForwards) {
    // Make sure that there are n % columns = 0 narrow buttons before the widget.
    let placementIndex = gPanelPlacements.indexOf(aWidgetId);
    let prevSiblingCount = 0;
    let fixedPos = null;
    while (placementIndex--) {
      let thisWidgetId = gPanelPlacements[placementIndex];
      if (gWideWidgets.has(thisWidgetId)) {
        continue;
      }
      let widgetStatus = this.checkWidgetStatus(thisWidgetId);
      if (!widgetStatus) {
        continue;
      }
      if (widgetStatus == "public-only") {
        fixedPos = !fixedPos ? placementIndex : Math.min(fixedPos, placementIndex);
        prevSiblingCount = 0;
      } else {
        prevSiblingCount++;
      }
    }

    if (fixedPos !== null || prevSiblingCount % CustomizableUI.PANEL_COLUMN_COUNT) {
      let desiredPos = (fixedPos !== null) ? fixedPos : gPanelPlacements.indexOf(aWidgetId);
      let desiredChange = -(prevSiblingCount % CustomizableUI.PANEL_COLUMN_COUNT);
      if (aMoveForwards && fixedPos == null) {
        // +1 because otherwise we'd count ourselves:
        desiredChange = CustomizableUI.PANEL_COLUMN_COUNT + desiredChange + 1;
      }
      desiredPos += desiredChange;
      CustomizableUI.moveWidgetWithinArea(aWidgetId, desiredPos);
    }
  },

  /*
   * Check whether a widget id is actually known anywhere.
   * @returns false if the widget doesn't exist,
   *          "public-only" if it's not shown in private windows
   *          "real" if it does exist and is shown even in private windows
   */
  checkWidgetStatus(aWidgetId) {
    let widgetWrapper = CustomizableUI.getWidget(aWidgetId);
    // This widget might not actually exist:
    if (!widgetWrapper) {
      return false;
    }
    // This widget might still not actually exist:
    if (widgetWrapper.provider == CustomizableUI.PROVIDER_XUL &&
        widgetWrapper.instances.length == 0) {
      return false;
    }

    // Or it might only be there some of the time:
    if (widgetWrapper.provider == CustomizableUI.PROVIDER_API &&
        widgetWrapper.showInPrivateBrowsing === false) {
      return "public-only";
    }
    return "real";
  },

  init() {
    // Initialize our local placements copy and register the listener
    gPanelPlacements = CustomizableUI.getWidgetIdsInArea(gPanel);
    CustomizableUI.addListener(this);
  },
};
